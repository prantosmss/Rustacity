//! Biquad digital filter design and processing.
//!
//! Faithful port of the original C++ implementation
//! (`src/effects/Biquad.{h,cpp}`): every operation is mirrored in the
//! same order and precision, including the deliberate single-precision
//! round trips (the C++ `Calc2D_DistSqr` returns `float`, so a `sqrt`
//! of its result is performed in f32 before being widened to f64).
//!
//! Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

use std::f64::consts::PI;

/// Possible filter orders for the design functions.
pub const MIN_ORDER: i32 = 1;
pub const MAX_ORDER: i32 = 10;

/// Filter subtypes, mirroring `Biquad::kSubTypes`.
pub const LOWPASS: i32 = 0;
pub const HIGHPASS: i32 = 1;

/// Numerator coefficient indices (`B0`, `B1`, `B2`).
pub const B0: usize = 0;
pub const B1: usize = 1;
pub const B2: usize = 2;
/// Denominator coefficient indices (`A1`, `A2`, `A0 == 1.0`).
pub const A1: usize = 0;
pub const A2: usize = 1;

/// Chebyshev polynomials of the first kind (order 0, 1, ... 9),
/// mirroring `Biquad::s_fChebyCoeffs`.
const CHEBY_COEFFS: [[f64; 11]; 10] = [
    [0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [-1.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [0.0, -3.0, 0.0, 4.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [1.0, 0.0, -8.0, 0.0, 8.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [0.0, 5.0, 0.0, -20.0, 0.0, 16.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [-1.0, 0.0, 18.0, 0.0, -48.0, 0.0, 32.0, 0.0, 0.0, 0.0, 0.0],
    [0.0, -7.0, 0.0, 56.0, 0.0, -112.0, 0.0, 64.0, 0.0, 0.0, 0.0],
    [1.0, 0.0, -32.0, 0.0, 160.0, 0.0, -256.0, 0.0, 128.0, 0.0, 0.0],
    [0.0, 9.0, 0.0, -120.0, 0.0, 432.0, 0.0, -576.0, 0.0, 256.0, 0.0],
    [-1.0, 0.0, 50.0, 0.0, -400.0, 0.0, 1120.0, 0.0, -1280.0, 0.0, 512.0],
];

/// A biquad section: numerator/denominator coefficients (`A0 == 1.0`)
/// and the sample state.  The `#[repr(C)]` layout mirrors the C struct
/// `rustacity_biquad_t` in `include/rustacity_dsp.h`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Biquad {
    pub numer: [f64; 3], // B0 B1 B2
    pub denom: [f64; 2], // A1 A2
    pub prev_in: f64,
    pub prev_prev_in: f64,
    pub prev_out: f64,
    pub prev_prev_out: f64,
}

impl Default for Biquad {
    fn default() -> Self {
        Self::new()
    }
}

impl Biquad {
    /// Equivalent of the C++ default constructor: pass-through
    /// coefficients and a reset state.
    pub fn new() -> Self {
        Biquad {
            numer: [1.0, 0.0, 0.0],
            denom: [0.0, 0.0],
            prev_in: 0.0,
            prev_prev_in: 0.0,
            prev_out: 0.0,
            prev_prev_out: 0.0,
        }
    }

    /// Clear the sample state.
    pub fn reset(&mut self) {
        self.prev_in = 0.0;
        self.prev_prev_in = 0.0;
        self.prev_out = 0.0;
        self.prev_prev_out = 0.0;
    }

    /// One sample through the section.  All arithmetic is in f64.
    #[inline]
    pub fn process_one(&mut self, f_in: f32) -> f32 {
        let f_out = f64::from(f_in) * self.numer[B0]
            + self.prev_in * self.numer[B1]
            + self.prev_prev_in * self.numer[B2]
            - self.prev_out * self.denom[A1]
            - self.prev_prev_out * self.denom[A2];
        self.prev_prev_in = self.prev_in;
        self.prev_in = f64::from(f_in);
        self.prev_prev_out = self.prev_out;
        self.prev_out = f_out;
        f_out as f32
    }

    /// A block through the section.
    pub fn process(&mut self, input: &[f32], output: &mut [f32]) {
        assert_eq!(input.len(), output.len());
        for (i, o) in input.iter().zip(output.iter_mut()) {
            *o = self.process_one(*i);
        }
    }
}

#[inline]
fn square(a: f64) -> f64 {
    a * a
}

/// `wxMax` for the two doubles used by the C++ source.
#[inline]
fn wx_max(a: f64, b: f64) -> f64 {
    if a > b {
        a
    } else {
        b
    }
}

/// `DB_TO_LINEAR` from Rustacity.h: `pow(10.0, (x) / 20.0)`.
#[inline]
fn db_to_linear(x: f64) -> f64 {
    10f64.powf(x / 20.0)
}

/// Complex division, mirroring `Biquad::ComplexDiv`.
pub fn complex_div(
    f_numer_r: f64,
    f_numer_i: f64,
    f_denom_r: f64,
    f_denom_i: f64,
) -> (f64, f64) {
    let f_denom = square(f_denom_r) + square(f_denom_i);
    (
        (f_numer_r * f_denom_r + f_numer_i * f_denom_i) / f_denom,
        (f_numer_i * f_denom_r - f_numer_r * f_denom_i) / f_denom,
    )
}

/// Bilinear transform, mirroring `Biquad::BilinTransform`.
pub fn bilin_transform(f_sx: f64, f_sy: f64) -> (f64, f64) {
    let f_denom = square(1.0 - f_sx) + square(f_sy);
    (
        (1.0 - square(f_sx) - square(f_sy)) / f_denom,
        2.0 * f_sy / f_denom,
    )
}

/// Squared 2D distance, mirroring `Biquad::Calc2D_DistSqr` - note the
/// C++ signature returns `float`, which the callers rely on.
pub fn calc_2d_dist_sqr(f_x1: f64, f_y1: f64, f_x2: f64, f_y2: f64) -> f32 {
    (square(f_x1 - f_x2) + square(f_y1 - f_y2)) as f32
}

/// Chebyshev polynomial of the first kind, mirroring `Biquad::ChebyPoly`
/// (`NormFreq == 1` at the `f0` point where the response is R dB down).
pub fn cheby_poly(order: i32, norm_freq: f64) -> f64 {
    debug_assert!((MIN_ORDER..=MAX_ORDER).contains(&order));
    let mut x = 1.0;
    let mut f_sum = 0.0;
    for i in 0..=order as usize {
        f_sum += CHEBY_COEFFS[(order - 1) as usize][i] * x;
        x *= norm_freq;
    }
    f_sum
}

/// Design a Butterworth filter.
///
/// * `order` - filter order
/// * `fn_` - Nyquist frequency, i.e. half the sample rate
/// * `fc` - cutoff frequency
/// * `subtype` - [`LOWPASS`] or [`HIGHPASS`]
pub fn calc_butterworth_filter(order: i32, fn_: f64, fc: f64, subtype: i32) -> Vec<Biquad> {
    if order < MIN_ORDER {
        return Vec::new();
    }
    let count = ((order + 1) / 2) as usize;
    let mut p_biquad = vec![Biquad::new(); count];

    // Set up the coefficients in all the biquads
    let mut f_norm = fc / fn_;
    if f_norm >= 0.9999 {
        f_norm = 0.9999f32 as f64;
    }
    let f_c = (PI * f_norm / 2.0).tan();
    let mut f_dc_pole_dist_sqr = 1.0f32 as f64;

    if (order & 1) == 0 {
        // Even order
        for i_pair in 0..order / 2 {
            let f_s_pole_x = f_c * (PI - (i_pair as f64 + 0.5) * PI / order as f64).cos();
            let f_s_pole_y = f_c * (PI - (i_pair as f64 + 0.5) * PI / order as f64).sin();
            let (f_z_pole_x, f_z_pole_y) = bilin_transform(f_s_pole_x, f_s_pole_y);
            let i = i_pair as usize;
            p_biquad[i].numer[B0] = 1.0;
            if subtype == LOWPASS {
                p_biquad[i].numer[B1] = 2.0;
            } else {
                p_biquad[i].numer[B1] = -2.0;
            }
            p_biquad[i].numer[B2] = 1.0;
            p_biquad[i].denom[A1] = -2.0 * f_z_pole_x;
            p_biquad[i].denom[A2] = square(f_z_pole_x) + square(f_z_pole_y);
            if subtype == LOWPASS {
                f_dc_pole_dist_sqr *= calc_2d_dist_sqr(1.0, 0.0, f_z_pole_x, f_z_pole_y) as f64;
            } else {
                // distance from Nyquist
                f_dc_pole_dist_sqr *= calc_2d_dist_sqr(-1.0, 0.0, f_z_pole_x, f_z_pole_y) as f64;
            }
        }
    } else {
        // Odd order - first do the 1st-order section
        let f_s_pole_x = -f_c;
        let f_s_pole_y = 0.0;
        let (f_z_pole_x, _f_z_pole_y) = bilin_transform(f_s_pole_x, f_s_pole_y);
        p_biquad[0].numer[B0] = 1.0;
        if subtype == LOWPASS {
            p_biquad[0].numer[B1] = 1.0;
        } else {
            p_biquad[0].numer[B1] = -1.0;
        }
        p_biquad[0].numer[B2] = 0.0;
        p_biquad[0].denom[A1] = -f_z_pole_x;
        p_biquad[0].denom[A2] = 0.0;
        if subtype == LOWPASS {
            f_dc_pole_dist_sqr = 1.0 - f_z_pole_x;
        } else {
            f_dc_pole_dist_sqr = f_z_pole_x + 1.0; // dist from Nyquist
        }
        for i_pair in 1..=order / 2 {
            let f_s_pole_x = f_c * (PI - i_pair as f64 * PI / order as f64).cos();
            let f_s_pole_y = f_c * (PI - i_pair as f64 * PI / order as f64).sin();
            let (f_z_pole_x, f_z_pole_y) = bilin_transform(f_s_pole_x, f_s_pole_y);
            let i = i_pair as usize;
            p_biquad[i].numer[B0] = 1.0;
            if subtype == LOWPASS {
                p_biquad[i].numer[B1] = 2.0;
            } else {
                p_biquad[i].numer[B1] = -2.0;
            }
            p_biquad[i].numer[B2] = 1.0;
            p_biquad[i].denom[A1] = -2.0 * f_z_pole_x;
            p_biquad[i].denom[A2] = square(f_z_pole_x) + square(f_z_pole_y);
            if subtype == LOWPASS {
                f_dc_pole_dist_sqr *= calc_2d_dist_sqr(1.0, 0.0, f_z_pole_x, f_z_pole_y) as f64;
            } else {
                // distance from Nyquist
                f_dc_pole_dist_sqr *= calc_2d_dist_sqr(-1.0, 0.0, f_z_pole_x, f_z_pole_y) as f64;
            }
        }
    }

    // mult by DC dist from poles, divide by dist from zeroes
    let scale = f_dc_pole_dist_sqr / ((1i32 << order) as f64);
    p_biquad[0].numer[B0] *= scale;
    p_biquad[0].numer[B1] *= scale;
    p_biquad[0].numer[B2] *= scale;

    p_biquad
}

/// Design a Chebyshev type I filter.
///
/// * `ripple` - passband ripple in dB
pub fn calc_chebyshev_type1_filter(
    order: i32,
    fn_: f64,
    fc: f64,
    ripple: f64,
    subtype: i32,
) -> Vec<Biquad> {
    if order < MIN_ORDER {
        return Vec::new();
    }
    let count = ((order + 1) / 2) as usize;
    let mut p_biquad = vec![Biquad::new(); count];

    // Set up the coefficients in all the biquads
    let mut f_norm = fc / fn_;
    if f_norm >= 0.9999 {
        f_norm = 0.9999f32 as f64;
    }
    let f_c = (PI * f_norm / 2.0).tan();
    let beta = (f_norm * PI).cos();

    let eps = (10f64.powf(wx_max(0.001, ripple) / 10.0) - 1.0).sqrt();
    let a = (1.0 / eps + (1.0 / square(eps) + 1.0).sqrt()).ln() / order as f64;

    // Assume even order to start
    for i_pair in 0..order / 2 {
        let f_s_pole_x =
            -f_c * a.sinh() * ((2 * i_pair + 1) as f64 * PI / (2 * order) as f64).sin();
        let f_s_pole_y =
            f_c * a.cosh() * ((2 * i_pair + 1) as f64 * PI / (2 * order) as f64).cos();
        let (mut f_z_pole_x, mut f_z_pole_y) = bilin_transform(f_s_pole_x, f_s_pole_y);
        let f_z_zero_x;
        let f_dc;
        if subtype == LOWPASS {
            f_z_zero_x = -1.0;
            f_dc = calc_2d_dist_sqr(1.0, 0.0, f_z_pole_x, f_z_pole_y) as f64 / (2.0 * 2.0);
        } else {
            // Highpass - do the digital LP->HP transform on the poles and zeroes
            let (zx, zy) = complex_div(
                beta - f_z_pole_x,
                -f_z_pole_y,
                1.0 - beta * f_z_pole_x,
                -beta * f_z_pole_y,
            );
            f_z_pole_x = zx;
            f_z_pole_y = zy;
            f_z_zero_x = 1.0;
            f_dc = calc_2d_dist_sqr(-1.0, 0.0, f_z_pole_x, f_z_pole_y) as f64 / (2.0 * 2.0);
        }
        let i = i_pair as usize;
        p_biquad[i].numer[B0] = f_dc;
        p_biquad[i].numer[B1] = -2.0 * f_z_zero_x * f_dc;
        p_biquad[i].numer[B2] = f_dc;
        p_biquad[i].denom[A1] = -2.0 * f_z_pole_x;
        p_biquad[i].denom[A2] = square(f_z_pole_x) + square(f_z_pole_y);
    }

    if (order & 1) == 0 {
        // at DC the response is down R dB (for even-order)
        let f_temp = db_to_linear(-wx_max(0.001, ripple));
        p_biquad[0].numer[B0] *= f_temp;
        p_biquad[0].numer[B1] *= f_temp;
        p_biquad[0].numer[B2] *= f_temp;
    } else {
        // Odd order - now do the 1st-order section
        let f_s_pole_x = -f_c * a.sinh();
        let f_s_pole_y = 0.0;
        let (mut f_z_pole_x, mut f_z_pole_y) = bilin_transform(f_s_pole_x, f_s_pole_y);
        let f_z_zero_x;
        let f_dc;
        if subtype == LOWPASS {
            f_z_zero_x = -1.0;
            f_dc = f32::sqrt(calc_2d_dist_sqr(1.0, 0.0, f_z_pole_x, f_z_pole_y)) as f64 / 2.0;
        } else {
            // Highpass - do the digital LP->HP transform on the poles and zeroes
            let (zx, zy) = complex_div(
                beta - f_z_pole_x,
                -f_z_pole_y,
                1.0 - beta * f_z_pole_x,
                -beta * f_z_pole_y,
            );
            f_z_pole_x = zx;
            f_z_pole_y = zy;
            f_z_zero_x = 1.0;
            f_dc = f32::sqrt(calc_2d_dist_sqr(-1.0, 0.0, f_z_pole_x, f_z_pole_y)) as f64 / 2.0;
        }
        let idx = ((order - 1) / 2) as usize;
        p_biquad[idx].numer[B0] = f_dc;
        p_biquad[idx].numer[B1] = -f_z_zero_x * f_dc;
        p_biquad[idx].numer[B2] = 0.0;
        p_biquad[idx].denom[A1] = -f_z_pole_x;
        p_biquad[idx].denom[A2] = 0.0;
    }

    p_biquad
}

/// Design a Chebyshev type II filter.
///
/// * `ripple` - stopband ripple in dB
pub fn calc_chebyshev_type2_filter(
    order: i32,
    fn_: f64,
    fc: f64,
    ripple: f64,
    subtype: i32,
) -> Vec<Biquad> {
    if order < MIN_ORDER {
        return Vec::new();
    }
    let count = ((order + 1) / 2) as usize;
    let mut p_biquad = vec![Biquad::new(); count];

    // Set up the coefficients in all the biquads
    let mut f_norm = fc / fn_;
    if f_norm >= 0.9999 {
        f_norm = 0.9999f32 as f64;
    }
    let f_c = (PI * f_norm / 2.0).tan();
    let beta = (f_norm * PI).cos();

    let eps = db_to_linear(-wx_max(0.001, ripple));
    let a = (1.0 / eps + (1.0 / square(eps) + 1.0).sqrt()).ln() / order as f64;

    // Assume even order
    for i_pair in 0..order / 2 {
        let (f_s_pole_x, f_s_pole_y) = complex_div(
            f_c,
            0.0,
            -a.sinh() * ((2 * i_pair + 1) as f64 * PI / (2 * order) as f64).sin(),
            a.cosh() * ((2 * i_pair + 1) as f64 * PI / (2 * order) as f64).cos(),
        );
        let (mut f_z_pole_x, mut f_z_pole_y) = bilin_transform(f_s_pole_x, f_s_pole_y);
        let f_s_zero_x = 0.0;
        let f_s_zero_y = f_c / (((2 * i_pair) as f64 + 1.0) * PI / (2 * order) as f64).cos();
        let (mut f_z_zero_x, mut f_z_zero_y) = bilin_transform(f_s_zero_x, f_s_zero_y);

        let f_dc;
        if subtype == LOWPASS {
            f_dc = calc_2d_dist_sqr(1.0, 0.0, f_z_pole_x, f_z_pole_y) as f64
                / calc_2d_dist_sqr(1.0, 0.0, f_z_zero_x, f_z_zero_y) as f64;
        } else {
            // Highpass - do the digital LP->HP transform on the poles and zeroes
            let (zx, zy) = complex_div(
                beta - f_z_pole_x,
                -f_z_pole_y,
                1.0 - beta * f_z_pole_x,
                -beta * f_z_pole_y,
            );
            f_z_pole_x = zx;
            f_z_pole_y = zy;
            let (zx, zy) = complex_div(
                beta - f_z_zero_x,
                -f_z_zero_y,
                1.0 - beta * f_z_zero_x,
                -beta * f_z_zero_y,
            );
            f_z_zero_x = zx;
            f_z_zero_y = zy;
            // distance from Nyquist
            f_dc = calc_2d_dist_sqr(-1.0, 0.0, f_z_pole_x, f_z_pole_y) as f64
                / calc_2d_dist_sqr(-1.0, 0.0, f_z_zero_x, f_z_zero_y) as f64;
        }
        let i = i_pair as usize;
        p_biquad[i].numer[B0] = f_dc;
        p_biquad[i].numer[B1] = -2.0 * f_z_zero_x * f_dc;
        p_biquad[i].numer[B2] = (square(f_z_zero_x) + square(f_z_zero_y)) * f_dc;
        p_biquad[i].denom[A1] = -2.0 * f_z_pole_x;
        p_biquad[i].denom[A2] = square(f_z_pole_x) + square(f_z_pole_y);
    }

    // Now, if it's odd order, we have one more to do
    if (order & 1) != 0 {
        // we'll do it as a biquad, but it's just first-order
        let i_pair = (order - 1) / 2;
        let (f_s_pole_x, f_s_pole_y) = complex_div(
            f_c,
            0.0,
            -a.sinh() * ((2 * i_pair + 1) as f64 * PI / (2 * order) as f64).sin(),
            a.cosh() * ((2 * i_pair + 1) as f64 * PI / (2 * order) as f64).cos(),
        );
        let (mut f_z_pole_x, mut f_z_pole_y) = bilin_transform(f_s_pole_x, f_s_pole_y);
        // in the s-plane, the zero is at infinity
        let mut f_z_zero_x = -1.0;
        let f_dc;
        if subtype == LOWPASS {
            f_dc = f32::sqrt(calc_2d_dist_sqr(1.0, 0.0, f_z_pole_x, f_z_pole_y)) as f64 / 2.0;
        } else {
            // Highpass - do the digital LP->HP transform on the poles and zeroes
            let (zx, zy) = complex_div(
                beta - f_z_pole_x,
                -f_z_pole_y,
                1.0 - beta * f_z_pole_x,
                -f_z_pole_y,
            );
            f_z_pole_x = zx;
            f_z_pole_y = zy;
            f_z_zero_x = 1.0;
            // distance from Nyquist
            f_dc = f32::sqrt(calc_2d_dist_sqr(-1.0, 0.0, f_z_pole_x, f_z_pole_y)) as f64 / 2.0;
        }
        let idx = i_pair as usize;
        p_biquad[idx].numer[B0] = f_dc;
        p_biquad[idx].numer[B1] = -f_z_zero_x * f_dc;
        p_biquad[idx].numer[B2] = 0.0;
        p_biquad[idx].denom[A1] = -f_z_pole_x;
        p_biquad[idx].denom[A2] = 0.0;
    }

    p_biquad
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_is_pass_through() {
        let mut bq = Biquad::new();
        for x in [0.0f32, 1.0, -1.0, 0.5, -0.25] {
            assert_eq!(bq.process_one(x), x);
        }
    }

    #[test]
    fn reset_clears_state() {
        let mut bq = Biquad::new();
        bq.process_one(1.0);
        bq.reset();
        assert_eq!(bq.prev_in, 0.0);
        assert_eq!(bq.prev_out, 0.0);
    }

    #[test]
    fn design_section_counts() {
        for order in MIN_ORDER..=MAX_ORDER {
            let n = ((order + 1) / 2) as usize;
            assert_eq!(
                calc_butterworth_filter(order, 22050.0, 1000.0, LOWPASS).len(),
                n
            );
            assert_eq!(
                calc_chebyshev_type1_filter(order, 22050.0, 1000.0, 3.0, HIGHPASS).len(),
                n
            );
            assert_eq!(
                calc_chebyshev_type2_filter(order, 22050.0, 1000.0, 20.0, LOWPASS).len(),
                n
            );
        }
    }

    #[test]
    fn butterworth_lowpass_dc_gain_is_unity() {
        // A low-pass Butterworth filter has unity gain at DC: the
        // cascade's response at DC (sum of the numerator coefficients
        // over the sum of the denominator coefficients, A0 == 1) is 1.
        // The margin has to allow for the design's deliberate f32
        // quantization (Calc2D_DistSqr returns a float).
        for order in MIN_ORDER..=MAX_ORDER {
            let secs = calc_butterworth_filter(order, 22050.0, 1000.0, LOWPASS);
            let mut gain = 1.0;
            for s in &secs {
                gain *= (s.numer[B0] + s.numer[B1] + s.numer[B2])
                    / (1.0 + s.denom[A1] + s.denom[A2]);
            }
            assert!(
                (gain - 1.0).abs() < 1e-6,
                "order {}: DC gain {}",
                order,
                gain
            );
        }
    }
}
