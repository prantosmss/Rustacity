/**********************************************************************

Rustacity: A Digital Audio Editor

EBUR128.h

Max Maisel

***********************************************************************/

#ifndef __EBUR128_H__
#define __EBUR128_H__

#include "Biquad.h"
#include "MemoryX.h"
#include "SampleFormat.h"

/// The EBU R128 measurement lives in the Rust DSP core (rust/dsp); this
/// is an opaque handle to it.  See rust/dsp/include/rustacity_dsp.h for
/// the C ABI.
struct rustacity_ebur128;

/// \brief Implements EBU-R128 loudness measurement.
class EBUR128
{
public:
   EBUR128(double rate, size_t channels);
   EBUR128(const EBUR128&) = delete;
   EBUR128(EBUR128&&) = delete;
   ~EBUR128();

   static ArrayOf<Biquad> CalcWeightingFilter(double fs);
   void Initialize();
   void ProcessSampleFromChannel(float x_in, size_t channel);

   /// Process `len` samples of `channelCount` channels at once; this is
   /// equivalent to ProcessSampleFromChannel() for every channel of
   /// every sample followed by NextSample().
   void ProcessBlock(const float* const* channels, size_t channelCount,
                     size_t len);

   void NextSample();
   double IntegrativeLoudness();
   inline double IntegrativeLoudnessToLUFS(double loudness)
      { return 10 * log10(loudness); }

private:
   rustacity_ebur128* mHandle;
};

#endif
