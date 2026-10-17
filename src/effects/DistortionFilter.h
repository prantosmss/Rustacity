/**********************************************************************

   Rustacity: A Digital Audio Editor

   DistortionFilter.h

   Steve Daulton

   The signal processing of EffectDistortion, moved out of Distortion.cpp so
   that it can be compiled on its own: no wxWidgets, no effect framework,
   nothing but <algorithm>, <cmath> and <queue>.  The Rust port of the effect
   replaces this file - see rust/dsp/src/distortion.rs and
   rust/dsp/goldens/capture/.

***********************************************************************/

#ifndef __RUSTACITY_DISTORTION_FILTER_H__
#define __RUSTACITY_DISTORTION_FILTER_H__

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <queue>

// M_PI and M_PI_2 are not in <cmath> on every platform the build targets.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

/// Number of positive or negative steps in the lookup table (STEPS in
/// Distortion.h).
const int kDistortionSteps = 1024;

/// Size of the lookup table (TABLESIZE in Distortion.h).
const int kDistortionTableSize = 2 * kDistortionSteps + 1;

/// dB to linear amplitude - DB_TO_LINEAR in Rustacity.h, kept local so that
/// this header compiles on its own.
inline double DistortionDbToLinear(double db)
{
   return pow(10.0, db / 20.0);
}

/// The kinds of table the effect builds.
enum kTableType
{
   kHardClip,
   kSoftClip,
   kHalfSinCurve,
   kExpCurve,
   kLogCurve,
   kCubic,
   kEvenHarmonics,
   kSinCurve,
   kLeveller,
   kRectifier,
   kHardLimiter,
   nTableTypes
};

/// The effect's settings for one block.
struct EffectDistortionParams
{
   int    tableChoiceIndx;
   bool   dcBlock;
   double thresholdDb;
   double noiseFloor;
   double param1;
   double param2;
   int    repeats;
};

/// One channel of the effect: the lookup table of the current settings, what
/// it was built from, and the state of the DC blocking filter.
struct EffectDistortionState
{
   float samplerate;

   /// The table, and the settings it holds.  The C++ rebuilt it from the
   /// effect's parameters every 1000 samples while they differed from the
   /// ones of the block before; the table is a function of those parameters
   /// alone, so rebuilding it once per block in which they change gives the
   /// same table.
   double table[kDistortionTableSize];
   EffectDistortionParams tableparams;
   bool tablevalid;
   double threshold;
   double makeupgain;

   /// DC block filter variables.
   std::queue<float> queuesamples;
   double queuetotal;
   unsigned int queuelength;

   /// Reset for `sampleRate` - the C++ EffectDistortion::InstanceInit().
   ///
   /// The C++ also stored the effect's parameters here and built the table, but
   /// every parameter reaches Process() per block and the table is built from
   /// them before the first sample, so Init() only has to reset.
   void Init(float sampleRate)
   {
      samplerate = sampleRate;

      tablevalid = false;
      threshold = 0.0;
      makeupgain = 1.0;

      while (!queuesamples.empty())
         queuesamples.pop();
      queuetotal = 0.0;

      // The C++ derived this inside the filter on every sample.
      queuelength = (unsigned int) std::floor(samplerate / 20.0);
   }

   /// Shape `len` samples of one channel - the C++ InstanceProcess().
   ///
   /// `params` are the effect's settings for this block.  `thresholdDb` is
   /// converted to the linear amplitude the table is built with, which is what
   /// the C++ had in a cache member of its own that the automation path never
   /// refreshed.
   size_t Process(const EffectDistortionParams & params,
                  const float * in, float * out, size_t len)
   {
      if (!tablevalid || !SameTableParams(params))
      {
         threshold = DistortionDbToLinear(params.thresholdDb);
         tableparams = params;
         tablevalid = true;
         MakeTable(params);
      }

      double p1 = params.param1 / 100.0;
      double p2 = params.param2 / 100.0;

      for (size_t i = 0; i < len; i++)
      {
         switch (params.tableChoiceIndx)
         {
         case kHardClip:
            // Param2 = make-up gain.
            out[i] = WaveShaper(params, in[i]) * ((1 - p2) + (makeupgain * p2));
            break;
         case kSoftClip:
            // Param2 = make-up gain.
            out[i] = WaveShaper(params, in[i]) * ((1 - p2) + (makeupgain * p2));
            break;
         case kHalfSinCurve:
            out[i] = WaveShaper(params, in[i]) * p2;
            break;
         case kExpCurve:
            out[i] = WaveShaper(params, in[i]) * p2;
            break;
         case kLogCurve:
            out[i] = WaveShaper(params, in[i]) * p2;
            break;
         case kCubic:
            out[i] = WaveShaper(params, in[i]) * p2;
            break;
         case kEvenHarmonics:
            out[i] = WaveShaper(params, in[i]);
            break;
         case kSinCurve:
            out[i] = WaveShaper(params, in[i]) * p2;
            break;
         case kLeveller:
            out[i] = WaveShaper(params, in[i]);
            break;
         case kRectifier:
            out[i] = WaveShaper(params, in[i]);
            break;
         case kHardLimiter:
            // Mix equivalent to LADSPA effect's "Wet / Residual" mix
            out[i] = (WaveShaper(params, in[i]) * (p1 - p2)) + (in[i] * p2);
            break;
         default:
            out[i] = WaveShaper(params, in[i]);
         }
         if (params.dcBlock) {
            out[i] = DCFilter(out[i]);
         }
      }

      return len;
   }

private:

