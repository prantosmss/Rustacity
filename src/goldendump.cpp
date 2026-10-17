/**********************************************************************

  Rustacity: A Digital Audio Editor

  goldendump.cpp

  Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

  TEMPORARY golden-value capture harness for the Rust DSP port.

  Prints one "GOLDEN ..." line per reference record produced by the
  ORIGINAL C++ Biquad / EBU R128 implementation.  The captured output
  is committed to rust/dsp/goldens/reference.txt and asserted by the
  Rust test suite in rust/dsp/tests/golden.rs.

  This file, its CMake target and the CI step that runs it are removed
  once the goldens are checked in.

  The noise generator below (wrapping u32 LCG; a sample is
  state / 2^32 - 1 rounded to float) is mirrored exactly by the Rust
  tests - keep both in sync.

***********************************************************************/

#include "effects/Biquad.h"
#include "effects/EBUR128.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

// ---------------------------------------------------------------------
// Deterministic input generator (mirrored byte-for-byte by the Rust
// tests).
// ---------------------------------------------------------------------
static uint32_t s_state = 0;

static void SeedNoise(uint32_t seed)
{
   s_state = seed;
}

static float NextNoise()
{
   s_state = s_state * 1664525u + 1013904223u;
   return float(double(s_state) / 4294967296.0 - 1.0);
}

// ---------------------------------------------------------------------
// Filter design records: the full section coefficient set.
// ---------------------------------------------------------------------
static void DumpDesign(const char *kind, int order, double fn, double fc,
                       double ripple, int subtype)
{
   ArrayOf<Biquad> secs;
   if (std::strcmp(kind, "butter") == 0)
      secs = Biquad::CalcButterworthFilter(order, fn, fc, subtype);
   else if (std::strcmp(kind, "cheby1") == 0)
      secs = Biquad::CalcChebyshevType1Filter(order, fn, fc, ripple, subtype);
   else
      secs = Biquad::CalcChebyshevType2Filter(order, fn, fc, ripple, subtype);

   const int n = (order + 1) / 2;
   std::printf("GOLDEN design %s %d %.17g %.17g %.17g %d %d",
               kind, order, fn, fc, ripple, subtype, n);
   for (int i = 0; i < n; ++i)
      std::printf(" %.17g %.17g %.17g %.17g %.17g",
                  secs[i].fNumerCoeffs[Biquad::B0],
                  secs[i].fNumerCoeffs[Biquad::B1],
                  secs[i].fNumerCoeffs[Biquad::B2],
                  secs[i].fDenomCoeffs[Biquad::A1],
                  secs[i].fDenomCoeffs[Biquad::A2]);
   std::printf("\n");
}

// ---------------------------------------------------------------------
// Block processing through the section cascade (what the Scientific
// Filter effect does per audio block).
// ---------------------------------------------------------------------
static void DumpProcessDesign(const char *kind, int order, double fn, double fc,
                              double ripple, int subtype)
{
   ArrayOf<Biquad> secs;
   if (std::strcmp(kind, "butter") == 0)
      secs = Biquad::CalcButterworthFilter(order, fn, fc, subtype);
   else if (std::strcmp(kind, "cheby1") == 0)
      secs = Biquad::CalcChebyshevType1Filter(order, fn, fc, ripple, subtype);
   else
      secs = Biquad::CalcChebyshevType2Filter(order, fn, fc, ripple, subtype);

   const int n = (order + 1) / 2;
   const size_t len = 64;
   SeedNoise(20261017u);
   std::vector<float> buf(len), tmp(len);
   for (size_t i = 0; i < len; ++i)
      buf[i] = NextNoise();
   for (int i = 0; i < n; ++i)
   {
      secs[i].Process(buf.data(), tmp.data(), int(len));
      buf.swap(tmp);
   }

   std::printf("GOLDEN process-design %s %d %.17g %.17g %.17g %d %zu",
               kind, order, fn, fc, ripple, subtype, len);
   for (size_t i = 0; i < len; ++i)
      std::printf(" %.17g", double(buf[i]));
   std::printf("\n");
}

// ---------------------------------------------------------------------
// EBU R128 weighting filter pair, driven exactly like EBUR128 drives
// it (double state chain with float round trips through ProcessOne).
// ---------------------------------------------------------------------
static void DumpProcessWeighting(double fs)
{
   ArrayOf<Biquad> w = EBUR128::CalcWeightingFilter(fs);
   const size_t len = 64;
   SeedNoise(20261017u);
   std::printf("GOLDEN process-weighting %.17g %zu", fs, len);
   for (size_t i = 0; i < len; ++i)
   {
      const float x = NextNoise();
      double v = w[0].ProcessOne(x);
      v = w[1].ProcessOne(v);
      std::printf(" %.17g", v);
   }
   std::printf("\n");
}

