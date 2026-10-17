/**********************************************************************

Rustacity: A Digital Audio Editor

RustDsp.h

Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

Conversions between the C++ DSP types and their Rust counterparts
declared in rust/dsp/include/rustacity_dsp.h.

***********************************************************************/

#ifndef __RUST_DSP_H__
#define __RUST_DSP_H__

#include "Biquad.h"
#include "rustacity_dsp.h"

// The Rust core's rustacity_biquad_t is a #[repr(C)] struct with the
// same nine doubles; assert the C++ side has that layout.  The helpers
// below copy the fields one by one, so no object is ever reinterpreted.
static_assert(sizeof(Biquad) == 9 * sizeof(double),
              "Biquad must stay layout-compatible with rustacity_biquad_t");

namespace RustDsp
{

inline void CopyToRust(const Biquad &src, rustacity_biquad_t &dst)
{
   dst.numer[0] = src.fNumerCoeffs[Biquad::B0];
   dst.numer[1] = src.fNumerCoeffs[Biquad::B1];
   dst.numer[2] = src.fNumerCoeffs[Biquad::B2];
   dst.denom[0] = src.fDenomCoeffs[Biquad::A1];
   dst.denom[1] = src.fDenomCoeffs[Biquad::A2];
   dst.prev_in = src.fPrevIn;
   dst.prev_prev_in = src.fPrevPrevIn;
   dst.prev_out = src.fPrevOut;
   dst.prev_prev_out = src.fPrevPrevOut;
}

inline void CopyFromRust(Biquad &dst, const rustacity_biquad_t &src)
{
   dst.fNumerCoeffs[Biquad::B0] = src.numer[0];
   dst.fNumerCoeffs[Biquad::B1] = src.numer[1];
   dst.fNumerCoeffs[Biquad::B2] = src.numer[2];
   dst.fDenomCoeffs[Biquad::A1] = src.denom[0];
   dst.fDenomCoeffs[Biquad::A2] = src.denom[1];
   dst.fPrevIn = src.prev_in;
   dst.fPrevPrevIn = src.prev_prev_in;
   dst.fPrevOut = src.prev_out;
   dst.fPrevPrevOut = src.prev_prev_out;
}

} // namespace RustDsp

#endif