   /// Are these the settings the table was built from?  The C++ compared the
   /// settings of this block against those of the block before, without the DC
   /// block, which does not shape the table.
   bool SameTableParams(const EffectDistortionParams & params) const
   {
      return params.tableChoiceIndx == tableparams.tableChoiceIndx &&
             params.thresholdDb == tableparams.thresholdDb &&
             params.noiseFloor == tableparams.noiseFloor &&
             params.param1 == tableparams.param1 &&
             params.param2 == tableparams.param2 &&
             params.repeats == tableparams.repeats;
   }

   void MakeTable(const EffectDistortionParams & params)
   {
      switch (params.tableChoiceIndx)
      {
         case kHardClip:
            HardClip();
            break;
         case kSoftClip:
            SoftClip(params);
            break;
         case kHalfSinCurve:
            HalfSinTable(params);
            break;
         case kExpCurve:
            ExponentialTable(params);
            break;
         case kLogCurve:
            LogarithmicTable(params);
            break;
         case kCubic:
            CubicTable(params);
            break;
         case kEvenHarmonics:
            EvenHarmonicTable(params);
            break;
         case kSinCurve:
            SineTable(params);
            break;
         case kLeveller:
            Leveller(params);
            break;
         case kRectifier:
            Rectifier(params);
            break;
         case kHardLimiter:
            HardLimiter();
            break;
      }
   }

   //
   // Preset tables for gain lookup
   //

   void HardClip()
   {
      double lowThresh = 1 - threshold;
      double highThresh = 1 + threshold;

      for (int n = 0; n < kDistortionTableSize; n++) {
         if (n < (kDistortionSteps * lowThresh))
            table[n] = - threshold;
         else if (n > (kDistortionSteps * highThresh))
            table[n] = threshold;
         else
            table[n] = n/(double)kDistortionSteps - 1;

         makeupgain = 1.0 / threshold;
      }
   }

   void SoftClip(const EffectDistortionParams & params)
   {
      double upper = 1 + threshold;
      double amount = std::pow(2.0, 7.0 * params.param1 / 100.0); // range 1 to 128
      double peak = LogCurve(threshold, 1.0, amount);
      makeupgain = 1.0 / peak;
      table[kDistortionSteps] = 0.0;   // origin

      // positive half of table
      for (int n = kDistortionSteps; n < kDistortionTableSize; n++) {
         if (n < (kDistortionSteps * upper)) // origin to threshold
            table[n] = n/(float)kDistortionSteps - 1;
         else
            table[n] = LogCurve(threshold, n/(double)kDistortionSteps - 1, amount);
      }
      CopyHalfTable();
   }

   void ExponentialTable(const EffectDistortionParams & params)
   {
      double amount = std::min(0.999, DistortionDbToLinear(-1 * params.param1));   // avoid divide by zero

      for (int n = kDistortionSteps; n < kDistortionTableSize; n++) {
         double linVal = n/(float)kDistortionSteps;
         double scale = -1.0 / (1.0 - amount);   // unity gain at 0dB
         double curve = std::exp((linVal - 1) * std::log(amount));
         table[n] = scale * (curve -1);
      }
      CopyHalfTable();
   }