// ---------------------------------------------------------------------
// End-to-end loudness measurement.
// ---------------------------------------------------------------------
static void DumpEbur128(double rate, size_t channels, size_t nsamples,
                        uint32_t seed, bool silent)
{
   EBUR128 meter(rate, channels);
   meter.Initialize();
   SeedNoise(seed);
   for (size_t i = 0; i < nsamples; ++i)
   {
      meter.ProcessSampleFromChannel(silent ? 0.0f : NextNoise(), 0);
      if (channels > 1)
         meter.ProcessSampleFromChannel(silent ? 0.0f : NextNoise(), 1);
      meter.NextSample();
   }
   std::printf("GOLDEN ebur128 %.17g %zu %zu %u %s %.17g\n",
               rate, channels, nsamples, seed,
               silent ? "silent" : "noise", meter.IntegrativeLoudness());
}

int main()
{
   static const double kFc[3]       = { 100.0, 1000.0, 8000.0 };
   static const double kFc2[2]      = { 100.0, 8000.0 };
   static const double kFn[3]       = { 8000.0, 24000.0, 48000.0 };
   static const double kRipple1[3]  = { 1.0, 3.0, 6.0 };
   static const double kRipple2[2]  = { 10.0, 20.0 };
   static const double kPolyX[5]    = { 0.25, 0.5, 1.0, 2.0, 4.0 };
   static const int    kSweep[3]    = { 2, 5, 8 };

   // Butterworth: every supported order x cutoff x subtype
   for (int order = Biquad::MIN_Order; order <= Biquad::MAX_Order; ++order)
      for (int subtype = Biquad::kLowPass; subtype < Biquad::nSubTypes; ++subtype)
         for (double fc : kFc)
            DumpDesign("butter", order, 22050.0, fc, 0.0, subtype);

   // Butterworth: Nyquist frequency sweep
   for (double fn : kFn)
      DumpDesign("butter", 4, fn, 1000.0, 0.0, Biquad::kLowPass);

   // Chebyshev I: ripple sweep at fc=1000, cutoff sweep at ripple=3
   for (int order = Biquad::MIN_Order; order <= Biquad::MAX_Order; ++order)
      for (int subtype = Biquad::kLowPass; subtype < Biquad::nSubTypes; ++subtype)
         for (double ripple : kRipple1)
            DumpDesign("cheby1", order, 22050.0, 1000.0, ripple, subtype);
   for (int order : kSweep)
      for (int subtype = Biquad::kLowPass; subtype < Biquad::nSubTypes; ++subtype)
         for (double fc : kFc2)
            DumpDesign("cheby1", order, 22050.0, fc, 3.0, subtype);

   // Chebyshev II: stopband ripple sweep, cutoff sweep
   for (int order = Biquad::MIN_Order; order <= Biquad::MAX_Order; ++order)
      for (int subtype = Biquad::kLowPass; subtype < Biquad::nSubTypes; ++subtype)
         for (double ripple : kRipple2)
            DumpDesign("cheby2", order, 22050.0, 1000.0, ripple, subtype);
   for (int order : kSweep)
      for (int subtype = Biquad::kLowPass; subtype < Biquad::nSubTypes; ++subtype)
         for (double fc : kFc2)
            DumpDesign("cheby2", order, 22050.0, fc, 20.0, subtype);

   // Chebyshev polynomial values (Scientific Filter response curve)
   for (int order = Biquad::MIN_Order; order <= Biquad::MAX_Order; ++order)
      for (double x : kPolyX)
         std::printf("GOLDEN chebypoly %d %.17g %.17g\n",
                     order, x, Biquad::ChebyPoly(order, x));

   // Block processing through the designed cascades
   DumpProcessDesign("butter", 4, 22050.0, 1000.0, 0.0, Biquad::kLowPass);
   DumpProcessDesign("butter", 5, 22050.0, 5000.0, 0.0, Biquad::kHighPass);
   DumpProcessDesign("butter", 1, 22050.0, 3000.0, 0.0, Biquad::kHighPass);
   DumpProcessDesign("cheby1", 6, 22050.0, 2000.0, 3.0, Biquad::kLowPass);
   DumpProcessDesign("cheby2", 3, 22050.0, 800.0, 20.0, Biquad::kHighPass);

   // EBU R128 K-weighting pair
   DumpProcessWeighting(48000.0);
   DumpProcessWeighting(44100.0);

   // End-to-end loudness: noise (mono/stereo, several rates and lengths,
   // incomplete-block and exactly-one-block edge cases) plus silence
   DumpEbur128(44100.0, 1, 44100, 20261017u, false);
   DumpEbur128(44100.0, 2, 88200, 20261017u, false);
   DumpEbur128(48000.0, 2, 96000, 20261017u, false);
   DumpEbur128(96000.0, 2, 192000, 20261017u, false);
   DumpEbur128(48000.0, 1, 12000, 20261017u, false);
   DumpEbur128(48000.0, 2, 19200, 20261017u, false);
   DumpEbur128(48000.0, 2, 48000, 20261017u, true);

   return 0;
}
