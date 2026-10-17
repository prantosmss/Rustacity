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

double rustacity_biquad_cheby_poly(int order, double norm_freq);

/* --- EffectBassTreble's shelf filters --------------------------------- */

/* Opaque state of one channel: a low shelf at 250 Hz, a high shelf at
 * 4 kHz and the effect's output gain (the C++ EffectBassTrebleState).  The
 * C++ side keeps one handle for the master and one per realtime group. */
typedef struct rustacity_bass_treble rustacity_bass_treble_t;

/* `sample_rate` is narrowed to float, like the C++ Init() took a float. */
rustacity_bass_treble_t *rustacity_bass_treble_new(float sample_rate);
void rustacity_bass_treble_free(rustacity_bass_treble_t *handle);

/* Filter `len` samples of one channel from `input` to `output`, which may
 * alias, with the effect's three parameters for this block.  Returns `len`,
 * or 0 if the handle or a pointer is unusable.  Like the original
 * Process(), every call designs the coefficients again. */
size_t rustacity_bass_treble_process(rustacity_bass_treble_t *handle,
                                     double bass_db, double treble_db,
                                     double gain_db,
                                     const float *input, float *output,
                                     size_t len);

/* --- EBU R128 loudness measurement ---------------------------------- */

/* Opaque loudness meter; `rate` is the sample rate in Hz and
 * `channels` the number of channels (1 for mono, 2 for stereo). */
typedef struct rustacity_ebur128 rustacity_ebur128_t;

rustacity_ebur128_t *rustacity_ebur128_new(double rate, size_t channels);
void rustacity_ebur128_free(rustacity_ebur128_t *meter);

void rustacity_ebur128_initialize(rustacity_ebur128_t *meter);
void rustacity_ebur128_process_sample(rustacity_ebur128_t *meter, float x,
                                      size_t channel);

/* `channels[i]` is a buffer of `len` samples; `channels[0]` is
 * processed first, exactly like ProcessSampleFromChannel() per channel
 * followed by NextSample(). */
void rustacity_ebur128_process_block(rustacity_ebur128_t *meter,
                                     const float *const *channels,
                                     size_t channel_count, size_t len);

void rustacity_ebur128_next_sample(rustacity_ebur128_t *meter);
double rustacity_ebur128_integrative_loudness(rustacity_ebur128_t *meter);

/* The K-weighting filter pair (HSF, HPF) for `fs`, mirroring
 * EBUR128::CalcWeightingFilter(); returns 2. */
int rustacity_ebur128_weighting_filter(double fs, rustacity_biquad_t *out,
                                       size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* __RUSTACITY_DSP_H__ */
