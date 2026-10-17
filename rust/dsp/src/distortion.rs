//! EffectDistortion's waveshaper.
//!
//! Faithful port of the original C++ implementation
//! (`src/effects/DistortionFilter.h`, the signal processing that used to sit in
//! `src/effects/Distortion.cpp`): the effect shapes each sample through a
//! lookup table of 2049 entries that it builds for the settings at hand, one
//! builder per kind of distortion, with linear interpolation between the
//! entries and an optional rolling-average DC block.
//!
//! Every operation is mirrored in the same order and precision, including the
//! `f32` divisions some of the builders use where others divide in `f64`, the
//! pre-gain that is applied in `f64` and narrowed back, and the `f32` sample
//! rate the DC block's queue length is derived from.
//!
//! Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

use std::collections::VecDeque;
use std::f64::consts::{FRAC_PI_2, PI};

/// Number of positive or negative steps in the lookup table (`STEPS` in the
/// C++).
const STEPS: usize = 1024;

/// Size of the lookup table (`TABLESIZE` in the C++).
const TABLE_SIZE: usize = 2 * STEPS + 1;

/// The kinds of table, in the order of the effect's choice list (`kTableType`
/// in the C++).
const HARD_CLIP: i32 = 0;
const SOFT_CLIP: i32 = 1;
const HALF_SIN_CURVE: i32 = 2;
const EXP_CURVE: i32 = 3;
const LOG_CURVE: i32 = 4;
const CUBIC: i32 = 5;
const EVEN_HARMONICS: i32 = 6;
const SIN_CURVE: i32 = 7;
const LEVELLER: i32 = 8;
const RECTIFIER: i32 = 9;
const HARD_LIMITER: i32 = 10;

/// dB to linear amplitude - `DB_TO_LINEAR` in Rustacity.h.
fn db_to_linear(db: f64) -> f64 {
    10.0_f64.powf(db / 20.0)
}

/// The log curve of the soft clipper, `y = T + (((e^(RT - Rx)) - 1) / -R)`.
///
/// The value is an `f32` and so is the result, as in the C++.
fn log_curve(threshold: f64, value: f32, ratio: f64) -> f32 {
    (threshold + ((ratio * (threshold - f64::from(value))).exp() - 1.0) / -ratio) as f32
}

/// The cubic curve of the cubic table, `y = x - (x^3 / 3.0)`, except that the
/// effect leaves the sample alone at zero parameter 1.
fn cubic(params: Params, x: f64) -> f64 {
    if params.param1 == 0.0 {
        return x;
    }

    x - (x.powf(3.0) / 3.0)
}

/// The effect's settings for one block.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Params {
    /// One of the table kinds above, as the effect's choice list numbers them.
    pub table_choice: i32,
    /// Whether the DC block runs after the shaper.
    pub dc_block: bool,
    /// The upper threshold in dB, from -100 to 0.
    pub threshold_db: f64,
    /// The noise floor in dB, from -80 to -20; the Leveller uses it.
    pub noise_floor: f64,
    /// Parameter 1, from 0 to 100.
    pub param1: f64,
    /// Parameter 2, from 0 to 100.
    pub param2: f64,
    /// How many times the curve is applied.
    pub repeats: i32,
}

/// One channel of the effect: the lookup table of the current settings, what it
/// was built from, and the state of the DC block.
pub struct Distortion {
    table: [f64; TABLE_SIZE],
    /// The settings `table` holds, or `None` before it has been built.
    table_params: Option<Params>,
    /// The linear upper threshold the table was built with.
    threshold: f64,
    /// What some kinds of distortion need to bring the gain back to unity.
    makeup_gain: f64,
    /// The DC block's rolling average.
    queue: VecDeque<f32>,
    queue_total: f64,
    queue_length: u32,
}

impl Distortion {
    /// Reset for `sample_rate` - the C++ `InstanceInit()`.
    ///
    /// The C++ built the table here, from the effect's parameters; this builds
    /// it before the first block instead, which is the same table because the
    /// settings that shape it reach `process_block` either way.
    pub fn new(sample_rate: f32) -> Self {
        Self {
            table: [0.0; TABLE_SIZE],
            table_params: None,
            threshold: 0.0,
            makeup_gain: 1.0,
            queue: VecDeque::new(),
            queue_total: 0.0,
            queue_length: (f64::from(sample_rate) / 20.0).floor() as u32,
        }
    }

