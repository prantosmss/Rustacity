/**********************************************************************

  Rustacity: A Digital Audio Editor

  rustacity_dsp.h

  Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

  C ABI of the Rust DSP core (crate rustacity-dsp under rust/dsp).
  The declarations below mirror the implementations in
  rust/dsp/src/{biquad,ffi}.rs; the #[repr(C)] Rust type and the C
  structs declared here are layout-compatible (both sides assert it).

***********************************************************************/

#ifndef __RUSTACITY_DSP_H__
#define __RUSTACITY_DSP_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Layout mirror of the C++ struct Biquad (src/effects/Biquad.h):
 * 3 numerator coefficients (B0 B1 B2), 2 denominator coefficients
 * (A1 A2, A0 == 1.0) and the 4 sample state variables - 9 doubles. */
typedef struct rustacity_biquad
{
   double numer[3];
   double denom[2];
   double prev_in;
   double prev_prev_in;
   double prev_out;
   double prev_prev_out;
} rustacity_biquad_t;

/* Filter subtypes, mirroring Biquad::kSubTypes. */
#define RUSTACITY_BIQUAD_LOWPASS 0
#define RUSTACITY_BIQUAD_HIGHPASS 1

/* Supported filter orders (Biquad::MIN_Order / MAX_Order). */
#define RUSTACITY_BIQUAD_MIN_ORDER 1
#define RUSTACITY_BIQUAD_MAX_ORDER 10

void rustacity_biquad_reset(rustacity_biquad_t *bq);

/* `input` and `output` may alias. */
void rustacity_biquad_process(rustacity_biquad_t *bq, const float *input,
                              float *output, int num_samples);

float rustacity_biquad_process_one(rustacity_biquad_t *bq, float x);

/* The design functions fill `out[0..return)` with the (order+1)/2
 * cascade sections and return the number of sections written, or 0 if
 * the order is invalid.  `cap` is the capacity of `out` in entries. */
int rustacity_biquad_design_butterworth(rustacity_biquad_t *out, size_t cap,
                                        int order, double fn_hz, double fc,
                                        int subtype);
int rustacity_biquad_design_chebyshev1(rustacity_biquad_t *out, size_t cap,
                                       int order, double fn_hz, double fc,
                                       double ripple, int subtype);
int rustacity_biquad_design_chebyshev2(rustacity_biquad_t *out, size_t cap,
                                       int order, double fn_hz, double fc,
                                       double ripple, int subtype);

void rustacity_biquad_complex_div(double numer_r, double numer_i,
                                  double denom_r, double denom_i,
                                  double *quotient_r, double *quotient_i);

/* Always returns 1 (the C++ function always returns true). */
int rustacity_biquad_bilin_transform(double sx, double sy, double *zx,
                                     double *zy);

/* Note: single precision, exactly like the C++ implementation. */
float rustacity_biquad_dist_sqr(double x1, double y1, double x2, double y2);

double rustacity_biquad_cheby_poly(int order, double norm_freq);

#ifdef __cplusplus
}
#endif

#endif /* __RUSTACITY_DSP_H__ */
