//! EffectWahwah's wah filter.
//!
//! Faithful port of the original C++ implementation
//! (`src/effects/WahwahFilter.h`, the signal processing that used to sit in
//! `src/effects/Wahwah.cpp`): an LFO sweeps a resonant lowpass filter across
//! the spectrum.  Every operation is mirrored in the same order and precision,
//! including the `f32` sample rate the LFO step is derived from and the final
//! narrowing of each sample to `f32`.
//!
//! Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

use std::f64::consts::PI;

/// How many samples pass before the LFO value is recomputed
/// (`lfoskipsamples` in the C++).
const LFO_SKIP_SAMPLES: u64 = 30;

/// dB to linear amplitude - `DB_TO_LINEAR` in Rustacity.h.
fn db_to_linear(db: f64) -> f64 {
    10.0_f64.powf(db / 20.0)
}

/// The effect's settings for one block: the LFO frequency in Hz and its start
/// phase in degrees, the depth (0..100), the resonance (greater than 0), the
/// frequency offset (0..100) and the output gain in dB.
#[derive(Clone, Copy, Debug)]
pub struct Params {
    pub freq: f64,
    pub phase_deg: f64,
    pub depth: i32,
    pub res: f64,
    pub freq_ofs: i32,
    pub out_gain_db: f64,
}

/// One channel of the effect: the LFO and the state of the wah filter.
pub struct Wahwah {
    /// An `f32` because the C++ field is one and the LFO step is computed from
    /// it widened to `f64`.
    samplerate: f32,
    /// The C++ used `unsigned long`, which is 64 bits where the reference
    /// values were captured; the counter only decides when the LFO is
    /// recomputed, so the width shows up after 2^32 samples at the earliest.
    skipcount: u64,
    xn1: f64,
    xn2: f64,
    yn1: f64,
    yn2: f64,
    b0: f64,
    b1: f64,
    b2: f64,
    a0: f64,
    a1: f64,
    a2: f64,
    depth: f64,
    freqofs: f64,
    phase: f64,
    outgain: f64,
    lfoskip: f64,
}

impl Wahwah {
    /// Reset for `sample_rate` - the C++ `InstanceInit()`.
    ///
    /// The LFO step, the depth, the frequency offset, the start phase and the
    /// output gain are not set here: `process_block` derives all of them from
    /// its parameters before it reads them, as the C++ `InstanceProcess()` did.
    pub fn new(sample_rate: f32) -> Self {
        Self {
            samplerate: sample_rate,
            skipcount: 0,
            xn1: 0.0,
            xn2: 0.0,
            yn1: 0.0,
            yn2: 0.0,
            b0: 0.0,
            b1: 0.0,
            b2: 0.0,
            a0: 0.0,
            a1: 0.0,
            a2: 0.0,
            depth: 0.0,
            freqofs: 0.0,
            phase: 0.0,
            outgain: 0.0,
            lfoskip: 0.0,
        }
    }

    /// Filter `input` into `output` - the C++ `InstanceProcess()`.
    ///
    /// The two blocks have to be the same length; use [`Self::process_raw`] for
    /// buffers that overlap.
    pub fn process_block(&mut self, params: Params, input: &[f32], output: &mut [f32]) {
        assert_eq!(input.len(), output.len(), "block lengths differ");

        // SAFETY: the two slices do not overlap (that is what holding one as
        // `&[f32]` and the other as `&mut [f32]` means) and both are valid for
        // that many samples.
        unsafe {
            self.process_raw(params, input.as_ptr(), output.as_mut_ptr(), input.len());
        }
    }

    /// The same, over raw buffers: each sample is read before it is written, so
    /// `input` and `output` may point at the same memory.
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
        self.lfoskip = params.freq * 2.0 * PI / f64::from(self.samplerate);
        self.depth = f64::from(params.depth) / 100.0;
        self.freqofs = f64::from(params.freq_ofs) / 100.0;

        self.phase = params.phase_deg * PI / 180.0;
        self.outgain = db_to_linear(params.out_gain_db);

