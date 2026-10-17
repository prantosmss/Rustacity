//! EffectPhaser's allpass filter.
//!
//! Faithful port of the original C++ implementation
//! (`src/effects/PhaserFilter.h`, the signal processing that used to sit in
//! `src/effects/Phaser.cpp`): a cascade of allpass stages swept by an LFO,
//! with feedback and a dry/wet mix.  Every operation is mirrored in the same
//! order and precision, including the `f32` sample rate the LFO step is
//! derived from and the final narrowing of each sample to `f32`.
//!
//! Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

use std::f64::consts::PI;

/// The most stages a phaser can have (`NUM_STAGES` in Phaser.h); a block that
/// asks for more is clamped to it.
pub const MAX_STAGES: i32 = 24;

/// How many samples pass before the LFO value is recomputed (`lfoskipsamples`).
const LFO_SKIP_SAMPLES: i64 = 20;

/// The shape of the LFO (`phaserlfoshape` in the C++).
const LFO_SHAPE: f64 = 4.0;

/// dB to linear amplitude - `DB_TO_LINEAR` in Rustacity.h.
fn db_to_linear(db: f64) -> f64 {
    10.0_f64.powf(db / 20.0)
}

/// The effect's settings for one block: the number of allpass stages, the
/// dry/wet mix (0..255, 255 is fully wet), the LFO frequency in Hz and its
/// start phase in degrees, the depth (0..255), the feedback (-100..100) and
/// the output gain in dB.
#[derive(Clone, Copy, Debug)]
pub struct Params {
    pub stages: i32,
    pub dry_wet: i32,
    pub freq: f64,
    pub phase_deg: f64,
    pub depth: i32,
    pub feedback: i32,
    pub out_gain_db: f64,
}

/// One channel of the effect: the LFO and the state of the allpass stages.
pub struct Phaser {
    /// An `f32` because the C++ field is one and the LFO step is computed
    /// from it widened to `f64`.
    samplerate: f32,
    /// The C++ used a `sampleCount` here, whose `as_double()` is this
    /// counter's conversion.
    skipcount: i64,
    /// The stage state; the array is as large as a phaser can be.
    old: [f64; MAX_STAGES as usize],
    gain: f64,
    fbout: f64,
    outgain: f64,
    lfoskip: f64,
    phase: f64,
    laststages: i32,
}

impl Phaser {
    /// Reset for `sample_rate` - the C++ `InstanceInit()`.
    pub fn new(sample_rate: f32) -> Self {
        Self {
            samplerate: sample_rate,
            skipcount: 0,
            old: [0.0; MAX_STAGES as usize],
            gain: 0.0,
            fbout: 0.0,
            outgain: 0.0,
            lfoskip: 0.0,
            phase: 0.0,
            laststages: 0,
        }
    }

    /// Filter `input` into `output` - the C++ `InstanceProcess()`.
    ///
    /// The two blocks have to be the same length; use [`Self::process_raw`]
    /// for buffers that overlap.
    pub fn process_block(
        &mut self,
        params: Params,
        input: &[f32],
        output: &mut [f32],
    ) {
        assert_eq!(input.len(), output.len(), "block lengths differ");

        // SAFETY: the two slices do not overlap (that is what holding one as
        // `&[f32]` and the other as `&mut [f32]` means) and both are valid
        // for that many samples.
        unsafe {
            self.process_raw(params, input.as_ptr(), output.as_mut_ptr(), input.len());
        }
    }

