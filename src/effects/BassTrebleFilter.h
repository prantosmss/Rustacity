/**********************************************************************

   Rustacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2016 Audacity Team.
   License: GPL v2.  See License.txt.

   BassTrebleFilter.h (the two shelf filters of EffectBassTreble)
   Steve Daulton

**********************************************************************/

#ifndef __RUSTACITY_BASS_TREBLE_FILTER__
#define __RUSTACITY_BASS_TREBLE_FILTER__

#include <cmath>
#include <cstddef>

// M_PI is not standard C++ (Distortion.cpp defines it the same way).
#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif

/// \brief The low shelf, the high shelf and the output gain of
/// EffectBassTreble.
///
/// This is the effect's signal processing, kept free of wxWidgets and of the
/// effect framework: rust/dsp/goldens/capture/ includes this header on its own
/// to capture the values the Rust port has to reproduce, and the port then
/// replaces the whole class.
class EffectBassTrebleState
{
public:
   /// Which shelf Coefficents() designs.  EffectBassTreble keeps its own
   /// file-scope kShelfType with the same two values for its gain link.
   enum
   {
      kBass,
      kTreble
   };

   /// Set the sample rate and reset the filter state.
   void Init(float sampleRate, double gainDb);

   /// Filter one block of one channel, `out` possibly aliasing `in`.  The
   /// three parameters are the effect's parameter values for this block.
   size_t Process(double bassDb, double trebleDb, double gainDb,
                  const float *in, float *out, size_t len);

   /// Design one shelf.  Public only so that the goldens capture tool can
   /// dump the coefficients.
   void Coefficents(double hz, double slope, double gain, double samplerate,
                    int type,
                    double& a0, double& a1, double& a2,
                    double& b0, double& b1, double& b2);

   float samplerate;
   double treble;
   double bass;
   double gain;
   double slope, hzBass, hzTreble;
   double a0Bass, a1Bass, a2Bass, b0Bass, b1Bass, b2Bass;
   double a0Treble, a1Treble, a2Treble, b0Treble, b1Treble, b2Treble;
   double xn1Bass, xn2Bass, yn1Bass, yn2Bass;
   double xn1Treble, xn2Treble, yn1Treble, yn2Treble;

private:
   float DoFilter(float in);
};

/// `DB_TO_LINEAR` from Rustacity.h, repeated here so that this file needs no
/// wxWidgets.
inline double BassTrebleDbToLinear(double x)
{
   return std::pow(10.0, x / 20.0);
}

inline void EffectBassTrebleState::Init(float sampleRate, double gainDb)
{
   samplerate = sampleRate;
   slope = 0.4f;   // same slope for both filters
   hzBass = 250.0f;   // could be tunable in a more advanced version
   hzTreble = 4000.0f;   // could be tunable in a more advanced version

   a0Bass = 1;
   a1Bass = 0;
   a2Bass = 0;
   b0Bass = 0;
   b1Bass = 0;
   b2Bass = 0;

   a0Treble = 1;
   a1Treble = 0;
   a2Treble = 0;
   b0Treble = 0;
   b1Treble = 0;
   b2Treble = 0;

   xn1Bass = 0;
   xn2Bass = 0;
   yn1Bass = 0;
   yn2Bass = 0;

   xn1Treble = 0;
   xn2Treble = 0;
   yn1Treble = 0;
   yn2Treble = 0;

   bass = -1;
   treble = -1;
   gain = BassTrebleDbToLinear(gainDb);
}

inline size_t EffectBassTrebleState::Process(double bassDb, double trebleDb, double gainDb,
                                             const float *in, float *out, size_t len)
{
   // Set value to ensure correct rounding
   const double oldBass = BassTrebleDbToLinear(bassDb);
   const double oldTreble = BassTrebleDbToLinear(trebleDb);

   gain = BassTrebleDbToLinear(gainDb);

   // Compute coefficients of the low shelf biquand IIR filter
   if (bass != oldBass)
      Coefficents(hzBass, slope, bassDb, samplerate, kBass,
                  a0Bass, a1Bass, a2Bass, b0Bass, b1Bass, b2Bass);

   // Compute coefficients of the high shelf biquand IIR filter
   if (treble != oldTreble)
      Coefficents(hzTreble, slope, trebleDb, samplerate, kTreble,
                  a0Treble, a1Treble, a2Treble, b0Treble, b1Treble, b2Treble);

   for (size_t i = 0; i < len; i++)
      out[i] = DoFilter(in[i]) * gain;

   return len;
}

inline void EffectBassTrebleState::Coefficents(double hz, double slope, double gain,
                                               double samplerate, int type,
                                               double& a0, double& a1, double& a2,
                                               double& b0, double& b1, double& b2)
{
   double w = 2 * M_PI * hz / samplerate;
   double a = std::exp(std::log(10.0) * gain / 40);
   double b = std::sqrt((a * a + 1) / slope - (std::pow((a - 1), 2)));

   if (type == kBass)
   {
      b0 = a * ((a + 1) - (a - 1) * std::cos(w) + b * std::sin(w));
      b1 = 2 * a * ((a - 1) - (a + 1) * std::cos(w));
      b2 = a * ((a + 1) - (a - 1) * std::cos(w) - b * std::sin(w));
      a0 = ((a + 1) + (a - 1) * std::cos(w) + b * std::sin(w));
      a1 = -2 * ((a - 1) + (a + 1) * std::cos(w));
      a2 = (a + 1) + (a - 1) * std::cos(w) - b * std::sin(w);
   }
   else //assumed kTreble
   {
      b0 = a * ((a + 1) + (a - 1) * std::cos(w) + b * std::sin(w));
      b1 = -2 * a * ((a - 1) + (a + 1) * std::cos(w));
      b2 = a * ((a + 1) + (a - 1) * std::cos(w) - b * std::sin(w));
      a0 = ((a + 1) - (a - 1) * std::cos(w) + b * std::sin(w));
      a1 = 2 * ((a - 1) - (a + 1) * std::cos(w));
      a2 = (a + 1) - (a - 1) * std::cos(w) - b * std::sin(w);
   }
}

inline float EffectBassTrebleState::DoFilter(float in)
{
   // Bass filter
   float out = (b0Bass * in + b1Bass * xn1Bass + b2Bass * xn2Bass -
         a1Bass * yn1Bass - a2Bass * yn2Bass) / a0Bass;
   xn2Bass = xn1Bass;
   xn1Bass = in;
   yn2Bass = yn1Bass;
   yn1Bass = out;

   // Treble filter
   in = out;
   out = (b0Treble * in + b1Treble * xn1Treble + b2Treble * xn2Treble -
         a1Treble * yn1Treble - a2Treble * yn2Treble) / a0Treble;
   xn2Treble = xn1Treble;
   xn1Treble = in;
   yn2Treble = yn1Treble;
   yn1Treble = out;

   return out;
}

#endif