   void LogarithmicTable(const EffectDistortionParams & params)
   {
      double amount = params.param1;
      double stepsize = 1.0 / kDistortionSteps;
      double linVal = 0;

      if (amount == 0) {
         for (int n = kDistortionSteps; n < kDistortionTableSize; n++) {
            table[n] = linVal;
            linVal += stepsize;
         }
      }
      else {
         for (int n = kDistortionSteps; n < kDistortionTableSize; n++) {
            table[n] = std::log(1 + (amount * linVal)) / std::log(1 + amount);
            linVal += stepsize;
         }
      }
      CopyHalfTable();
   }

   void HalfSinTable(const EffectDistortionParams & params)
   {
      int iter = std::floor(params.param1 / 20.0);
      double fractionalpart = (params.param1 / 20.0) - iter;
      double stepsize = 1.0 / kDistortionSteps;
      double linVal = 0;

      for (int n = kDistortionSteps; n < kDistortionTableSize; n++) {
         table[n] = linVal;
         for (int i = 0; i < iter; i++) {
            table[n] = std::sin(table[n] * M_PI_2);
         }
         table[n] += ((std::sin(table[n] * M_PI_2) - table[n]) * fractionalpart);
         linVal += stepsize;
      }
      CopyHalfTable();
   }

   void CubicTable(const EffectDistortionParams & params)
   {
      double amount = params.param1 * std::sqrt(3.0) / 100.0;
      double gain = 1.0;
      if (amount != 0.0)
         gain = 1.0 / Cubic(params, std::min(amount, 1.0));

      double stepsize = amount / kDistortionSteps;
      double x = -amount;

      if (amount == 0) {
         for (int i = 0; i < kDistortionTableSize; i++) {
            table[i] = (i / (double)kDistortionSteps) - 1.0;
         }
      }
      else {
         for (int i = 0; i < kDistortionTableSize; i++) {
            table[i] = gain * Cubic(params, x);
            for (int j = 0; j < params.repeats; j++) {
               table[i] = gain * Cubic(params, table[i] * amount);
            }
            x += stepsize;
         }
      }
   }

   void EvenHarmonicTable(const EffectDistortionParams & params)
   {
      double amount = params.param1 / -100.0;
      double C = std::max(0.001, params.param2) / 10.0;

      double step = 1.0 / kDistortionSteps;
      double xval = -1.0;

      for (int i = 0; i < kDistortionTableSize; i++) {
         table[i] = ((1 + amount) * xval) -
                     (xval * (amount / std::tanh(C)) * std::tanh(C * xval));
         xval += step;
      }
   }

   void SineTable(const EffectDistortionParams & params)
   {
      int iter = std::floor(params.param1 / 20.0);
      double fractionalpart = (params.param1 / 20.0) - iter;
      double stepsize = 1.0 / kDistortionSteps;
      double linVal = 0.0;

      for (int n = kDistortionSteps; n < kDistortionTableSize; n++) {
         table[n] = linVal;
         for (int i = 0; i < iter; i++) {
            table[n] = (1.0 + std::sin((table[n] * M_PI) - M_PI_2)) / 2.0;
         }
         table[n] += (((1.0 + std::sin((table[n] * M_PI) - M_PI_2)) / 2.0) - table[n]) * fractionalpart;
         linVal += stepsize;
      }
      CopyHalfTable();
   }