    /// The same, over raw buffers: each sample is read before it is written,
    /// so `input` and `output` may point at the same memory.
    ///
    /// # Safety
    ///
    /// Both pointers must be valid for `len` samples.
    pub unsafe fn process_raw(
        &mut self,
        params: Params,
        input: *const f32,
        output: *mut f32,
        len: usize,
    ) {
        // A C caller must not be able to index the stage array out of bounds.
        // The effect itself stays between two stages and MAX_STAGES.
        let stages = params.stages.clamp(0, MAX_STAGES);

        for j in self.laststages..stages {
            self.old[j as usize] = 0.0;
        }
        self.laststages = stages;

        self.lfoskip = params.freq * 2.0 * PI / f64::from(self.samplerate);
        self.phase = params.phase_deg * PI / 180.0;
        self.outgain = db_to_linear(params.out_gain_db);

        for i in 0..len {
            let sample = f64::from(unsafe { *input.add(i) });

            let mut m = sample + self.fbout * f64::from(params.feedback) / 101.0;

            // The C++ post-increments the counter in the condition, so the
            // LFO sees the value after this sample.
            let recompute = self.skipcount % LFO_SKIP_SAMPLES == 0;
            self.skipcount += 1;

            if recompute {
                // compute sine between 0 and 1
                let n = self.skipcount as f64;
                self.gain = (1.0 + (n * self.lfoskip + self.phase).cos()) / 2.0;

                // change lfo shape
                self.gain = (self.gain * LFO_SHAPE).exp_m1() / LFO_SHAPE.exp_m1();

                // attenuate the lfo
                self.gain = 1.0 - self.gain / 255.0 * f64::from(params.depth);
            }

            // phasing routine
            for j in 0..stages as usize {
                let tmp = self.old[j];
                self.old[j] = self.gain * tmp + m;
                m = tmp - self.gain * self.old[j];
            }
            self.fbout = m;

            let value = self.outgain
                * (m * f64::from(params.dry_wet)
                    + sample * f64::from(255 - params.dry_wet))
                / 255.0;
            unsafe { *output.add(i) = value as f32 };
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const BLOCK: usize = 16;

    /// The input the unit tests use; anything with a few distinct values
    /// will do.
    fn input() -> [f32; BLOCK] {
        let mut block = [0.0f32; BLOCK];
        for (i, sample) in block.iter_mut().enumerate() {
            *sample = ((i * 37) % 101) as f32 / 64.0 - 0.5;
        }
        block
    }

    fn params() -> Params {
        Params {
            stages: 24,
            dry_wet: 128,
            freq: 0.4,
            phase_deg: 0.0,
            depth: 100,
            feedback: 0,
            out_gain_db: -6.0,
        }
    }

    /// A fully dry block never reaches the filter: what comes out is the
    /// input with the output gain applied to it.
    #[test]
    fn a_fully_dry_block_is_the_input_times_the_gain() {
        let params = Params {
            dry_wet: 0,
            freq: 4.0,
            phase_deg: 90.0,
            depth: 255,
            feedback: 100,
            ..params()
        };
        let input = input();
        let mut output = [0.0f32; BLOCK];

        let mut phaser = Phaser::new(44100.0);
        phaser.process_block(params, &input, &mut output);

        let gain = db_to_linear(params.out_gain_db);
        for i in 0..BLOCK {
            let want = f64::from(input[i]) * gain;
            let got = f64::from(output[i]);
            assert!(
                (got - want).abs() <= 1e-6 * want.abs() + 1e-9,
                "sample {} came back as {}, not {}",
                i,
                got,
                want
            );
        }
    }

    /// The depth attenuates the LFO away from unity, so at zero the allpass
    /// stages are swept by exactly 1.0.
    #[test]
    fn zero_depth_keeps_the_lfo_gain_at_unity() {
        let mut output = [0.0f32; BLOCK];
        let mut phaser = Phaser::new(44100.0);
        phaser.process_block(
            Params {
                depth: 0,
                freq: 4.0,
                ..params()
            },
            &input(),
            &mut output,
        );

        assert_eq!(phaser.gain, 1.0);
    }

    /// Shrinking the stage count leaves the stages that fell out of it alone,
    /// as the C++ does; growing into them again clears them, which the golden
    /// records that change the stage count cover.
    #[test]
    fn shrinking_leaves_the_extra_stages_alone() {
        let mut output = [0.0f32; BLOCK];
        let mut phaser = Phaser::new(44100.0);

        phaser.process_block(params(), &input(), &mut output);
        let full = phaser.old;
        assert!(full.iter().any(|&value| value != 0.0));

        phaser.process_block(
            Params {
                stages: 6,
                ..params()
            },
            &input(),
            &mut output,
        );

        assert_eq!(
            &phaser.old[6..],
            &full[6..],
            "the stages that fell out of the count kept their state"
        );
        assert_ne!(
            &phaser.old[..6],
            &full[..6],
            "the live stages kept processing"
        );
    }
}