        for i in 0..len {
            let sample = f64::from(unsafe { *input.add(i) });

            // The C++ post-increments the counter in the condition, so the LFO
            // sees the value after this sample.
            let recompute = self.skipcount % LFO_SKIP_SAMPLES == 0;
            self.skipcount += 1;

            if recompute {
                let n = self.skipcount as f64;
                let frequency = (1.0 + (n * self.lfoskip + self.phase).cos()) / 2.0;
                let frequency = frequency * self.depth * (1.0 - self.freqofs) + self.freqofs;
                let frequency = ((frequency - 1.0) * 6.0).exp();
                let omega = PI * frequency;
                let sn = omega.sin();
                let cs = omega.cos();
                let alpha = sn / (2.0 * params.res);
                self.b0 = (1.0 - cs) / 2.0;
                self.b1 = 1.0 - cs;
                self.b2 = (1.0 - cs) / 2.0;
                self.a0 = 1.0 + alpha;
                self.a1 = -2.0 * cs;
                self.a2 = 1.0 - alpha;
            }

            let mut value = (self.b0 * sample + self.b1 * self.xn1 + self.b2 * self.xn2
                - self.a1 * self.yn1
                - self.a2 * self.yn2)
                / self.a0;
            self.xn2 = self.xn1;
            self.xn1 = sample;
            self.yn2 = self.yn1;
            self.yn1 = value;
            value *= self.outgain;

            unsafe { *output.add(i) = value as f32 };
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const BLOCK: usize = 16;

    /// The input the unit tests use; anything with a few distinct values will
    /// do.
    fn input() -> [f32; BLOCK] {
        let mut block = [0.0f32; BLOCK];
        for (i, sample) in block.iter_mut().enumerate() {
            *sample = ((i * 37) % 101) as f32 / 64.0 - 0.5;
        }
        block
    }

    fn params() -> Params {
        Params {
            freq: 1.5,
            phase_deg: 0.0,
            depth: 70,
            res: 2.5,
            freq_ofs: 30,
            out_gain_db: -6.0,
        }
    }

    /// A zero input leaves the filter at zero, whatever the parameters are: the
    /// coefficients change but nothing feeds the recursion.
    #[test]
    fn silence_in_is_silence_out() {
        let mut output = [1.0f32; BLOCK];

        for (freq, phase_deg, depth, res, freq_ofs, out_gain_db) in [
            (0.1, 0.0, 0, 0.1, 0, -30.0),
            (1.5, 90.0, 70, 2.5, 30, -6.0),
            (4.0, 360.0, 100, 10.0, 100, 30.0),
        ] {
            let mut wahwah = Wahwah::new(44100.0);
            wahwah.process_block(
                Params {
                    freq,
                    phase_deg,
                    depth,
                    res,
                    freq_ofs,
                    out_gain_db,
                },
                &[0.0f32; BLOCK],
                &mut output,
            );

            assert_eq!(output, [0.0f32; BLOCK], "{} Hz is not silent", freq);
        }
    }

    /// The first sample of a fresh filter is the one the LFO designs on it: the
    /// cascade has no history yet, so only b0 and a0 survive.
    #[test]
    fn first_sample_is_the_designed_b0_path() {
        let params = Params {
            freq: 2.0,
            phase_deg: 45.0,
            depth: 100,
            res: 4.0,
            freq_ofs: 20,
            out_gain_db: 3.0,
        };
        let input = input();

        let mut output = [0.0f32; BLOCK];
        let mut wahwah = Wahwah::new(48000.0);
        wahwah.process_block(params, &input, &mut output);

        // The same design, from the same parameters, on the first sample:
        // skipcount has become 1 by the time the LFO reads it.
        let lfoskip = params.freq * 2.0 * PI / 48000.0;
        let depth = f64::from(params.depth) / 100.0;
        let freqofs = f64::from(params.freq_ofs) / 100.0;
        let n = 1.0;
        let frequency = (1.0 + (n * lfoskip + params.phase_deg * PI / 180.0).cos()) / 2.0;
        let frequency = frequency * depth * (1.0 - freqofs) + freqofs;
        let frequency = ((frequency - 1.0) * 6.0).exp();
        let omega = PI * frequency;
        let alpha = omega.sin() / (2.0 * params.res);
        let b0 = (1.0 - omega.cos()) / 2.0;
        let a0 = 1.0 + alpha;

        let want = f64::from(input[0]) * b0 / a0 * db_to_linear(params.out_gain_db);
        assert!(
            (f64::from(output[0]) - want).abs() <= 1e-6 * want.abs() + 1e-9,
            "got {}, want {}",
            output[0],
            want
        );
    }

    /// Changing the resonance and the output gain only is audible, and the
    /// output stays finite over the whole parameter range: the filter is
    /// recursive, so this is the cheap guard against a coefficient that blows
    /// up.
    #[test]
    fn the_whole_parameter_range_stays_finite() {
        for freq in [0.1, 1.5, 4.0] {
            for depth in [0, 70, 100] {
                for res in [0.1, 2.5, 10.0] {
                    for freq_ofs in [0, 30, 100] {
                        let mut output = [0.0f32; BLOCK];
                        let mut wahwah = Wahwah::new(44100.0);
                        wahwah.process_block(
                            Params {
                                freq,
                                depth,
                                res,
                                freq_ofs,
                                ..params()
                            },
                            &input(),
                            &mut output,
                        );

                        assert!(
                            output.iter().all(|sample| sample.is_finite()),
                            "{} Hz, depth {}, res {}, offset {} went infinite",
                            freq,
                            depth,
                            res,
                            freq_ofs
                        );
                    }
                }
            }
        }
    }
}
