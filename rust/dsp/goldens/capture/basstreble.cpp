/*

   Capture reference values from the original C++ implementation of
   EffectBassTreble, for the Rust port of its shelf filters.

   This includes only src/effects/BassTrebleFilter.h, which is free of
   wxWidgets, so that it compiles on its own - see README.md in this
   directory.  What it prints is appended to rust/dsp/goldens/reference.txt.

   Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

*/

#include "BassTrebleFilter.h"

#include <cstddef>
#include <cstdio>

namespace
{

// %.17g round trips a double exactly, and %.9g round trips a float.
void PrintDouble(double value)
{
   std::printf(" %.17g", value);
}

}

int main()
{
   const double rates[] = { 44100.0, 48000.0, 96000.0 };
   const double gains[] = { -30.0, -12.0, -6.0, 0.0, 6.0, 12.0, 30.0 };

   EffectBassTrebleState state;

   // The designed coefficients: both shelves, every gain the effect allows,
   // on the sample rates it is normally used at.
   for (size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); ++r)
   {
      state.Init((float)rates[r], 0.0);

      for (int type = EffectBassTrebleState::kBass;
           type <= EffectBassTrebleState::kTreble; ++type)
      {
         const double hz = (type == EffectBassTrebleState::kBass)
                              ? state.hzBass : state.hzTreble;

         for (size_t g = 0; g < sizeof(gains) / sizeof(gains[0]); ++g)
         {
            double a0, a1, a2, b0, b1, b2;
            state.Coefficents(hz, state.slope, gains[g], rates[r], type,
                              a0, a1, a2, b0, b1, b2);

            std::printf("bt-coeffs %.17g %d", rates[r], type);
            PrintDouble(hz);
            PrintDouble(state.slope);
            PrintDouble(gains[g]);
            PrintDouble(a0);
            PrintDouble(a1);
            PrintDouble(a2);
            PrintDouble(b0);
            PrintDouble(b1);
            PrintDouble(b2);
            std::printf("\n");
         }
      }
   }

   // Whole blocks of filtered output, including the flat 0 dB case.  The
   // input is integer arithmetic narrowed to float, so a port reproduces it
   // on any platform.
   const double bassValues[] = { -15.0, 0.0, 6.0, -30.0 };
   const double trebleValues[] = { 0.0, 0.0, 9.0, 30.0 };
   const double blockGains[] = { -3.0, 0.0, 12.0, 6.0 };
   const size_t len = 32;
   const size_t blocks = sizeof(bassValues) / sizeof(bassValues[0]);

   for (size_t c = 0; c < blocks; ++c)
   {
      float in[len];
      for (size_t i = 0; i < len; ++i)
         in[i] = float(int((i * 37) % 101) - 50) / 64.0f;

      state.Init(44100.0f, blockGains[c]);

      float out[len];
      state.Process(bassValues[c], trebleValues[c], blockGains[c],
                    in, out, len);

      std::printf("bt-block %.17g", 44100.0);
      PrintDouble(bassValues[c]);
      PrintDouble(trebleValues[c]);
      PrintDouble(blockGains[c]);
      std::printf(" %d", (int)len);
      for (size_t i = 0; i < len; ++i)
         std::printf(" %.9g", double(out[i]));
      std::printf("\n");
   }

   return 0;
}
