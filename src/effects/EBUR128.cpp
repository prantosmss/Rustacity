/**********************************************************************

Rustacity: A Digital Audio Editor

EBUR128.cpp

Max Maisel

***********************************************************************/

#include "EBUR128.h"
#include "RustDsp.h"
#include "rustacity_dsp.h"

// The loudness measurement itself lives in the Rust core (rust/dsp,
// crate "rustacity-dsp"); this file is the C++ side of the C ABI
// declared in rust/dsp/include/rustacity_dsp.h.

EBUR128::EBUR128(double rate, size_t channels)
   : mHandle(rustacity_ebur128_new(rate, channels))
{
}

EBUR128::~EBUR128()
{
   rustacity_ebur128_free(mHandle);
}

void EBUR128::Initialize()
{
   rustacity_ebur128_initialize(mHandle);
}

// fs: sample rate
// returns array of two Biquads
//
// EBU R128 parameter sampling rate adaption after
// Mansbridge, Stuart, Saoirse Finn, and Joshua D. Reiss.
// "Implementation and Evaluation of Autonomous Multi-track Fader Control."
// Paper presented at the 132nd Audio Engineering Society Convention,
// Budapest, Hungary, 2012."
ArrayOf<Biquad> EBUR128::CalcWeightingFilter(double fs)
{
   ArrayOf<Biquad> pBiquad(size_t(2), true);

   // The HSF (0) and HPF (1) sections are designed by the Rust core.
   rustacity_biquad_t sections[2];
   const int written = rustacity_ebur128_weighting_filter(fs, sections, 2);
   for (int i = 0; i < written && i < 2; ++i)
      RustDsp::CopyFromRust(pBiquad[i], sections[i]);

   return std::move(pBiquad);
}

void EBUR128::ProcessSampleFromChannel(float x_in, size_t channel)
{
   rustacity_ebur128_process_sample(mHandle, x_in, channel);
}

void EBUR128::ProcessBlock(
   const float* const* channels, size_t channelCount, size_t len)
{
   rustacity_ebur128_process_block(mHandle, channels, channelCount, len);
}

void EBUR128::NextSample()
{
   rustacity_ebur128_next_sample(mHandle);
}

double EBUR128::IntegrativeLoudness()
{
   return rustacity_ebur128_integrative_loudness(mHandle);
}
