/**********************************************************************

   Rustacity: A Digital Audio Editor

   FFTMath.h

   Philip Van Baren, Dominic Mazzoni

   The arithmetic of the real and complex FFTs and of the window functions,
   moved out of RealFFTf.cpp and FFT.cpp so that it can be compiled on its
   own: no wxWidgets, no application headers, nothing but <cassert>, <cmath>,
   <cstddef>, <cstdio>, <cstdlib> and <vector>.  Those two files keep the table cache,
   the translated window names, the argument checks and the scratch buffers,
   and call in here.

   The move is a copy, not a revision.  The only substitutions are the ones
   wx-freedom forces: wxASSERT becomes assert, wxFprintf becomes fprintf (with
   the conversion specifiers corrected for the types passed), and the
   wxWidgets containers become std::vector.

   The Rust port replaces this file - see rust/dsp/src/fft.rs and
   rust/dsp/goldens/capture/.

***********************************************************************/

#ifndef __RUSTACITY_FFT_MATH_H__
#define __RUSTACITY_FFT_MATH_H__

#include <cmath>
#include <cstddef>

// M_PI is not in <cmath> on every platform the build targets.  FFT.h has its
// own copy of this for the code that has not moved.
#ifndef M_PI
#define	M_PI		3.14159265358979323846  /* pi */
#endif

/*
  Salvo Ventura - November 2006
  Added more window functions:
    * 4: Blackman
    * 5: Blackman-Harris
    * 6: Welch
    * 7: Gaussian(a=2.5)
    * 8: Gaussian(a=3.5)
    * 9: Gaussian(a=4.5)

  The window functions, in the order they are offered to the user.  Moved here
  from FFT.h, which includes this header, so that the arithmetic and the names
  it indexes by agree by construction.
*/

enum eWindowFunctions
{
   eWinFuncRectangular,
   eWinFuncBartlett,
   eWinFuncHamming,
   eWinFuncHanning,
   eWinFuncBlackman,
   eWinFuncBlackmanHarris,
   eWinFuncWelch,
   eWinFuncGaussian25,
   eWinFuncGaussian35,
   eWinFuncGaussian45,
   eWinFuncCount
};

namespace fftmath {

/*
*  Initialize the Sine table and Twiddle pointers (bit-reversed pointers)
*  for the FFT routine.  `bit_reversed` holds fftlen / 2 ints and `sin_table`
*  holds fftlen floats; the caller owns both.  This is InitializeFFT() from
*  RealFFTf.cpp with its storage taken as arguments.
*/
void InitTables(size_t fftlen, int *bit_reversed, float *sin_table);

/*
*  Forward FFT routine.  The buffer holds fftlen floats and is transformed in
*  place; output is BIT-REVERSED, so Real_i = buffer[bit_reversed[i]] and
*  Imag_i = buffer[bit_reversed[i] + 1].  Input is in normal order.  This is
*  RealFFTf() from RealFFTf.cpp.
*/
void Forward(float *buffer, const int *bit_reversed, const float *sin_table,
             size_t fftlen);

/*
*  Inverse FFT to real data, in place, on a buffer of fftlen conjugate-
*  symmetric floats.  Output is BIT-REVERSED.  This is InverseRealFFTf() from
*  RealFFTf.cpp.
*/
void Inverse(float *buffer, const int *bit_reversed, const float *sin_table,
             size_t fftlen);

/*
*  Undo the bit-reversal: copy a transformed buffer into the real and
*  imaginary halves.  This is ReorderToFreq() from RealFFTf.cpp.
*/
void ReorderToFreq(const int *bit_reversed, size_t fftlen,
                   const float *buffer, float *real_out, float *imag_out);

/*
*  Undo the bit-reversal into an interleaved time-domain buffer.  This is
*  ReorderToTime() from RealFFTf.cpp.
*/
void ReorderToTime(const int *bit_reversed, size_t fftlen,
                   const float *buffer, float *time_out);

/*
*  Real FFT with complex output, conjugate-symmetric, on a scratch buffer of
*  its own.  This is RealFFT() from FFT.cpp, minus its GetFFT() call.
*/
void RealFFT(size_t fftlen, const int *bit_reversed, const float *sin_table,
             const float *real_in, float *real_out, float *imag_out);

/*
*  The inverse of RealFFT(), above: a purely real output from conjugate-
*  symmetric input.  `imag_in` may be null, in which case it is taken as
*  zeroes.  This is InverseRealFFT() from FFT.cpp, minus its GetFFT() call.
*/
void InverseRealFFT(size_t fftlen, const int *bit_reversed,
                    const float *sin_table, const float *real_in,
                    const float *imag_in, float *real_out);

/*
*  Power spectrum: the sum of the squares of the real and imaginary parts of
*  the coefficients, with the phase thrown away.  This is PowerSpectrum() from
*  FFT.cpp, minus its GetFFT() call.
*/
void PowerSpectrum(size_t fftlen, const int *bit_reversed,
                   const float *sin_table, const float *in, float *out);

/*
*  Complex FFT, forwards or inverse.  This is FFT() from FFT.cpp; it keeps the
*  bit-reversal table that used to be gFFTBitTable, which is why the caller
*  has to release it at shutdown.
*/
void FFT(size_t NumSamples, bool InverseTransform,
         const float *RealIn, const float *ImagIn,
         float *RealOut, float *ImagOut);

/*
*  Release what FFT() caches.  This is DeinitFFT() from FFT.cpp.
*/
void DeinitFFT();

/*
*  Multiply the values in the buffer by those of the chosen window function.
*  This is NewWindowFunc() from FFT.cpp.
*/
void NewWindowFunc(int whichFunction, size_t NumSamplesIn, bool extraSample,
                   float *in);

/*
*  Multiply the values in the buffer by the derivative of the chosen window
*  function, assuming a unit sampling interval.  This is
*  DerivativeOfWindowFunc() from FFT.cpp.
*/
void DerivativeOfWindowFunc(int whichFunction, size_t NumSamples,
                            bool extraSample, float *in);

}

#endif // __RUSTACITY_FFT_MATH_H__
