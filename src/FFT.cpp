/**********************************************************************

  FFT.cpp

  Dominic Mazzoni

  September 2000

*******************************************************************//*!

\file FFT.cpp
\brief Fast Fourier Transform routines.

  This file contains a few FFT routines, including a real-FFT
  routine that is almost twice as fast as a normal complex FFT,
  and a power spectrum routine when you know you don't care
  about phase information.

  Some of this code was based on a free implementation of an FFT
  by Don Cross, available on the web at:

    http://www.intersrv.com/~dcross/fft.html

  The basic algorithm for his code was based on Numerican Recipes
  in Fortran.  I optimized his code further by reducing array
  accesses, caching the bit reversal table, and eliminating
  float-to-double conversions, and I added the routines to
  calculate a real FFT and a real power spectrum.

*//*******************************************************************/
/*
  Salvo Ventura - November 2006
  Added more window functions:
    * 4: Blackman
    * 5: Blackman-Harris
    * 6: Welch
    * 7: Gaussian(a=2.5)
    * 8: Gaussian(a=3.5)
    * 9: Gaussian(a=4.5)
*/

#include "Rustacity.h"
#include "FFT.h"

#include "Internat.h"

#include "SampleFormat.h"

#include <wx/wxcrtvararg.h>
#include <wx/intl.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "RealFFTf.h"

// The arithmetic of the transforms and of the window functions is in
// FFTMath.cpp, which is free of wxWidgets so that the golden-value dumpers
// can compile it on its own.  What is left here is the table cache, the
// scratch buffers and what the user reads.

/*
 * Real Fast Fourier Transform
 *
 * This is merely a wrapper of RealFFTf() from RealFFTf.h.
 */

void RealFFT(size_t NumSamples, const float *RealIn, float *RealOut, float *ImagOut)
{
   auto hFFT = GetFFT(NumSamples);
   fftmath::RealFFT(NumSamples, hFFT->BitReversed.get(), hFFT->SinTable.get(),
                    RealIn, RealOut, ImagOut);
}

/*
 * InverseRealFFT
 *
 * This function computes the inverse of RealFFT, above.
 * The RealIn and ImagIn is assumed to be conjugate-symmetric
 * and as a result the output is purely real.
 * Only the first half of RealIn and ImagIn are used due to this
 * symmetry assumption.
 *
 * This is merely a wrapper of InverseRealFFTf() from RealFFTf.h.
 */
void InverseRealFFT(size_t NumSamples, const float *RealIn, const float *ImagIn,
		    float *RealOut)
{
   auto hFFT = GetFFT(NumSamples);
   fftmath::InverseRealFFT(NumSamples, hFFT->BitReversed.get(),
                           hFFT->SinTable.get(), RealIn, ImagIn, RealOut);
}

/*
 * PowerSpectrum
 *
 * This function uses RealFFTf() from RealFFTf.h to perform the real
 * FFT computation, and then squares the real and imaginary part of
 * each coefficient, extracting the power and throwing away the phase.
 *
 * For speed, it does not call RealFFT, but duplicates some
 * of its code.
 */

void PowerSpectrum(size_t NumSamples, const float *In, float *Out)
{
   auto hFFT = GetFFT(NumSamples);
   fftmath::PowerSpectrum(NumSamples, hFFT->BitReversed.get(),
                          hFFT->SinTable.get(), In, Out);
}

/*
 * Complex Fast Fourier Transform
 */

void FFT(size_t NumSamples,
         bool InverseTransform,
         const float *RealIn, const float *ImagIn,
	 float *RealOut, float *ImagOut)
{
   fftmath::FFT(NumSamples, InverseTransform, RealIn, ImagIn, RealOut, ImagOut);
}

void DeinitFFT()
{
   fftmath::DeinitFFT();
}

/*
 * Windowing Functions
 */

int NumWindowFuncs()
{
   return eWinFuncCount;
}

const TranslatableString WindowFuncName(int whichFunction)
{
   switch (whichFunction) {
   default:
   case eWinFuncRectangular:
      return XO("Rectangular");
   case eWinFuncBartlett:
      /* i18n-hint a proper name */
      return XO("Bartlett");
   case eWinFuncHamming:
      /* i18n-hint a proper name */
      return XO("Hamming");
   case eWinFuncHanning:
      /* i18n-hint a proper name */
      return XO("Hann");
   case eWinFuncBlackman:
      /* i18n-hint a proper name */
      return XO("Blackman");
   case eWinFuncBlackmanHarris:
      /* i18n-hint two proper names */
      return XO("Blackman-Harris");
   case eWinFuncWelch:
      /* i18n-hint a proper name */
      return XO("Welch");
   case eWinFuncGaussian25:
      /* i18n-hint a mathematical function named for C. F. Gauss */
      return XO("Gaussian(a=2.5)");
   case eWinFuncGaussian35:
      /* i18n-hint a mathematical function named for C. F. Gauss */
      return XO("Gaussian(a=3.5)");
   case eWinFuncGaussian45:
      /* i18n-hint a mathematical function named for C. F. Gauss */
      return XO("Gaussian(a=4.5)");
   }
}

void NewWindowFunc(int whichFunction, size_t NumSamplesIn, bool extraSample, float *in)
{
   fftmath::NewWindowFunc(whichFunction, NumSamplesIn, extraSample, in);
}

// See cautions in FFT.h !
void WindowFunc(int whichFunction, size_t NumSamples, float *in)
{
   bool extraSample = false;
   switch (whichFunction)
   {
   case eWinFuncHamming:
   case eWinFuncHanning:
   case eWinFuncBlackman:
   case eWinFuncBlackmanHarris:
      extraSample = true;
      break;
   default:
      break;
   case eWinFuncBartlett:
      // PRL:  Do nothing here either
      // But I want to comment that the old function did this case
      // wrong in the second half of the array, in case NumSamples was odd
      // but I think that never happened, so I am not bothering to preserve that
      break;
   }
   NewWindowFunc(whichFunction, NumSamples, extraSample, in);
}

void DerivativeOfWindowFunc(int whichFunction, size_t NumSamples, bool extraSample, float *in)
{
   fftmath::DerivativeOfWindowFunc(whichFunction, NumSamples, extraSample, in);
}
