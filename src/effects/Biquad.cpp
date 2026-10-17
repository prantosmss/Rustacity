/**********************************************************************

Rustacity: A Digital Audio Editor

Biquad.cpp

Norm C
Max Maisel

***********************************************************************/

#include "Biquad.h"
#include "RustDsp.h"
#include "rustacity_dsp.h"

#include <cstddef>
#include <utility>
#include <vector>

// The filter design math and the sample processing live in the Rust core
// (rust/dsp, crate "rustacity-dsp"); this file is the C++ side of the C
// ABI declared in rust/dsp/include/rustacity_dsp.h.

namespace
{

// The buffer handed to the Rust design functions has to hold the
// (order + 1) / 2 sections of the cascade.
std::vector<rustacity_biquad_t> SectionBuffer(int order)
{
   const size_t count = order > 0 ? size_t((order + 1) / 2) : 0;
   return std::vector<rustacity_biquad_t>(count);
}

// Copy the `written` designed sections into the result array.
ArrayOf<Biquad> SectionArray(
   const std::vector<rustacity_biquad_t> &sections, int written)
{
   const size_t count = sections.size();
   ArrayOf<Biquad> pBiquad(count, true);
   for (int i = 0; i < written && size_t(i) < count; ++i)
      RustDsp::CopyFromRust(pBiquad[i], sections[i]);
   return std::move(pBiquad);
}

} // namespace

Biquad::Biquad()
{
   fNumerCoeffs[B0] = 1;
   fNumerCoeffs[B1] = 0;
   fNumerCoeffs[B2] = 0;
   fDenomCoeffs[A1] = 0;
   fDenomCoeffs[A2] = 0;
   Reset();
}

void Biquad::Reset()
{
   fPrevIn = 0;
   fPrevPrevIn = 0;
   fPrevOut = 0;
   fPrevPrevOut = 0;
}

void Biquad::Process(float* pfIn, float* pfOut, int iNumSamples)
{
   rustacity_biquad_t state;
   RustDsp::CopyToRust(*this, state);
   rustacity_biquad_process(&state, pfIn, pfOut, iNumSamples);
   RustDsp::CopyFromRust(*this, state);
}

ArrayOf<Biquad> Biquad::CalcButterworthFilter(int order, double fn, double fc, int type)
{
   auto sections = SectionBuffer(order);
   const int written = rustacity_biquad_design_butterworth(
      sections.data(), sections.size(), order, fn, fc, type);
   return SectionArray(sections, written);
}

ArrayOf<Biquad> Biquad::CalcChebyshevType1Filter(
   int order, double fn, double fc, double ripple, int type)
{
   auto sections = SectionBuffer(order);
   const int written = rustacity_biquad_design_chebyshev1(
      sections.data(), sections.size(), order, fn, fc, ripple, type);
   return SectionArray(sections, written);
}

ArrayOf<Biquad> Biquad::CalcChebyshevType2Filter(
   int order, double fn, double fc, double ripple, int type)
{
   auto sections = SectionBuffer(order);
   const int written = rustacity_biquad_design_chebyshev2(
      sections.data(), sections.size(), order, fn, fc, ripple, type);
   return SectionArray(sections, written);
}

double Biquad::ChebyPoly(int Order, double NormFreq)
{
   return rustacity_biquad_cheby_poly(Order, NormFreq);
}