   void Leveller(const EffectDistortionParams & params)
   {
      double noiseFloor = DistortionDbToLinear(params.noiseFloor);
      int numPasses = params.repeats;
      double fractionalPass = params.param1 / 100.0;

      const int numPoints = 6;
      const double gainFactors[numPoints] = { 0.80, 1.00, 1.20, 1.20, 1.00, 0.80 };
      double gainLimits[numPoints] = { 0.0001, 0.0, 0.1, 0.3, 0.5, 1.0 };
      double addOnValues[numPoints];

      gainLimits[1] = noiseFloor;
      /* In the original Leveller effect, behaviour was undefined for threshold > 20 dB.
       * If we want to support > 20 dB we need to scale the points to keep them non-decreasing.
       *
       * if (noiseFloor > gainLimits[2]) {
       *    for (int i = 3; i < numPoints; i++) {
       *    gainLimits[i] = noiseFloor + ((1 - noiseFloor)*((gainLimits[i] - 0.1) / 0.9));
       * }
       * gainLimits[2] = noiseFloor;
       * }
       */

      // Calculate add-on values
      addOnValues[0] = 0.0;
      for (int i = 0; i < numPoints-1; i++) {
         addOnValues[i+1] = addOnValues[i] + (gainLimits[i] * (gainFactors[i] - gainFactors[1 + i]));
      }

      // Positive half of table.
      // The original effect increased the 'strength' of the effect by
      // repeated passes over the audio data.
      // Here we model that more efficiently by repeated passes over a linear table.
      for (int n = kDistortionSteps; n < kDistortionTableSize; n++) {
         table[n] = ((double) (n - kDistortionSteps) / (double) kDistortionSteps);
         for (int j = 0; j < numPasses; j++) {
            // Find the highest index for gain adjustment
            int index = numPoints - 1;
            for (int i = index; i >= 0 && table[n] < gainLimits[i]; i--) {
               index = i;
            }
            // the whole number of 'repeats'
            table[n] = (table[n] * gainFactors[index]) + addOnValues[index];
         }
         // Extrapolate for fine adjustment.
         // tiny fractions are not worth the processing time
         if (fractionalPass > 0.001) {
            int index = numPoints - 1;
            for (int i = index; i >= 0 && table[n] < gainLimits[i]; i--) {
               index = i;
            }
            table[n] += fractionalPass * ((table[n] * (gainFactors[index] - 1)) + addOnValues[index]);
         }
      }
      CopyHalfTable();
   }

   void Rectifier(const EffectDistortionParams & params)
   {
      double amount = (params.param1 / 50.0) - 1;
      double stepsize = 1.0 / kDistortionSteps;
      int index = kDistortionSteps;

      // positive half of waveform is passed unaltered.
      for  (int n = 0; n <= kDistortionSteps; n++) {
         table[index] = n * stepsize;
         index += 1;
      }

      // negative half of table
      index = kDistortionSteps - 1;
      for (int n = 1; n <= kDistortionSteps; n++) {
         table[index] = n * stepsize * amount;
         index--;
      }
   }

   void HardLimiter()
   {
      // The LADSPA "hardLimiter 1413" is basically hard clipping,
      // but with a 'kind of' wet/dry mix:
      // out = ((wet-residual)*clipped) + (residual*in)
      HardClip();
   }

   //
   // Helper functions for lookup tables
   //

   void CopyHalfTable()
   {
      // Copy negative half of table from positive half
      int count = kDistortionTableSize - 1;
      for (int n = 0; n < kDistortionSteps; n++) {
         table[n] = -table[count];
         count--;
      }
   }

   /// Used by Soft Clipping but could be used for other tables.
   /// Log curve formula: y = T + (((e^(RT - Rx)) - 1) / -R)
   /// where R is the ratio, T is the threshold, and x is from T to 1.
   ///
   /// The value is a float because the C++ took one, and the result narrows
   /// back to a float.
   inline float LogCurve(double threshold, float value, double ratio)
   {
      return threshold + ((std::exp(ratio * (threshold - value)) - 1) / -ratio);
   }

   /// Used by Cubic curve but could be used for other tables
   /// Cubic formula: y = x - (x^3 / 3.0)
   inline double Cubic(const EffectDistortionParams & params, double x)
   {
      if (params.param1 == 0.0)
         return x;

      return x - (std::pow(x, 3.0) / 3.0);
   }

   float WaveShaper(const EffectDistortionParams & params, float sample)
   {
      float out;
      int index;
      double xOffset;
      double amount = 1;

      switch (params.tableChoiceIndx)
      {
         // Do any pre-processing here
         case kHardClip:
            // Pre-gain
            amount = params.param1 / 100.0;
            sample *= 1+amount;
            break;
         default: break;
      }

      index = std::floor(sample * kDistortionSteps) + kDistortionSteps;
      index = std::max<int>(std::min<int>(index, 2 * kDistortionSteps - 1), 0);
      xOffset = ((1 + sample) * kDistortionSteps) - index;
      xOffset = std::min<double>(std::max<double>(xOffset, 0.0), 1.0);   // Clip at 0dB

      // linear interpolation: y = y0 + (y1-y0)*(x-x0)
      out = table[index] + (table[index + 1] - table[index]) * xOffset;

      return out;
   }

   float DCFilter(float sample)
   {
      // Rolling average gives less offset at the start than an IIR filter.
      queuetotal += sample;
      queuesamples.push(sample);

      if (queuesamples.size() > queuelength) {
         queuetotal -= queuesamples.front();
         queuesamples.pop();
      }

      return sample - (queuetotal / queuesamples.size());
   }
};

#endif
