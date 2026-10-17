/*

   Capture reference values from the original C++ implementation of
   EffectWahwah, for the Rust port of its wah filter.

   This includes only src/effects/WahwahFilter.h, which is free of wxWidgets,
   so that it compiles on its own - see README.md in this directory.  What it
   prints is appended to rust/dsp/goldens/reference.txt.

   Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

*/

#include "WahwahFilter.h"

#include <cstddef>
#include <cstdio>

namespace
{

/// The samples a block is run with: integer arithmetic narrowed to a float,
/// so that a port is reproduced on any platform.  `block` is the record's
/// index within its run, so that consecutive blocks do not share an input.
float InputSample(size_t i, size_t block)
{
   return float(int((i * 37 + block * 11) % 101) - 50) / 64.0f;
}

/// One record's parameters: everything the filter takes for a block.
struct Params
{
   double freq;
   double phase;
   int depth;
   double res;
   int freqOfs;
   double outGain;
};

const size_t kMaxLen = 512;

/// Run one block of `len` samples and print it; `stride` is how many samples
/// apart the printed ones are.
void Run(EffectWahwahState & state, const Params & p, double rate,
         size_t block, size_t len, size_t stride)
{
   float in[kMaxLen];
   float out[kMaxLen];

   for (size_t i = 0; i < len; ++i)
      in[i] = InputSample(i, block);

   state.Process(p.freq, p.phase, p.depth, p.res, p.freqOfs, p.outGain,
                 in, out, len);

   std::printf("wah-block %.17g %.17g %.17g %d %.17g %d %.17g %d %d",
               rate, p.freq, p.phase, p.depth, p.res, p.freqOfs, p.outGain,
               (int) len, (int) stride);

   for (size_t i = 0; i < len; i += stride)
      std::printf(" %.9g", double(out[i]));

   std::printf("\n");
}

}

int main()
{
   const double rates[] = { 44100.0, 48000.0 };

   // The parameters of the short blocks, spread over the whole range the
   // effect allows: the extremes of the LFO frequency and the resonance, no
   // depth at all (where the LFO collapses onto the frequency offset), the
   // full sweep, and the defaults again at the end, so that a record with
   // unchanged parameters follows the ones that moved them.
   const Params blocks[] =
   {
      { 1.5,   0.0,  70,  2.5,  30,  -6.0 },   // the defaults
      { 0.1,   0.0,   0,  0.1,   0, -30.0 },   // no depth, lowest LFO rate
      { 4.0, 180.0, 100, 10.0,   0,  30.0 },   // highest rate, depth and res
      { 4.0, 360.0, 100, 10.0, 100,  30.0 },   // the same at full offset
      { 0.5,  90.0,  50,  1.0,  50,   0.0 },
      { 3.0, 270.0, 100,  5.0,  10, -12.0 },
      { 1.5,   0.0,  70,  2.5,  30,  -6.0 },   // the defaults again
      { 2.0,  45.0,  25,  0.5,  75,   6.0 },
   };

   const size_t shortLen = 32;
   const size_t count = sizeof(blocks) / sizeof(blocks[0]);

   // One long block, sampled sparsely: at the highest LFO frequency this is a
   // few percent of a cycle, which the short blocks above do not reach, and
   // printed at a stride so that the record stays small.
   const Params lfo = { 4.0, 0.0, 100, 2.5, 0, 0.0 };
   const size_t lfoLen = 512;
   const size_t lfoStride = 4;

   for (size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); ++r)
   {
      EffectWahwahState state;
      state.Init((float) rates[r]);

      for (size_t b = 0; b < count; ++b)
         Run(state, blocks[b], rates[r], b, shortLen, 1);

      Run(state, lfo, rates[r], count, lfoLen, lfoStride);
   }

   return 0;
}
