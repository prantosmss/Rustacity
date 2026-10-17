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

// The Chebyshev coefficients are no longer read by this file - the
// authoritative copy used by the design functions lives in the Rust
// core (rust/dsp/src/biquad.rs) - but the table keeps its definition
// here because Biquad.h declares it.  The golden-value tests
// (rust/dsp/tests/) compare the two copies through ChebyPoly().
const double Biquad::s_fChebyCoeffs[MAX_Order][MAX_Order + 1] =
{
   // For Chebyshev polynomials of the first kind (see http://en.wikipedia.org/wiki/Chebyshev_polynomial)
   // Coeffs are in the order 0, 1, 2...9
   { 0,  1},        // order 1
   {-1,  0,   2},   // order 2 etc.
   { 0, -3,   0,    4},
   { 1,  0,  -8,    0,    8},
   { 0,  5,   0,  -20,    0,   16},
   {-1,  0,  18,    0,  -48,    0,   32},
   { 0, -7,   0,   56,    0, -112,    0,   64},
   { 1,  0, -32,    0,  160,    0, -256,    0,  128},
   { 0,  9,   0, -120,    0,  432,    0, -576,    0,   256},
   {-1,  0,  50,    0, -400,    0, 1120,    0, -1280,    0, 512}
};

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

void Biquad::ComplexDiv (double fNumerR, double fNumerI, double fDenomR, double fDenomI,
                         double* pfQuotientR, double* pfQuotientI)
{
   rustacity_biquad_complex_div(
      fNumerR, fNumerI, fDenomR, fDenomI, pfQuotientR, pfQuotientI);
}

bool Biquad::BilinTransform (double fSX, double fSY, double* pfZX, double* pfZY)
{
   return rustacity_biquad_bilin_transform(fSX, fSY, pfZX, pfZY) != 0;
}

float Biquad::Calc2D_DistSqr (double fX1, double fY1, double fX2, double fY2)
{
   return rustacity_biquad_dist_sqr(fX1, fY1, fX2, fY2);
}

double Biquad::ChebyPoly(int Order, double NormFreq)
{
   return rustacity_biquad_cheby_poly(Order, NormFreq);
}
