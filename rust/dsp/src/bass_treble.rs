//! The low and high shelf filters of EffectBassTreble.
//!
//! Faithful port of the original C++ implementation
//! (`src/effects/BassTrebleFilter.h`, the signal processing that used to
//! sit in `src/effects/BassTreble.cpp`): a low shelf at 250 Hz, a high
//! shelf at 4 kHz and the effect's output gain.  Every operation is
//! mirrored in the same order and precision, including the two
//! single-precision round trips - the coefficient design widens the `f32`
//! sample rate and slope to `f64`, and each filtered sample is narrowed
//! to `f32` before the gain is applied to it in `f64`.
//!
//! Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

use std::f64::consts::PI;

/// Both shelves are designed with this slope.  The C++ writes the `float`
/// literal `0.4f` into a `double`, which is not the same number as `0.4`,
/// so keep the round trip.
const SLOPE: f64 = 0.4f32 as f64;

/// The corner frequencies of the two shelves, from the C++ `Init()`.
const HZ_BASS: f64 = 250.0;
const HZ_TREBLE: f64 = 4000.0;

/// dB to linear amplitude - `DB_TO_LINEAR` in Rustacity.h.
fn db_to_linear(db: f64) -> f64 {
    10.0_f64.powf(db / 20.0)
}

/// Which shelf [`BassTreble::coefficients`] designs.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Shelf {
    /// The low shelf, at 250 Hz.
    Bass,
    /// The high shelf, at 4 kHz.
    Treble,
}

/// One shelf's biquad, in the order the C++ `Coefficents()` wrote its six
/// reference parameters out.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Coefficients {
    pub a0: f64,
    pub a1: f64,
    pub a2: f64,
    pub b0: f64,
    pub b1: f64,
    pub b2: f64,
}

impl Coefficients {
    /// The coefficients `Init()` starts from (`a0` 1, everything else 0).
    const RESET: Coefficients = Coefficients {
        a0: 1.0,
        a1: 0.0,
        a2: 0.0,
        b0: 0.0,
        b1: 0.0,
        b2: 0.0,
    };
}

/// One channel of the effect: the two shelves and their filter state.
pub struct BassTreble {
    /// An `f32` because the C++ field is one and `coefficients()` takes
    /// it widened to `f64`.
    samplerate: f32,
    slope: f64,
    hz_bass: f64,
    hz_treble: f64,
    /// The output gain, linear, from this block's gain parameter.
    gain: f64,
    bass: Coefficients,
    treble: Coefficients,
    xn1_bass: f64,
    xn2_bass: f64,
    yn1_bass: f64,
    yn2_bass: f64,
    xn1_treble: f64,
    xn2_treble: f64,
    yn1_treble: f64,
    yn2_treble: f64,
}

impl BassTreble {
    /// A reset filter for `sample_rate`, which is narrowed to `f32` exactly
    /// like the C++ passes the effect's `double` sample rate to `Init()`.
    ///
    /// The C++ also passed the output gain to `Init()`, but every block
    /// recomputes it in [`Self::set_parameters`] before any sample is
    /// filtered, so there is nothing to pass here.
    pub fn new(sample_rate: f32) -> Self {
        Self {
            samplerate: sample_rate,
            slope: SLOPE,
            hz_bass: HZ_BASS,
            hz_treble: HZ_TREBLE,
            gain: 1.0,
            bass: Coefficients::RESET,
            treble: Coefficients::RESET,
            xn1_bass: 0.0,
            xn2_bass: 0.0,
            yn1_bass: 0.0,
            yn2_bass: 0.0,
            xn1_treble: 0.0,
            xn2_treble: 0.0,
            yn1_treble: 0.0,
            yn2_treble: 0.0,
        }
    }

    /// The coefficients of one shelf - the C++ `Coefficents()`.
    ///
    /// `gain_db` is the shelf's own parameter (the effect's bass or treble
    /// value), not the output gain.  Public because the golden-value
    /// capture dumped these coefficients too.
    pub fn coefficients(
        shelf: Shelf,
        hz: f64,
        slope: f64,
        gain_db: f64,
        samplerate: f64,
    ) -> Coefficients {
        let w = 2.0 * PI * hz / samplerate;
        let a = (10.0_f64.ln() * gain_db / 40.0).exp();
        let b = ((a * a + 1.0) / slope - (a - 1.0).powf(2.0)).sqrt();

        if shelf == Shelf::Bass {
            Coefficients {
                b0: a * ((a + 1.0) - (a - 1.0) * w.cos() + b * w.sin()),
                b1: 2.0 * a * ((a - 1.0) - (a + 1.0) * w.cos()),
                b2: a * ((a + 1.0) - (a - 1.0) * w.cos() - b * w.sin()),
                a0: (a + 1.0) + (a - 1.0) * w.cos() + b * w.sin(),
                a1: -2.0 * ((a - 1.0) + (a + 1.0) * w.cos()),
                a2: (a + 1.0) + (a - 1.0) * w.cos() - b * w.sin(),
            }
        } else {
            Coefficients {
                b0: a * ((a + 1.0) + (a - 1.0) * w.cos() + b * w.sin()),
                b1: -2.0 * a * ((a - 1.0) + (a + 1.0) * w.cos()),
                b2: a * ((a + 1.0) + (a - 1.0) * w.cos() - b * w.sin()),
                a0: (a + 1.0) - (a - 1.0) * w.cos() + b * w.sin(),
                a1: 2.0 * ((a - 1.0) - (a + 1.0) * w.cos()),
                a2: (a + 1.0) - (a - 1.0) * w.cos() - b * w.sin(),
            }
        }
    }

