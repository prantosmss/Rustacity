/**********************************************************************

   Rustacity: A Digital Audio Editor

   PhaserFilter.h

   Effect programming:
   Nasca Octavian Paul (Paul Nasca)

   The signal processing of EffectPhaser, moved out of Phaser.cpp so that it
   can be compiled on its own: no wxWidgets, no effect framework, nothing but
   <cmath>.  The Rust port of the effect replaces this file - see
   rust/dsp/src/phaser.rs and rust/dsp/goldens/capture/.

***********************************************************************/

#ifndef __RUSTACITY_PHASER_FILTER_H__
#define __RUSTACITY_PHASER_FILTER_H__

#include <cmath>

// M_PI is not in <cmath> on every platform the build targets.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/// The most stages a phaser can have; NUM_STAGES in Phaser.h asserts that the
/// two agree.
const int kPhaserMaxStages = 24;

// How many samples are processed before the LFO value is recomputed again.
const int kPhaserLfoSkipSamples = 20;

/// The shape of the LFO (phaserlfoshape in Phaser.cpp).
const double kPhaserLfoShape = 4.0;

/// dB to linear amplitude - DB_TO_LINEAR in Rustacity.h, kept local so that
/// this header compiles on its own.
inline double PhaserDbToLinear(double db)
{
   return pow(10.0, db / 20.0);
}

/// One channel of the effect: the LFO and the state of the allpass stages.
struct EffectPhaserState
{
   float samplerate;
   /// The C++ used `sampleCount` here, whose as_double() is this conversion.
   long long skipcount;
   /// The stage state; the array is as large as a phaser can be.
   double old[kPhaserMaxStages];
   double gain;
   double fbout;
   double outgain;
   double lfoskip;
   double phase;
   int laststages;

   /// Reset for `sampleRate` - the C++ EffectPhaser::InstanceInit().
   ///
   /// The stage state is cleared in full, where the C++ cleared only the
   /// stages the effect currently had: the stages above that are cleared
   /// again before they are read, when the stage count grows.
   void Init(float sampleRate)
   {
      samplerate = sampleRate;

      for (int j = 0; j < kPhaserMaxStages; j++)
         old[j] = 0;

      skipcount = 0;
      gain = 0;
      fbout = 0;
      laststages = 0;
      outgain = 0;
      lfoskip = 0;
      // The C++ left this one unset until the first block, which is also when
      // it overwrote the start phase the caller had just offset.
      phase = 0;
   }

   /// Filter `len` samples of one channel - the C++ InstanceProcess().
   ///
   /// `stages` is the number of allpass stages, `dryWet` is 0..255 (255 is
   /// fully wet), `freq` is the LFO frequency in Hz, `phaseDeg` its start
   /// phase in degrees, `depth` is 0..255, `feedback` is -100..100 (percent)
   /// and `outGainDb` the output gain in dB.
   size_t Process(int stages, int dryWet, double freq, double phaseDeg,
                  int depth, int feedback, double outGainDb,
                  const float * in, float * out, size_t len)
   {
      for (int j = laststages; j < stages; j++)
         old[j] = 0;
      laststages = stages;

      lfoskip = freq * 2 * M_PI / samplerate;
      phase = phaseDeg * M_PI / 180;
      outgain = PhaserDbToLinear(outGainDb);

      for (size_t i = 0; i < len; i++)
      {
         double input = in[i];

         double m = input + fbout * feedback / 101;  // Feedback must be less than 100% to avoid infinite gain.

         if (((skipcount++) % kPhaserLfoSkipSamples) == 0)
         {
            //compute sine between 0 and 1
            gain = (1.0 + cos((double) skipcount * lfoskip + phase)) / 2.0;

            // change lfo shape
            gain = expm1(gain * kPhaserLfoShape) / expm1(kPhaserLfoShape);

            // attenuate the lfo
            gain = 1.0 - gain / 255.0 * depth;
         }

         // phasing routine
         for (int j = 0; j < stages; j++)
         {
            double tmp = old[j];
            old[j] = gain * tmp + m;
            m = tmp - gain * old[j];
         }
         fbout = m;

         out[i] = (float) (outgain * (m * dryWet + input * (255 - dryWet)) / 255);
      }

      return len;
   }
};

#endif