    /// Shape `input` into `output` - the C++ `InstanceProcess()`.
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
        // The C++ rebuilt the table only while the settings differed from the
        // ones of the block before, and a table is a function of the settings
        // alone, so rebuilding it whenever they differ from those it holds is
        // the same table.  The DC block is not part of it: it does not shape
        // the table.
        if self.table_params != Some(params) {
            self.threshold = db_to_linear(params.threshold_db);
            self.table_params = Some(params);
            self.make_table(params);
        }

        let p1 = params.param1 / 100.0;
        let p2 = params.param2 / 100.0;

        for i in 0..len {
            let sample = unsafe { *input.add(i) };
            let shaped = self.wave_shaper(params, sample);

            let mut out = match params.table_choice {
                // Param2 = make-up gain.
                HARD_CLIP | SOFT_CLIP => {
                    (f64::from(shaped) * ((1.0 - p2) + self.makeup_gain * p2)) as f32
                }
                HALF_SIN_CURVE | EXP_CURVE | LOG_CURVE | CUBIC | SIN_CURVE => {
                    (f64::from(shaped) * p2) as f32
                }
                HARD_LIMITER => {
                    // Mix equivalent to LADSPA effect's "Wet / Residual" mix
                    (f64::from(shaped) * (p1 - p2) + f64::from(sample) * p2) as f32
                }
                _ => shaped,
            };

            if params.dc_block {
                out = self.dc_filter(out);
            }

            unsafe { *output.add(i) = out };
        }
    }

    /// Build the table for `params` - the C++ `MakeTable()`.
    fn make_table(&mut self, params: Params) {
        match params.table_choice {
            HARD_CLIP => self.hard_clip(),
            SOFT_CLIP => self.soft_clip(params),
            HALF_SIN_CURVE => self.half_sin_table(params),
            EXP_CURVE => self.exponential_table(params),
            LOG_CURVE => self.logarithmic_table(params),
            CUBIC => self.cubic_table(params),
            EVEN_HARMONICS => self.even_harmonic_table(params),
            SIN_CURVE => self.sine_table(params),
            LEVELLER => self.leveller(params),
            RECTIFIER => self.rectifier(params),
            HARD_LIMITER => self.hard_clip(),
            // The C++ switch has no default either: an unknown kind leaves the
            // table as it was.
            _ => {}
        }
    }

    //
    // Preset tables for gain lookup
    //

    fn hard_clip(&mut self) {
        let low_thresh = 1.0 - self.threshold;
        let high_thresh = 1.0 + self.threshold;

        for n in 0..TABLE_SIZE {
            if (n as f64) < (STEPS as f64 * low_thresh) {
                self.table[n] = -self.threshold;
            } else if (n as f64) > (STEPS as f64 * high_thresh) {
                self.table[n] = self.threshold;
            } else {
                self.table[n] = n as f64 / STEPS as f64 - 1.0;
            }

            self.makeup_gain = 1.0 / self.threshold;
        }
    }

    fn soft_clip(&mut self, params: Params) {
        let upper = 1.0 + self.threshold;
        let amount = 2.0_f64.powf(7.0 * params.param1 / 100.0); // range 1 to 128
        let peak = log_curve(self.threshold, 1.0, amount);
        self.makeup_gain = 1.0 / f64::from(peak);
        self.table[STEPS] = 0.0; // origin

        // positive half of table
        for n in STEPS..TABLE_SIZE {
            if (n as f64) < (STEPS as f64 * upper) {
                // origin to threshold
                self.table[n] = f64::from(n as f32 / STEPS as f32 - 1.0);
            } else {
                let value = n as f64 / STEPS as f64 - 1.0;
                self.table[n] = f64::from(log_curve(self.threshold, value as f32, amount));
            }
        }
        self.copy_half_table();
    }

    fn exponential_table(&mut self, params: Params) {
        // avoid divide by zero
        let amount = 0.999_f64.min(db_to_linear(-1.0 * params.param1));

        for n in STEPS..TABLE_SIZE {
            let lin_val = f64::from(n as f32 / STEPS as f32);
            let scale = -1.0 / (1.0 - amount); // unity gain at 0dB
            let curve = ((lin_val - 1.0) * amount.ln()).exp();
            self.table[n] = scale * (curve - 1.0);
        }
        self.copy_half_table();
    }

    fn logarithmic_table(&mut self, params: Params) {
        let amount = params.param1;
        let stepsize = 1.0 / STEPS as f64;
        let mut lin_val = 0.0;

        if amount == 0.0 {
            for n in STEPS..TABLE_SIZE {
                self.table[n] = lin_val;
                lin_val += stepsize;
            }
        } else {
            for n in STEPS..TABLE_SIZE {
                self.table[n] = (1.0 + (amount * lin_val)).ln() / (1.0 + amount).ln();
                lin_val += stepsize;
            }
        }
        self.copy_half_table();
    }

    fn half_sin_table(&mut self, params: Params) {
        let iter = (params.param1 / 20.0).floor() as i32;
        let fractionalpart = (params.param1 / 20.0) - f64::from(iter);
        let stepsize = 1.0 / STEPS as f64;
        let mut lin_val = 0.0;

        for n in STEPS..TABLE_SIZE {
            self.table[n] = lin_val;
            for _ in 0..iter {
                self.table[n] = (self.table[n] * FRAC_PI_2).sin();
            }
            self.table[n] +=
                ((self.table[n] * FRAC_PI_2).sin() - self.table[n]) * fractionalpart;
            lin_val += stepsize;
        }
        self.copy_half_table();
    }

    fn cubic_table(&mut self, params: Params) {
        let amount = params.param1 * 3.0_f64.sqrt() / 100.0;
        let mut gain = 1.0;
        if amount != 0.0 {
            gain = 1.0 / cubic(params, amount.min(1.0));
        }

        let stepsize = amount / STEPS as f64;
        let mut x = -amount;

        if amount == 0.0 {
            for i in 0..TABLE_SIZE {
                self.table[i] = i as f64 / STEPS as f64 - 1.0;
            }
        } else {
            for i in 0..TABLE_SIZE {
                self.table[i] = gain * cubic(params, x);
                for _ in 0..params.repeats {
                    self.table[i] = gain * cubic(params, self.table[i] * amount);
                }
                x += stepsize;
            }
        }
    }

    fn even_harmonic_table(&mut self, params: Params) {
        let amount = params.param1 / -100.0;
        let c = 0.001_f64.max(params.param2) / 10.0;

        let step = 1.0 / STEPS as f64;
        let mut xval = -1.0;

        for i in 0..TABLE_SIZE {
            self.table[i] = ((1.0 + amount) * xval)
                - (xval * (amount / c.tanh()) * (c * xval).tanh());
            xval += step;
        }
    }

    fn sine_table(&mut self, params: Params) {
        let iter = (params.param1 / 20.0).floor() as i32;
        let fractionalpart = (params.param1 / 20.0) - f64::from(iter);
        let stepsize = 1.0 / STEPS as f64;
        let mut lin_val = 0.0;

        for n in STEPS..TABLE_SIZE {
            self.table[n] = lin_val;
            for _ in 0..iter {
                self.table[n] = (1.0 + ((self.table[n] * PI) - FRAC_PI_2).sin()) / 2.0;
            }
            self.table[n] += (((1.0 + ((self.table[n] * PI) - FRAC_PI_2).sin()) / 2.0)
                - self.table[n])
                * fractionalpart;
            lin_val += stepsize;
        }
        self.copy_half_table();
    }

    /// 'Leveller' wavetable is modeled on the legacy effect of the same name.
    fn leveller(&mut self, params: Params) {
        let noise_floor = db_to_linear(params.noise_floor);
        let num_passes = params.repeats;
        let fractional_pass = params.param1 / 100.0;

        const NUM_POINTS: usize = 6;
        let gain_factors: [f64; NUM_POINTS] = [0.80, 1.00, 1.20, 1.20, 1.00, 0.80];
        let mut gain_limits: [f64; NUM_POINTS] = [0.0001, 0.0, 0.1, 0.3, 0.5, 1.0];
        let mut add_on_values = [0.0_f64; NUM_POINTS];

        gain_limits[1] = noise_floor;

        // Calculate add-on values
        add_on_values[0] = 0.0;
        for i in 0..NUM_POINTS - 1 {
            add_on_values[i + 1] =
                add_on_values[i] + (gain_limits[i] * (gain_factors[i] - gain_factors[1 + i]));
        }

        // Positive half of table.
        // The original effect increased the 'strength' of the effect by
        // repeated passes over the audio data.
        // Here we model that more efficiently by repeated passes over a linear
        // table.
        for n in STEPS..TABLE_SIZE {
            self.table[n] = (n - STEPS) as f64 / STEPS as f64;
            for _ in 0..num_passes {
                // Find the highest index for gain adjustment
                let mut index = NUM_POINTS - 1;
                let mut i = index as i32;
                while i >= 0 && self.table[n] < gain_limits[i as usize] {
                    index = i as usize;
                    i -= 1;
                }
                // the whole number of 'repeats'
                self.table[n] = (self.table[n] * gain_factors[index]) + add_on_values[index];
            }
            // Extrapolate for fine adjustment.
            // tiny fractions are not worth the processing time
            if fractional_pass > 0.001 {
                let mut index = NUM_POINTS - 1;
                let mut i = index as i32;
                while i >= 0 && self.table[n] < gain_limits[i as usize] {
                    index = i as usize;
                    i -= 1;
                }
                self.table[n] +=
                    fractional_pass * ((self.table[n] * (gain_factors[index] - 1.0)) + add_on_values[index]);
            }
        }
        self.copy_half_table();
    }

    /// 0% = Dry, 50% = half-wave rectified, 100% = full-wave rectified (abs
    /// value).
    fn rectifier(&mut self, params: Params) {
        let amount = (params.param1 / 50.0) - 1.0;
        let stepsize = 1.0 / STEPS as f64;
        let mut index = STEPS;

        // positive half of waveform is passed unaltered.
        for n in 0..=STEPS {
            self.table[index] = n as f64 * stepsize;
            index += 1;
        }

        // negative half of table, walking down from STEPS - 1 to 0 (the C++
        // decremented a counter and left it below zero after the last pass).
        for n in 1..=STEPS {
            self.table[STEPS - n] = n as f64 * stepsize * amount;
        }
    }

    //
    // Helper functions for lookup tables
    //

    /// Copy the negative half of the table from the positive half.
    fn copy_half_table(&mut self) {
        let mut count = TABLE_SIZE - 1;
        for n in 0..STEPS {
            self.table[n] = -self.table[count];
            count -= 1;
        }
    }

    fn wave_shaper(&self, params: Params, sample: f32) -> f32 {
        // Pre-gain, applied in f64 and narrowed back, as the C++ did through
        // its `float *= double`.
        let pre_gain = if params.table_choice == HARD_CLIP {
            1.0 + params.param1 / 100.0
        } else {
            1.0
        };
        let sample = (f64::from(sample) * pre_gain) as f32;

        let index = ((sample * STEPS as f32).floor() + STEPS as f32) as i32;
        let index = index.min(2 * STEPS as i32 - 1).max(0);
        let mut x_offset = f64::from((1.0 + sample) * STEPS as f32 - index as f32);
        x_offset = x_offset.max(0.0).min(1.0); // Clip at 0dB

        // linear interpolation: y = y0 + (y1-y0)*(x-x0)
        let index = index as usize;
        (self.table[index] + (self.table[index + 1] - self.table[index]) * x_offset) as f32
    }

    fn dc_filter(&mut self, sample: f32) -> f32 {
        // Rolling average gives less offset at the start than an IIR filter.
        self.queue_total += f64::from(sample);
        self.queue.push_back(sample);

        if self.queue.len() as u32 > self.queue_length {
            let front = self.queue.pop_front().expect("a sample was just pushed");
            self.queue_total -= f64::from(front);
        }

        (f64::from(sample) - self.queue_total / self.queue.len() as f64) as f32
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const BLOCK: usize = 64;

    /// The input the unit tests use.
    fn input(block: usize) -> [f32; BLOCK] {
        let mut samples = [0.0f32; BLOCK];
        for (i, sample) in samples.iter_mut().enumerate() {
            *sample = (((i * 37 + block * 11) % 129) as i32 - 64) as f32 / 64.0;
        }
        samples
    }

    fn params() -> Params {
        Params {
            table_choice: HARD_CLIP,
            dc_block: false,
            threshold_db: -6.0,
            noise_floor: -70.0,
            param1: 50.0,
            param2: 50.0,
            repeats: 1,
        }
    }

    fn table_choices() -> std::ops::RangeInclusive<i32> {
        HARD_CLIP..=HARD_LIMITER
    }

    /// A zero input leaves the shaper at (nearly) zero, whatever the settings
    /// are: every table has its origin at zero and the DC block of a fresh
    /// filter has nothing to average.  The cubic table is not exactly zero
    /// there, because its entries walk an accumulated x, as the C++ does.
    #[test]
    fn silence_in_is_silence_out() {
        for table_choice in table_choices() {
            for dc_block in [false, true] {
                let mut output = [1.0f32; BLOCK];

                for (threshold_db, noise_floor, param1, param2, repeats) in [
                    (-6.0, -70.0, 50.0, 50.0, 1),
                    (-100.0, -80.0, 0.0, 0.0, 0),
                    (0.0, -30.0, 100.0, 100.0, 5),
                ] {
                    let mut distortion = Distortion::new(44100.0);
                    distortion.process_block(
                        Params {
                            table_choice,
                            dc_block,
                            threshold_db,
                            noise_floor,
                            param1,
                            param2,
                            repeats,
                        },
                        &[0.0f32; BLOCK],
                        &mut output,
                    );

                    assert!(
                        output.iter().all(|sample| sample.abs() <= 1e-9),
                        "table {}, DC block {}, threshold {} dB is not silent: {:?}",
                        table_choice,
                        dc_block,
                        threshold_db,
                        output
                    );
                }
            }
        }
    }

    /// At the top of its range the threshold does not clip, so the hard clipper
    /// passes the sample through, and at half make-up gain the mix sums to
    /// unity: the effect is the identity.
    #[test]
    fn hard_clipping_at_full_threshold_is_the_identity() {
        let params = Params {
            table_choice: HARD_CLIP,
            dc_block: false,
            threshold_db: 0.0,
            noise_floor: -70.0,
            param1: 0.0,
            param2: 50.0,
            repeats: 1,
        };
        let input = input(0);

        let mut output = [0.0f32; BLOCK];
        let mut distortion = Distortion::new(44100.0);
        distortion.process_block(params, &input, &mut output);

        for i in 0..BLOCK {
            assert!(
                (output[i] - input[i]).abs() <= 1e-6,
                "sample {}: got {}, want {}",
                i,
                output[i],
                input[i]
            );
        }
    }

    /// The table is a function of the settings, so a run that changes them
    /// mid-stream has to sound like a filter that was given the new settings
    /// from the start.  With the DC block off the table is all the state there
    /// is.
    #[test]
    fn the_table_follows_the_settings() {
        let first = Params {
            table_choice: CUBIC,
            param1: 20.0,
            ..params()
        };
        let second = Params {
            table_choice: CUBIC,
            param1: 90.0,
            ..params()
        };

        let mut changed = Distortion::new(48000.0);
        let mut fresh = Distortion::new(48000.0);

        let mut warmed = [0.0f32; BLOCK];
        changed.process_block(first, &input(0), &mut warmed);

        let mut got = [0.0f32; BLOCK];
        let mut want = [0.0f32; BLOCK];
        changed.process_block(second, &input(1), &mut got);
        fresh.process_block(second, &input(1), &mut want);

        assert_eq!(got, want, "the second block used a stale table");
    }

    /// Every table kind and a spread of settings stay finite: the shaper
    /// interpolates inside the table and the DC block averages, so this is the
    /// cheap guard against an index or a sum that runs away.
    #[test]
    fn the_whole_parameter_range_stays_finite() {
        for table_choice in table_choices() {
            for threshold_db in [-100.0, -6.0, 0.0] {
                for param1 in [0.0, 50.0, 100.0] {
                    for param2 in [0.0, 50.0, 100.0] {
                        for repeats in [0, 1, 5] {
                            let params = Params {
                                table_choice,
                                dc_block: true,
                                threshold_db,
                                noise_floor: -20.0,
                                param1,
                                param2,
                                repeats,
                            };
                            let mut output = [0.0f32; BLOCK];
                            let mut distortion = Distortion::new(44100.0);
                            distortion.process_block(params, &input(1), &mut output);

                            assert!(
                                output.iter().all(|sample| sample.is_finite()),
                                "table {}, threshold {}, params {} {} x{} went infinite",
                                table_choice,
                                threshold_db,
                                param1,
                                param2,
                                repeats
                            );
                        }
                    }
                }
            }
        }
    }
}
