/*

   Capture reference values from the original C++ implementation of
   EffectPhaser, for the Rust port of its allpass filter.

   This includes only src/effects/PhaserFilter.h, which is free of
   wxWidgets, so that it compiles on its own - see README.md in this
   directory.  What it prints is appended to rust/dsp/goldens/reference.txt.

   Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

*/

#include "PhaserFilter.h"

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
   int stages;
   int dryWet;
   double freq;
   double phase;
   int depth;
   int feedback;
   double outGain;
};

const size_t kMaxLen = 512;

/// Run one block of `len` samples and print it; `stride` is how many samples
/// apart the printed ones are.
void Run(EffectPhaserState & state, const Params & p, double rate,
         size_t block, size_t len, size_t stride)
{
   float in[kMaxLen];
   float out[kMaxLen];

   for (size_t i = 0; i < len; ++i)
      in[i] = InputSample(i, block);

   state.Process(p.stages, p.dryWet, p.freq, p.phase, p.depth, p.feedback,
                 p.outGain, in, out, len);

   std::printf("ph-block %.17g %d %d %.17g %.17g %d %d %.17g %d %d",
               rate, p.stages, p.dryWet, p.freq, p.phase, p.depth, p.feedback,
               p.outGain, (int) len, (int) stride);

   for (size_t i = 0; i < len; i += stride)
      std::printf(" %.9g", double(out[i]));

   std::printf("\n");
}

}

int main()
{
   const double rates[] = { 44100.0, 48000.0 };

   // The parameters of the short blocks, spread over the whole range the
   // effect allows.  The stage counts change between records too, which the
   // filter has to follow: it clears the stages it grows into and leaves the
   // ones it shrinks out of alone.
   const Params blocks[] =
   {
      { 2,  128, 0.4,   0.0,   100,    0,  -6.0 },
      { 24, 128, 0.4,   0.0,   100,    0,  -6.0 },
      { 24, 255, 4.0,   180.0, 255,  100,   6.0 },
      { 24, 0,   0.001, 360.0,   0, -100, -30.0 },
      { 6,  64,  2.0,   90.0,   50,   50,   0.0 },
      { 8,  200, 1.0,   45.0,  200,  -50,  12.0 },
      { 24, 128, 0.25,  0.0,   100,   99,  -6.0 },
      { 2,  255, 3.0,   270.0, 128, -100,  30.0 },
   };

   const size_t shortLen = 32;
   const size_t count = sizeof(blocks) / sizeof(blocks[0]);

   // One long block, sampled sparsely: at the highest LFO frequency this is a
   // few percent of a cycle, which the short blocks above do not reach, and
   // printed at a stride so that the record stays small.
   const Params lfo = { 24, 128, 4.0, 0.0, 255, 50, 0.0 };
   const size_t lfoLen = 512;
   const size_t lfoStride = 4;

   for (size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); ++r)
   {
      EffectPhaserState state;
      state.Init((float) rates[r]);

      for (size_t b = 0; b < count; ++b)
         Run(state, blocks[b], rates[r], b, shortLen, 1);

      Run(state, lfo, rates[r], count, lfoLen, lfoStride);
   }

   return 0;
}
