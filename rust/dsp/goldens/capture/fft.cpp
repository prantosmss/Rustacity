/*

   Capture reference values from the original C++ implementation of the FFT,
   for the Rust port of RealFFTf.cpp and FFT.cpp.

   This includes only src/FFTMath.h - the arithmetic moved out of those two
   files - which is free of wxWidgets, so that it compiles on its own.  Unlike
   the earlier dumpers the unit is not in src/effects/, hence the extra -Isrc
   on the compiler command line in the Goldens job.  What it prints is
   appended to rust/dsp/goldens/reference.txt.

   Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

*/

#include "FFTMath.h"

#include <cstddef>
#include <cstdio>
#include <vector>

namespace
{

/// The samples a block is run with: integer arithmetic narrowed to a float, so
/// that a port is reproduced on any platform.  The values span -1 to 1.
/// `block` is the record's index within its run, so that consecutive records
/// do not share an input.
float InputSample(size_t i, size_t block)
{
   return float(int((i * 37 + block * 11) % 129) - 64) / 64.0f;
}

/// The indices a record prints: every `stride`-th sample, with the last always
/// included, so that a window's extra sample and a table's last entry cannot
/// be missed.  Records carry `len` and `stride` so the Rust side rebuilds this
/// list exactly.
std::vector<size_t> Indices(size_t len, size_t stride)
{
   std::vector<size_t> idx;
   for (size_t i = 0; i < len; i += stride)
      idx.push_back(i);
   if (len > 0 && idx.back() != len - 1)
      idx.push_back(len - 1);
   return idx;
}

void PrintSamples(const float *a, const std::vector<size_t> &idx)
{
   for (auto i : idx)
      std::printf(" %.9g", double(a[i]));
}

void PrintCount(size_t len, size_t stride)
{
   std::printf(" %d", (int) Indices(len, stride).size());
}

/// One FFT length: the tables themselves, then the transforms the two C++
/// files offer, at the wrapper level and at the raw level the wrapper is
/// built from.
void RunSize(size_t fftlen, size_t stride, size_t block)
{
   const size_t points = fftlen / 2;

   std::vector<float> in(fftlen);
   for (size_t i = 0; i < fftlen; ++i)
      in[i] = InputSample(i, block);

   std::vector<int> bitReversed(points);
   std::vector<float> sinTable(2 * points);
   fftmath::InitTables(fftlen, bitReversed.data(), sinTable.data());

   std::printf("fft-bitreversed %d %d", (int) fftlen, (int) stride);
   PrintCount(points, stride);
   for (auto i : Indices(points, stride))
      std::printf(" %d", bitReversed[i]);
   std::printf("\n");

   std::printf("fft-sintable %d %d", (int) fftlen, (int) stride);
   PrintCount(2 * points, stride);
   PrintSamples(sinTable.data(), Indices(2 * points, stride));
   std::printf("\n");

   /// The raw transform, in place, and what the two reorder functions make of
   /// its bit-reversed output.
   std::vector<float> buf(in);
   fftmath::Forward(buf.data(), bitReversed.data(), sinTable.data(), fftlen);

   std::printf("fft-forward %d %d", (int) fftlen, (int) stride);
   PrintCount(fftlen, stride);
   PrintSamples(buf.data(), Indices(fftlen, stride));
   std::printf("\n");

   std::vector<float> real(points + 1), imag(points + 1);
   fftmath::ReorderToFreq(bitReversed.data(), fftlen, buf.data(),
                          real.data(), imag.data());

   std::printf("fft-reorder-freq %d %d", (int) fftlen, (int) stride);
   PrintCount(points + 1, stride);
   PrintSamples(real.data(), Indices(points + 1, stride));
   PrintSamples(imag.data(), Indices(points + 1, stride));
   std::printf("\n");

   std::vector<float> time(fftlen);
   fftmath::ReorderToTime(bitReversed.data(), fftlen, buf.data(), time.data());

   std::printf("fft-reorder-time %d %d", (int) fftlen, (int) stride);
   PrintCount(fftlen, stride);
   PrintSamples(time.data(), Indices(fftlen, stride));
   std::printf("\n");

   /// The inverse, on the interleaved conjugate-symmetric buffer the real
   /// transform's input would produce: the Fs/2 component rides in the
   /// imaginary half of the DC bin.
   std::vector<float> sym(fftlen);
   for (size_t i = 0; i < points; ++i) {
      sym[2 * i] = real[i];
      sym[2 * i + 1] = imag[i];
   }
   sym[1] = real[points];
   fftmath::Inverse(sym.data(), bitReversed.data(), sinTable.data(), fftlen);

   std::printf("fft-inverse %d %d", (int) fftlen, (int) stride);
   PrintCount(fftlen, stride);
   PrintSamples(sym.data(), Indices(fftlen, stride));
   std::printf("\n");

   /// The wrapper level.
   std::vector<float> wrapReal(fftlen), wrapImag(fftlen);
   fftmath::RealFFT(fftlen, bitReversed.data(), sinTable.data(), in.data(),
                    wrapReal.data(), wrapImag.data());

   std::printf("fft-real %d %d", (int) fftlen, (int) stride);
   PrintCount(fftlen, stride);
   PrintSamples(wrapReal.data(), Indices(fftlen, stride));
   PrintSamples(wrapImag.data(), Indices(fftlen, stride));
   std::printf("\n");

   std::vector<float> recon(fftlen);
   fftmath::InverseRealFFT(fftlen, bitReversed.data(), sinTable.data(),
                           wrapReal.data(), wrapImag.data(), recon.data());

   std::printf("fft-inverse-real %d 1 %d", (int) fftlen, (int) stride);
   PrintCount(fftlen, stride);
   PrintSamples(recon.data(), Indices(fftlen, stride));
   std::printf("\n");

   std::vector<float> reconNoImag(fftlen);
   fftmath::InverseRealFFT(fftlen, bitReversed.data(), sinTable.data(),
                           wrapReal.data(), nullptr, reconNoImag.data());

   std::printf("fft-inverse-real %d 0 %d", (int) fftlen, (int) stride);
   PrintCount(fftlen, stride);
   PrintSamples(reconNoImag.data(), Indices(fftlen, stride));
   std::printf("\n");

   std::vector<float> power(fftlen);
   fftmath::PowerSpectrum(fftlen, bitReversed.data(), sinTable.data(), in.data(),
                          power.data());

   std::printf("fft-power %d %d", (int) fftlen, (int) stride);
   PrintCount(points + 1, stride);
   PrintSamples(power.data(), Indices(points + 1, stride));
   std::printf("\n");

   /// And the complex transform of FFT.cpp, which keeps a bit table of its
   /// own, with and without an imaginary part.
   std::vector<float> cRealIn(fftlen), cImagIn(fftlen);
   for (size_t i = 0; i < fftlen; ++i) {
      cRealIn[i] = InputSample(i, block + 1);
      cImagIn[i] = InputSample(i, block + 2);
   }

   for (int inverse = 0; inverse < 2; ++inverse) {
      std::vector<float> cRealOut(fftlen), cImagOut(fftlen);
      fftmath::FFT(fftlen, inverse != 0, cRealIn.data(), cImagIn.data(),
                   cRealOut.data(), cImagOut.data());

      std::printf("fft-complex %d %d 1 %d", (int) fftlen, inverse, (int) stride);
      PrintCount(fftlen, stride);
      PrintSamples(cRealOut.data(), Indices(fftlen, stride));
      PrintSamples(cImagOut.data(), Indices(fftlen, stride));
      std::printf("\n");

      std::vector<float> cRealOut2(fftlen), cImagOut2(fftlen);
      fftmath::FFT(fftlen, inverse != 0, cRealIn.data(), nullptr,
                   cRealOut2.data(), cImagOut2.data());

      std::printf("fft-complex %d %d 0 %d", (int) fftlen, inverse, (int) stride);
      PrintCount(fftlen, stride);
      PrintSamples(cRealOut2.data(), Indices(fftlen, stride));
      PrintSamples(cImagOut2.data(), Indices(fftlen, stride));
      std::printf("\n");
   }
}

/// One window function, at one length, with the extra sample either way.
void RunWindow(int which, size_t len, bool extra, size_t stride, size_t block)
{
   const auto idx = Indices(len, stride);
   std::vector<float> data(len + 1);

   for (auto &value : data)
      value = 0;

   for (size_t i = 0; i < len; ++i)
      data[i] = InputSample(i, block);
   fftmath::NewWindowFunc(which, len, extra, data.data());

   std::printf("fft-window %d %d %d %d %d", which, (int) len, extra ? 1 : 0,
               (int) stride, (int) idx.size());
   PrintSamples(data.data(), idx);
   std::printf("\n");

   for (size_t i = 0; i < len; ++i)
      data[i] = InputSample(i, block + 1);
   fftmath::DerivativeOfWindowFunc(which, len, extra, data.data());

   std::printf("fft-window-derivative %d %d %d %d %d", which, (int) len,
               extra ? 1 : 0, (int) stride, (int) idx.size());
   PrintSamples(data.data(), idx);
   std::printf("\n");
}

}

int main()
{
   // Small enough to be a table you can read, the size most of the callers
   // use, and one large enough that the butterflies run several levels deep.
   const size_t lens[] = { 16, 64, 1024 };
   const size_t strides[] = { 1, 4, 16 };

   size_t block = 0;
   for (size_t k = 0; k < sizeof(lens) / sizeof(lens[0]); ++k)
      RunSize(lens[k], strides[k], block++);

   // Every window, at a length a caller is likely to use and at an odd one,
   // with and without the extra sample.
   for (int which = 0; which < eWinFuncCount; ++which) {
      RunWindow(which, 64, false, 4, block++);
      RunWindow(which, 64, true, 4, block++);
      RunWindow(which, 257, false, 16, block++);
      RunWindow(which, 257, true, 16, block++);
   }

   return 0;
}
