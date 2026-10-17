/**********************************************************************

   Rustacity: A Digital Audio Editor

   WahwahFilter.h

   Effect programming:
   Nasca Octavian Paul (Paul Nasca)

   The signal processing of EffectWahwah, moved out of Wahwah.cpp so that it
   can be compiled on its own: no wxWidgets, no effect framework, nothing but
   <cmath>.  The Rust port of the effect replaces this file - see
   rust/dsp/src/wahwah.rs and rust/dsp/goldens/capture/.

***********************************************************************/

#ifndef __RUSTACITY_WAHWAH_FILTER_H__
#define __RUSTACITY_WAHWAH_FILTER_H__

#include <cmath>

// M_PI is not in <cmath> on every platform the build targets.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/// How many samples are processed before the LFO value is recomputed again
/// (lfoskipsamples in Wahwah.cpp).
const int kWahwahLfoSkipSamples = 30;

/// dB to linear amplitude - DB_TO_LINEAR in Rustacity.h, kept local so that
/// this header compiles on its own.
inline double WahwahDbToLinear(double db)
{
   return pow(10.0, db / 20.0);
}

/// One channel of the effect: the LFO and the state of the wah filter.
struct EffectWahwahState
{
   float samplerate;
   double depth;
   double freqofs;
   double phase;
   double outgain;
   double lfoskip;
   unsigned long skipcount;
   double xn1, xn2, yn1, yn2;
   double b0, b1, b2, a0, a1, a2;

   /// Reset for `sampleRate` - the C++ EffectWahwah::InstanceInit().
   ///
   /// The C++ also derived the LFO step, the depth, the frequency offset, the
   /// start phase and the output gain from the effect's parameters here, but
   /// Process() recomputes every one of them before it reads them, so they are
   /// set there instead.
   void Init(float sampleRate)
   {
      samplerate = sampleRate;

      skipcount = 0;
      xn1 = 0;
      xn2 = 0;
      yn1 = 0;
      yn2 = 0;
      b0 = 0;
      b1 = 0;
      b2 = 0;
      a0 = 0;
      a1 = 0;
      a2 = 0;

      depth = 0;
      freqofs = 0;
      phase = 0;
      outgain = 0;
      lfoskip = 0;
   }

   /// Filter `len` samples of one channel - the C++ InstanceProcess().
   ///
   /// `freq` is the LFO frequency in Hz, `phaseDeg` its start phase in
   /// degrees, `depthPercent` is 0..100, `res` is the resonance (greater than
   /// 0), `freqOffset` is 0..100 and `outGainDb` the output gain in dB.
   size_t Process(double freq, double phaseDeg, int depthPercent, double res,
                  int freqOffset, double outGainDb,
                  const float * in, float * out, size_t len)
   {
      lfoskip = freq * 2 * M_PI / samplerate;
      depth = depthPercent / 100.0;
      freqofs = freqOffset / 100.0;

      phase = phaseDeg * M_PI / 180.0;
      outgain = WahwahDbToLinear(outGainDb);

      for (size_t i = 0; i < len; i++)
      {
         double input = (double) in[i];

         if ((skipcount++) % kWahwahLfoSkipSamples == 0)
         {
            double frequency =
               (1 + cos(skipcount * lfoskip + phase)) / 2;
            frequency = frequency * depth * (1 - freqofs) + freqofs;
            frequency = exp((frequency - 1) * 6);
            double omega = M_PI * frequency;
            double sn = sin(omega);
            double cs = cos(omega);
            double alpha = sn / (2 * res);
            b0 = (1 - cs) / 2;
            b1 = 1 - cs;
            b2 = (1 - cs) / 2;
            a0 = 1 + alpha;
            a1 = -2 * cs;
            a2 = 1 - alpha;
         }

         double output = (b0 * input + b1 * xn1 + b2 * xn2
                          - a1 * yn1 - a2 * yn2) / a0;
         xn2 = xn1;
         xn1 = input;
         yn2 = yn1;
         yn1 = output;
         output *= outgain;

         out[i] = (float) output;
      }

      return len;
   }
};

#endif
