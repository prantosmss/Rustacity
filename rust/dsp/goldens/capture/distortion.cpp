/*

   Capture reference values from the original C++ implementation of
   EffectDistortion, for the Rust port of its waveshaper.

   This includes only src/effects/DistortionFilter.h, which is free of
   wxWidgets, so that it compiles on its own - see README.md in this directory.
   What it prints is appended to rust/dsp/goldens/reference.txt.

   Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

*/

#include "DistortionFilter.h"

#include <cstddef>
#include <cstdio>

namespace
{

/// The samples a block is run with: integer arithmetic narrowed to a float, so
/// that a port is reproduced on any platform.  The values span -1 to 1, which
/// is the whole of the lookup table, ends included.  `block` is the record's
/// index within its run, so that consecutive blocks do not share an input.
float InputSample(size_t i, size_t block)
{
   return float(int((i * 37 + block * 11) % 129) - 64) / 64.0f;
}

const size_t kMaxLen = 4096;

/// Run one block of `len` samples and print it; `stride` is how many samples
/// apart the printed ones are.
void Run(EffectDistortionState & state, const EffectDistortionParams & p,
         double rate, size_t block, size_t len, size_t stride)
{
   float in[kMaxLen];
   float out[kMaxLen];

   for (size_t i = 0; i < len; ++i)
      in[i] = InputSample(i, block);

   state.Process(p, in, out, len);

   std::printf("dis-block %.17g %d %d %.17g %.17g %.17g %.17g %d %d %d",
               rate, p.tableChoiceIndx, p.dcBlock ? 1 : 0, p.thresholdDb,
               p.noiseFloor, p.param1, p.param2, p.repeats,
               (int) len, (int) stride);

   for (size_t i = 0; i < len; i += stride)
      std::printf(" %.9g", double(out[i]));

   std::printf("\n");
}

}

int main()
{
   const double rates[] = { 44100.0, 48000.0 };
   const int tables = nTableTypes;
   const size_t shortLen = 32;

   // One set of settings per group of blocks, spread over what the effect
   // allows: the defaults; no parameter 1, which takes several builders down a
   // branch of their own; the highest parameter 1 and number of repeats with
   // the lowest noise floor; and no threshold at all, with the whole of
   // parameter 2 as make-up gain.  The DC block rides on the last two sets, so
   // that its queue fills up over the blocks that follow.
   const EffectDistortionParams sets[] =
   {
      { 0, false,  -6.0, -70.0,  50.0,  50.0, 1 },
      { 0, false, -12.0, -50.0,   0.0,  80.0, 0 },
      { 0, true,  -40.0, -80.0, 100.0,  20.0, 5 },
      { 0, true,    0.0, -30.0,  25.0, 100.0, 2 },
   };

   // One long block, sampled sparsely: the DC block's queue holds a rolling
   // average of a twentieth of a second of samples, which the short blocks
   // above never fill, whereas this one passes it several times over.  The
   // rectifier leaves the average well away from zero.
   const EffectDistortionParams longParams =
      { kRectifier, true, -6.0, -70.0, 25.0, 50.0, 0 };
   const size_t longLen = 4096;
   const size_t longStride = 4;

   for (size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); ++r)
   {
      EffectDistortionState state;
      state.Init((float) rates[r]);

      size_t block = 0;
      for (size_t s = 0; s < sizeof(sets) / sizeof(sets[0]); ++s)
      {
         for (int table = 0; table < tables; ++table)
         {
            EffectDistortionParams p = sets[s];
            p.tableChoiceIndx = table;
            Run(state, p, rates[r], block++, shortLen, 1);
         }
      }

      Run(state, longParams, rates[r], block++, longLen, longStride);
   }

   return 0;
}