    /// Take this block's parameters: the output gain and the coefficients of
    /// both shelves.
    ///
    /// The C++ did this at the top of every `Process()` and only skipped the
    /// coefficient design when its cache said the parameter was unchanged -
    /// but its two cache keys are never assigned, so it always designed
    /// both shelves.  Doing the same unconditionally is therefore not just
    /// simpler but identical.
    pub fn set_parameters(&mut self, bass_db: f64, treble_db: f64, gain_db: f64) {
        self.gain = db_to_linear(gain_db);

        let samplerate = f64::from(self.samplerate);
        self.bass = Self::coefficients(Shelf::Bass, self.hz_bass, self.slope, bass_db, samplerate);
        self.treble =
            Self::coefficients(Shelf::Treble, self.hz_treble, self.slope, treble_db, samplerate);
    }

    /// Filter one sample through the low shelf, then the high shelf, and
    /// apply the output gain - the C++ `Process()` loop body.
    pub fn process_one(&mut self, input: f32) -> f32 {
        // Low shelf.  The C++ accumulates in `double` and narrows the
        // result to `float` on assignment, so the state stays `f64` and
        // only `out` becomes `f32`.
        let mut out = ((self.bass.b0 * f64::from(input)
            + self.bass.b1 * self.xn1_bass
            + self.bass.b2 * self.xn2_bass
            - self.bass.a1 * self.yn1_bass
            - self.bass.a2 * self.yn2_bass)
            / self.bass.a0) as f32;
        self.xn2_bass = self.xn1_bass;
        self.xn1_bass = f64::from(input);
        self.yn2_bass = self.yn1_bass;
        self.yn1_bass = f64::from(out);

        // High shelf, fed with the low shelf's output.
        let input = out;
        out = ((self.treble.b0 * f64::from(input)
            + self.treble.b1 * self.xn1_treble
            + self.treble.b2 * self.xn2_treble
            - self.treble.a1 * self.yn1_treble
            - self.treble.a2 * self.yn2_treble)
            / self.treble.a0) as f32;
        self.xn2_treble = self.xn1_treble;
        self.xn1_treble = f64::from(input);
        self.yn2_treble = self.yn1_treble;
        self.yn1_treble = f64::from(out);

        // `DoFilter(...) * data.gain` in the C++: a `float` promoted to
        // `double` for the multiplication and narrowed back on store.
        (f64::from(out) * self.gain) as f32
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// At 0 dB both shelves are flat, so the whole effect is a
    /// pass-through (up to the rounding of the filter arithmetic).
    #[test]
    fn flat_at_zero_db_is_pass_through() {
        let mut state = BassTreble::new(44100.0);
        state.set_parameters(0.0, 0.0, 0.0);
        for x in [0.0f32, 1.0, -1.0, 0.5, -0.25, 0.125] {
            let y = state.process_one(x);
            assert!((y - x).abs() < 1e-6, "{} came back as {}", x, y);
        }
    }

    /// A 0 dB shelf designs identical numerator and denominator
    /// coefficients - the `bt-coeffs` records show it too - which is what
    /// makes the pass-through above hold.
    #[test]
    fn zero_db_coefficients_are_symmetric() {
        for (shelf, hz) in [(Shelf::Bass, HZ_BASS), (Shelf::Treble, HZ_TREBLE)] {
            let c = BassTreble::coefficients(shelf, hz, SLOPE, 0.0, 48000.0);
            assert_eq!(c.b0, c.a0);
            assert_eq!(c.b1, c.a1);
            assert_eq!(c.b2, c.a2);
            assert_ne!(c.a0, 0.0);
        }
    }

    /// The output gain is applied to the filtered sample, outside the two
    /// filters: with flat shelves the output follows it.
    #[test]
    fn gain_scales_a_flat_effect() {
        let mut quiet = BassTreble::new(44100.0);
        quiet.set_parameters(0.0, 0.0, -20.0);
        let mut loud = BassTreble::new(44100.0);
        loud.set_parameters(0.0, 0.0, 0.0);

        for x in [0.25f32, -0.5, 0.75] {
            let a = f64::from(quiet.process_one(x));
            let b = f64::from(loud.process_one(x));
            // -20 dB is one tenth, up to the f32 rounding of the sum.
            assert!((a - 0.1 * b).abs() < 1e-6, "{} vs {}", a, b);
        }
    }
}
