/**********************************************************************

   Rustacity: A Digital Audio Editor

   FFTMath.cpp

   Philip Van Baren, Dominic Mazzoni

   The arithmetic of the real and complex FFTs and of the window functions,
   moved here unchanged from RealFFTf.cpp and FFT.cpp.  Those two files keep
   the table cache, the translated window names, the argument checks and the
   scratch buffers, and call in here - see FFTMath.h.

   The complex transform below is Don Cross's algorithm as Dominic Mazzoni
   adapted it (http://www.intersrv.com/~dcross/fft.html).

   Copyright (C) 2009  Philip VanBaren

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

***********************************************************************/

#include "FFTMath.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace fftmath {

/*
*  Initialize the Sine table and Twiddle pointers (bit-reversed pointers)
*  for the FFT routine.
*
*  This is InitializeFFT() from RealFFTf.cpp with its storage taken as
*  arguments.  `bit_reversed` holds fftlen / 2 ints and `sin_table` holds
*  fftlen floats; the caller owns both.
*/
void InitTables(size_t fftlen, int *bit_reversed, float *sin_table)
{
   const size_t points = fftlen / 2;

   for (size_t i = 0; i < points; i++)
   {
      int temp = 0;
      for (size_t mask = points / 2; mask > 0; mask >>= 1)
         temp = (temp >> 1) + int(i & mask ? points : 0);

      bit_reversed[i] = temp;
   }

   for (size_t i = 0; i < points; i++)
   {
      sin_table[bit_reversed[i]  ]=(float)-sin(2*M_PI*i/(2*points));
      sin_table[bit_reversed[i]+1]=(float)-cos(2*M_PI*i/(2*points));
   }
}

/*
*  Forward FFT routine.
*
*  Note: Output is BIT-REVERSED! so you must use the BitReversed to
*        get legible output, (i.e. Real_i = buffer[ bit_reversed[i] ]
*                                  Imag_i = buffer[ bit_reversed[i]+1 ] )
*        Input is in normal order.
*
* Output buffer[0] is the DC bin, and output buffer[1] is the Fs/2 bin
* - this can be done because both values will always be real only
* - this allows us to not have to allocate an extra complex value for the Fs/2 bin
*
*  Note: The scaling on this is done according to the standard FFT definition,
*        so a unit amplitude DC signal will output an amplitude of (N)
*        (Older revisions would progressively scale the input, so the output
*        values would be similar in amplitude to the input values, which is
*        good when using fixed point arithmetic)
*/
void Forward(float *buffer, const int *bit_reversed, const float *sin_table,
             size_t fftlen)
{
   float *A,*B;
   const float *sptr;
   const float *endptr1,*endptr2;
   const int *br1,*br2;
   float HRplus,HRminus,HIplus,HIminus;
   float v1,v2,sin,cos;

   const size_t points = fftlen / 2;
   auto ButterfliesPerGroup = points/2;

   /*
   *  Butterfly:
   *     Ain-----Aout
   *         \ /
   *         / \
   *     Bin-----Bout
   */

   endptr1 = buffer + points * 2;

   while(ButterfliesPerGroup > 0)
   {
      A = buffer;
      B = buffer + ButterfliesPerGroup * 2;
      sptr = sin_table;

      while(A < endptr1)
      {
         sin = *sptr;
         cos = *(sptr+1);
         endptr2 = B;
         while(A < endptr2)
         {
            v1 = *B * cos + *(B + 1) * sin;
            v2 = *B * sin - *(B + 1) * cos;
            *B = (*A + v1);
            *(A++) = *(B++) - 2 * v1;
            *B = (*A - v2);
            *(A++) = *(B++) + 2 * v2;
         }
         A = B;
         B += ButterfliesPerGroup * 2;
         sptr += 2;
      }
      ButterfliesPerGroup >>= 1;
   }
   /* Massage output to get the output for a real input sequence. */
   br1 = bit_reversed + 1;
   br2 = bit_reversed + points - 1;

   while(br1<br2)
   {
      sin=sin_table[*br1];
      cos=sin_table[*br1+1];
      A=buffer+*br1;
      B=buffer+*br2;
      HRplus = (HRminus = *A     - *B    ) + (*B     * 2);
      HIplus = (HIminus = *(A+1) - *(B+1)) + (*(B+1) * 2);
      v1 = (sin*HRminus - cos*HIplus);
      v2 = (cos*HRminus + sin*HIplus);
      *A = (HRplus  + v1) * (float)0.5;
      *B = *A - v1;
      *(A+1) = (HIminus + v2) * (float)0.5;
      *(B+1) = *(A+1) - HIminus;

      br1++;
      br2--;
   }
   /* Handle the center bin (just need a conjugate) */
   A=buffer+*br1+1;
   *A=-*A;
   /* Handle DC bin separately - and ignore the Fs/2 bin
   buffer[0]+=buffer[1];
   buffer[1]=(float)0;*/
   /* Handle DC and Fs/2 bins separately */
   /* Put the Fs/2 value into the imaginary part of the DC bin */
   v1=buffer[0]-buffer[1];
   buffer[0]+=buffer[1];
   buffer[1]=v1;
}

/* Description: This routine performs an inverse FFT to real data.
*              This code is for floating point data.
*
*  Note: Output is BIT-REVERSED! so you must use the BitReversed to
*        get legible output, (i.e. wave[2*i]   = buffer[ bit_reversed[i] ]
*                                  wave[2*i+1] = buffer[ bit_reversed[i]+1 ] )
*        Input is in normal order, interleaved (real,imaginary) complex data
*
* Input buffer[0] is the DC bin, and input buffer[1] is the Fs/2 bin
* - this can be done because both values will always be real only
* - this allows us to not have to allocate an extra complex value for the Fs/2 bin
*
*  Note: The scaling on this is done according to the standard FFT definition,
*        so a unit amplitude DC signal will output an amplitude of (N)
*        (Older revisions would progressively scale the input, so the output
*        values would be similar in amplitude to the input values, which is
*        good when using fixed point arithmetic)
*/
void Inverse(float *buffer, const int *bit_reversed, const float *sin_table,
             size_t fftlen)
{
   float *A,*B;
   const float *sptr;
   const float *endptr1,*endptr2;
   const int *br1;
   float HRplus,HRminus,HIplus,HIminus;
   float v1,v2,sin,cos;

   const size_t points = fftlen / 2;
   auto ButterfliesPerGroup = points / 2;

   /* Massage input to get the input for a real output sequence. */
   A = buffer + 2;
   B = buffer + points * 2 - 2;
   br1 = bit_reversed + 1;
   while(A<B)
   {
      sin=sin_table[*br1];
      cos=sin_table[*br1+1];
      HRplus = (HRminus = *A     - *B    ) + (*B     * 2);
      HIplus = (HIminus = *(A+1) - *(B+1)) + (*(B+1) * 2);
      v1 = (sin*HRminus + cos*HIplus);
      v2 = (cos*HRminus - sin*HIplus);
      *A = (HRplus  + v1) * (float)0.5;
      *B = *A - v1;
      *(A+1) = (HIminus - v2) * (float)0.5;
      *(B+1) = *(A+1) - HIminus;

      A+=2;
      B-=2;
      br1++;
   }
   /* Handle center bin (just need conjugate) */
   *(A+1)=-*(A+1);
   /* Handle DC bin separately - this ignores any Fs/2 component
   buffer[1]=buffer[0]=buffer[0]/2;*/
   /* Handle DC and Fs/2 bins specially */
   /* The DC bin is passed in as the real part of the DC complex value */
   /* The Fs/2 bin is passed in as the imaginary part of the DC complex value */
   /* (v1+v2) = buffer[0] == the DC component */
   /* (v1-v2) = buffer[1] == the Fs/2 component */
   v1=0.5f*(buffer[0]+buffer[1]);
   v2=0.5f*(buffer[0]-buffer[1]);
   buffer[0]=v1;
   buffer[1]=v2;

   /*
   *  Butterfly:
   *     Ain-----Aout
   *         \ /
   *         / \
   *     Bin-----Bout
   */

   endptr1 = buffer + points * 2;

   while(ButterfliesPerGroup > 0)
   {
      A = buffer;
      B = buffer + ButterfliesPerGroup * 2;
      sptr = sin_table;

      while(A < endptr1)
      {
         sin = *(sptr++);
         cos = *(sptr++);
         endptr2 = B;
         while(A < endptr2)
         {
            v1 = *B * cos - *(B + 1) * sin;
            v2 = *B * sin + *(B + 1) * cos;
            *B = (*A + v1) * (float)0.5;
            *(A++) = *(B++) - v1;
            *B = (*A + v2) * (float)0.5;
            *(A++) = *(B++) - v2;
         }
         A = B;
         B += ButterfliesPerGroup * 2;
      }
      ButterfliesPerGroup >>= 1;
   }
}

void ReorderToFreq(const int *bit_reversed, size_t fftlen,
                   const float *buffer, float *RealOut, float *ImagOut)
{
   const size_t points = fftlen / 2;

   // Copy the data into the real and imaginary outputs
   for(size_t i = 1; i < points; i++) {
      RealOut[i] = buffer[bit_reversed[i]  ];
      ImagOut[i] = buffer[bit_reversed[i]+1];
   }
   RealOut[0] = buffer[0]; // DC component
   ImagOut[0] = 0;
   RealOut[points] = buffer[1]; // Fs/2 component
   ImagOut[points] = 0;
}

void ReorderToTime(const int *bit_reversed, size_t fftlen,
                   const float *buffer, float *TimeOut)
{
   const size_t points = fftlen / 2;

   // Copy the data into the real outputs
   for(size_t i = 0; i < points; i++) {
      TimeOut[i*2  ]=buffer[bit_reversed[i]  ];
      TimeOut[i*2+1]=buffer[bit_reversed[i]+1];
   }
}

/*
 * Real Fast Fourier Transform
 *
 * This is merely a wrapper of the forward transform above.
 */

void RealFFT(size_t NumSamples, const int *bit_reversed,
             const float *sin_table, const float *RealIn, float *RealOut,
             float *ImagOut)
{
   std::vector<float> pFFT(NumSamples);
   // Copy the data into the processing buffer
   for(size_t i = 0; i < NumSamples; i++)
      pFFT[i] = RealIn[i];

   // Perform the FFT
   Forward(pFFT.data(), bit_reversed, sin_table, NumSamples);

   // Copy the data into the real and imaginary outputs
   for (size_t i = 1; i<(NumSamples / 2); i++) {
      RealOut[i]=pFFT[bit_reversed[i]  ];
      ImagOut[i]=pFFT[bit_reversed[i]+1];
   }
   // Handle the (real-only) DC and Fs/2 bins
   RealOut[0] = pFFT[0];
   RealOut[NumSamples / 2] = pFFT[1];
   ImagOut[0] = ImagOut[NumSamples / 2] = 0;
   // Fill in the upper half using symmetry properties
   for(size_t i = NumSamples / 2 + 1; i < NumSamples; i++) {
      RealOut[i] =  RealOut[NumSamples-i];
      ImagOut[i] = -ImagOut[NumSamples-i];
   }
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
 * This is merely a wrapper of the inverse transform above.
 */
void InverseRealFFT(size_t NumSamples, const int *bit_reversed,
                    const float *sin_table, const float *RealIn,
                    const float *ImagIn, float *RealOut)
{
   std::vector<float> pFFT(NumSamples);
   // Copy the data into the processing buffer
   for (size_t i = 0; i < (NumSamples / 2); i++)
      pFFT[2*i  ] = RealIn[i];
   if(ImagIn == nullptr) {
      for (size_t i = 0; i < (NumSamples / 2); i++)
         pFFT[2*i+1] = 0;
   } else {
      for (size_t i = 0; i < (NumSamples / 2); i++)
         pFFT[2*i+1] = ImagIn[i];
   }
   // Put the fs/2 component in the imaginary part of the DC bin
   pFFT[1] = RealIn[NumSamples / 2];

   // Perform the FFT
   Inverse(pFFT.data(), bit_reversed, sin_table, NumSamples);

   // Copy the data to the (purely real) output buffer
   ReorderToTime(bit_reversed, NumSamples, pFFT.data(), RealOut);
}

/*
 * PowerSpectrum
 *
 * This function performs the real FFT computation, and then squares the real
 * and imaginary part of each coefficient, extracting the power and throwing
 * away the phase.
 *
 * For speed, it does not call RealFFT, but duplicates some
 * of its code.
 */

void PowerSpectrum(size_t NumSamples, const int *bit_reversed,
                   const float *sin_table, const float *In, float *Out)
{
   std::vector<float> pFFT(NumSamples);
   // Copy the data into the processing buffer
   for (size_t i = 0; i<NumSamples; i++)
      pFFT[i] = In[i];

   // Perform the FFT
   Forward(pFFT.data(), bit_reversed, sin_table, NumSamples);

   // Copy the data into the real and imaginary outputs
   for (size_t i = 1; i<NumSamples / 2; i++) {
      Out[i]= (pFFT[bit_reversed[i]  ]*pFFT[bit_reversed[i]  ])
         + (pFFT[bit_reversed[i]+1]*pFFT[bit_reversed[i]+1]);
   }
   // Handle the (real-only) DC and Fs/2 bins
   Out[0] = pFFT[0]*pFFT[0];
   Out[NumSamples / 2] = pFFT[1]*pFFT[1];
}

/*
 * Complex Fast Fourier Transform
 *
 * The bit-reversal table below used to be gFFTBitTable in FFT.cpp, and these
 * functions were its InitFFT(), ReverseBits() and FastReverseBits().
 */

static std::vector< std::vector<int> > gFFTBitTable;
static const size_t MaxFastBits = 16;

static int ReverseBits(size_t index, size_t NumBits)
{
   size_t i, rev;

   for (i = rev = 0; i < NumBits; i++) {
      rev = (rev << 1) | (index & 1);
      index >>= 1;
   }

   return int(rev);
}

static void InitFFT()
{
   gFFTBitTable.assign(MaxFastBits, std::vector<int>{});

   size_t len = 2;
   for (size_t b = 1; b <= MaxFastBits; b++) {
      auto &array = gFFTBitTable[b - 1];
      array.resize(len);
      for (size_t i = 0; i < len; i++)
         array[i] = ReverseBits(i, b);

      len <<= 1;
   }
}

void DeinitFFT()
{
   gFFTBitTable.clear();
   gFFTBitTable.shrink_to_fit();
}

static inline size_t FastReverseBits(size_t i, size_t NumBits)
{
   if (NumBits <= MaxFastBits)
      return gFFTBitTable[NumBits - 1][i];
   else
      return ReverseBits(i, NumBits);
}

static bool IsPowerOfTwo(size_t x);
static size_t NumberOfBitsNeeded(size_t PowerOfTwo);

/*
 * Complex Fast Fourier Transform
 */

void FFT(size_t NumSamples,
         bool InverseTransform,
         const float *RealIn, const float *ImagIn,
         float *RealOut, float *ImagOut)
{
   double angle_numerator = 2.0 * M_PI;
   double tr, ti;                /* temp real, temp imaginary */

   if (!IsPowerOfTwo(NumSamples)) {
      fprintf(stderr, "%zu is not a power of two\n", NumSamples);
      exit(1);
   }

   if (gFFTBitTable.empty())
      InitFFT();

   if (!InverseTransform)
      angle_numerator = -angle_numerator;

   /* Number of bits needed to store indices */
   auto NumBits = NumberOfBitsNeeded(NumSamples);

   /*
    **   Do simultaneous data copy and bit-reversal ordering into outputs...
    */

   for (size_t i = 0; i < NumSamples; i++) {
      auto j = FastReverseBits(i, NumBits);
      RealOut[j] = RealIn[i];
      ImagOut[j] = (ImagIn == nullptr) ? 0.0 : ImagIn[i];
   }

   /*
    **   Do the FFT itself...
    */

   size_t BlockEnd = 1;
   for (size_t BlockSize = 2; BlockSize <= NumSamples; BlockSize <<= 1) {

      double delta_angle = angle_numerator / (double) BlockSize;

      double sm2 = sin(-2 * delta_angle);
      double sm1 = sin(-delta_angle);
      double cm2 = cos(-2 * delta_angle);
      double cm1 = cos(-delta_angle);
      double w = 2 * cm1;
      double ar0, ar1, ar2, ai0, ai1, ai2;

      for (size_t i = 0; i < NumSamples; i += BlockSize) {
         ar2 = cm2;
         ar1 = cm1;

         ai2 = sm2;
         ai1 = sm1;

         for (size_t j = i, n = 0; n < BlockEnd; j++, n++) {
            ar0 = w * ar1 - ar2;
            ar2 = ar1;
            ar1 = ar0;

            ai0 = w * ai1 - ai2;
            ai2 = ai1;
            ai1 = ai0;

            size_t k = j + BlockEnd;
            tr = ar0 * RealOut[k] - ai0 * ImagOut[k];
            ti = ar0 * ImagOut[k] + ai0 * RealOut[k];

            RealOut[k] = RealOut[j] - tr;
            ImagOut[k] = ImagOut[j] - ti;

            RealOut[j] += tr;
            ImagOut[j] += ti;
         }
      }

      BlockEnd = BlockSize;
   }

   /*
      **   Need to normalize if inverse transform...
    */

   if (InverseTransform) {
      float denom = (float) NumSamples;

      for (size_t i = 0; i < NumSamples; i++) {
         RealOut[i] /= denom;
         ImagOut[i] /= denom;
      }
   }
}

static bool IsPowerOfTwo(size_t x)
{
   if (x < 2)
      return false;

   if (x & (x - 1))             /* Thanks to 'byang' for this cute trick! */
      return false;

   return true;
}

static size_t NumberOfBitsNeeded(size_t PowerOfTwo)
{
   if (PowerOfTwo < 2) {
      fprintf(stderr, "Error: FFT called with size %zu\n", PowerOfTwo);
      exit(1);
   }

   size_t i = 0;
   while (PowerOfTwo > 1)
      PowerOfTwo >>= 1, ++i;

   return i;
}

/*
 * Windowing Functions
 */

void NewWindowFunc(int whichFunction, size_t NumSamplesIn, bool extraSample, float *in)
{
   int NumSamples = (int)NumSamplesIn;
   if (extraSample) {
      assert(NumSamples > 0);
      --NumSamples;
   }
   assert(NumSamples > 0);

   switch (whichFunction) {
   default:
      fprintf(stderr, "FFT::WindowFunc - Invalid window function: %d\n", whichFunction);
      break;
   case eWinFuncRectangular:
      // Multiply all by 1.0f -- do nothing
      break;

   case eWinFuncBartlett:
   {
      // Bartlett (triangular) window
      const int nPairs = (NumSamples - 1) / 2; // whether even or odd NumSamples, this is correct
      const float denom = NumSamples / 2.0f;
      in[0] = 0.0f;
      for (int ii = 1;
           ii <= nPairs; // Yes, <=
           ++ii) {
         const float value = ii / denom;
         in[ii] *= value;
         in[NumSamples - ii] *= value;
      }
      // When NumSamples is even, in[half] should be multiplied by 1.0, so unchanged
      // When odd, the value of 1.0 is not reached
   }
      break;
   case eWinFuncHamming:
   {
      // Hamming
      const double multiplier = 2 * M_PI / NumSamples;
      static const double coeff0 = 0.54, coeff1 = -0.46;
      for (int ii = 0; ii < NumSamples; ++ii)
         in[ii] *= coeff0 + coeff1 * cos(ii * multiplier);
   }
      break;
   case eWinFuncHanning:
   {
      // Hann
      const double multiplier = 2 * M_PI / NumSamples;
      static const double coeff0 = 0.5, coeff1 = -0.5;
      for (int ii = 0; ii < NumSamples; ++ii)
         in[ii] *= coeff0 + coeff1 * cos(ii * multiplier);
   }
      break;
   case eWinFuncBlackman:
   {
      // Blackman
      const double multiplier = 2 * M_PI / NumSamples;
      const double multiplier2 = 2 * multiplier;
      static const double coeff0 = 0.42, coeff1 = -0.5, coeff2 = 0.08;
      for (int ii = 0; ii < NumSamples; ++ii)
         in[ii] *= coeff0 + coeff1 * cos(ii * multiplier) + coeff2 * cos(ii * multiplier2);
   }
      break;
   case eWinFuncBlackmanHarris:
   {
      // Blackman-Harris
      const double multiplier = 2 * M_PI / NumSamples;
      const double multiplier2 = 2 * multiplier;
      const double multiplier3 = 3 * multiplier;
      static const double coeff0 = 0.35875, coeff1 = -0.48829, coeff2 = 0.14128, coeff3 = -0.01168;
      for (int ii = 0; ii < NumSamples; ++ii)
         in[ii] *= coeff0 + coeff1 * cos(ii * multiplier) + coeff2 * cos(ii * multiplier2) + coeff3 * cos(ii * multiplier3);
   }
      break;
   case eWinFuncWelch:
   {
      // Welch
      const float N = NumSamples;
      for (int ii = 0; ii < NumSamples; ++ii) {
         const float iOverN = ii / N;
         in[ii] *= 4 * iOverN * (1 - iOverN);
      }
   }
      break;
   case eWinFuncGaussian25:
   {
      // Gaussian (a=2.5)
      // Precalculate some values, and simplify the fmla to try and reduce overhead
      static const double A = -2 * 2.5*2.5;
      const float N = NumSamples;
      for (int ii = 0; ii < NumSamples; ++ii) {
         const float iOverN = ii / N;
         // full
         // in[ii] *= exp(-0.5*(A*((ii-NumSamples/2)/NumSamples/2))*(A*((ii-NumSamples/2)/NumSamples/2)));
         // reduced
         in[ii] *= exp(A * (0.25 + (iOverN * iOverN) - iOverN));
      }
   }
      break;
   case eWinFuncGaussian35:
   {
      // Gaussian (a=3.5)
      static const double A = -2 * 3.5*3.5;
      const float N = NumSamples;
      for (int ii = 0; ii < NumSamples; ++ii) {
         const float iOverN = ii / N;
         in[ii] *= exp(A * (0.25 + (iOverN * iOverN) - iOverN));
      }
   }
      break;
   case eWinFuncGaussian45:
   {
      // Gaussian (a=4.5)
      static const double A = -2 * 4.5*4.5;
      const float N = NumSamples;
      for (int ii = 0; ii < NumSamples; ++ii) {
         const float iOverN = ii / N;
         in[ii] *= exp(A * (0.25 + (iOverN * iOverN) - iOverN));
      }
   }
      break;
   }

   if (extraSample && whichFunction != eWinFuncRectangular) {
      double value = 0.0;
      switch (whichFunction) {
      case eWinFuncHamming:
         value = 0.08;
         break;
      case eWinFuncGaussian25:
         value = exp(-2 * 2.5 * 2.5 * 0.25);
         break;
      case eWinFuncGaussian35:
         value = exp(-2 * 3.5 * 3.5 * 0.25);
         break;
      case eWinFuncGaussian45:
         value = exp(-2 * 4.5 * 4.5 * 0.25);
         break;
      default:
         break;
      }
      in[NumSamples] *= value;
   }
}

void DerivativeOfWindowFunc(int whichFunction, size_t NumSamples, bool extraSample, float *in)
{
   if (eWinFuncRectangular == whichFunction)
   {
      // Rectangular
      // There are deltas at the ends
      assert(NumSamples > 0);
      --NumSamples;
      // in[0] *= 1.0f;
      for (int ii = 1; ii < (int)NumSamples; ++ii)
         in[ii] = 0.0f;
      in[NumSamples] *= -1.0f;
      return;
   }

   if (extraSample) {
      assert(NumSamples > 0);
      --NumSamples;
   }

   assert(NumSamples > 0);

   double A;
   switch (whichFunction) {
   case eWinFuncBartlett:
   {
      // Bartlett (triangular) window
      // There are discontinuities in the derivative at the ends, and maybe at the midpoint
      const int nPairs = (NumSamples - 1) / 2; // whether even or odd NumSamples, this is correct
      const float value = 2.0f / NumSamples;
      in[0] *=
         // Average the two limiting values of discontinuous derivative
         value / 2.0f;
      for (int ii = 1;
         ii <= nPairs; // Yes, <=
         ++ii) {
         in[ii] *= value;
         in[NumSamples - ii] *= -value;
      }
      if (NumSamples % 2 == 0)
         // Average the two limiting values of discontinuous derivative
         in[NumSamples / 2] = 0.0f;
      if (extraSample)
         in[NumSamples] *=
         // Average the two limiting values of discontinuous derivative
            -value / 2.0f;
      else
         // Halve the multiplier previously applied
         // Average the two limiting values of discontinuous derivative
         in[NumSamples - 1] *= 0.5f;
   }
      break;
   case eWinFuncHamming:
   {
      // Hamming
      // There are deltas at the ends
      const double multiplier = 2 * M_PI / NumSamples;
      // The slope used to be a function-local static, so it was initialised
      // once, from the first call's window size, and every later call at a
      // different size multiplied a stale slope by a fresh multiplier.  The
      // derivative window of a spectrogram came out wrong as soon as the
      // window size changed during a session (SpectrogramSettings.cpp calls
      // this with the user's size).  The slope belongs to the call.
      const double coeff0 = 0.54, coeff1 = -0.46 * multiplier;
      // TODO This code should be more explicit about the precision it intends.
      // For now we get C4305 warnings, truncation from 'const double' to 'float'
      in[0] *= coeff0;
      if (!extraSample)
         --NumSamples;
      for (int ii = 0; ii < (int)NumSamples; ++ii)
         in[ii] *= - coeff1 * sin(ii * multiplier);
      if (extraSample)
         in[NumSamples] *= - coeff0;
      else
         // slightly different
         in[NumSamples] *= - coeff0 - coeff1 * sin(NumSamples * multiplier);
   }
      break;
   case eWinFuncHanning:
   {
      // Hann
      const double multiplier = 2 * M_PI / NumSamples;
      const double coeff1 = -0.5 * multiplier;
      for (int ii = 0; ii < (int)NumSamples; ++ii)
         in[ii] *= - coeff1 * sin(ii * multiplier);
      if (extraSample)
         in[NumSamples] = 0.0f;
   }
      break;
   case eWinFuncBlackman:
   {
      // Blackman
      const double multiplier = 2 * M_PI / NumSamples;
      const double multiplier2 = 2 * multiplier;
      const double coeff1 = -0.5 * multiplier, coeff2 = 0.08 * multiplier2;
      for (int ii = 0; ii < (int)NumSamples; ++ii)
         in[ii] *= - coeff1 * sin(ii * multiplier) - coeff2 * sin(ii * multiplier2);
      if (extraSample)
         in[NumSamples] = 0.0f;
   }
      break;
   case eWinFuncBlackmanHarris:
   {
      // Blackman-Harris
      const double multiplier = 2 * M_PI / NumSamples;
      const double multiplier2 = 2 * multiplier;
      const double multiplier3 = 3 * multiplier;
      const double coeff1 = -0.48829 * multiplier,
         coeff2 = 0.14128 * multiplier2, coeff3 = -0.01168 * multiplier3;
      for (int ii = 0; ii < (int)NumSamples; ++ii)
         in[ii] *= - coeff1 * sin(ii * multiplier) - coeff2 * sin(ii * multiplier2) - coeff3 * sin(ii * multiplier3);
      if (extraSample)
         in[NumSamples] = 0.0f;
   }
      break;
   case eWinFuncWelch:
   {
      // Welch
      const float N = NumSamples;
      const float NN = NumSamples * NumSamples;
      for (int ii = 0; ii < (int)NumSamples; ++ii) {
         in[ii] *= 4 * (N - ii - ii) / NN;
      }
      if (extraSample)
         in[NumSamples] = 0.0f;
      // Average the two limiting values of discontinuous derivative
      in[0] /= 2.0f;
      in[NumSamples - 1] /= 2.0f;
   }
      break;
   case eWinFuncGaussian25:
      // Gaussian (a=2.5)
      A = -2 * 2.5*2.5;
      goto Gaussian;
   case eWinFuncGaussian35:
      // Gaussian (a=3.5)
      A = -2 * 3.5*3.5;
      goto Gaussian;
   case eWinFuncGaussian45:
      // Gaussian (a=4.5)
      A = -2 * 4.5*4.5;
      goto Gaussian;
   Gaussian:
   {
      // Gaussian (a=2.5)
      // There are deltas at the ends
      const float invN = 1.0f / NumSamples;
      const float invNN = invN * invN;
      // Simplify formula from the loop for ii == 0, add term for the delta
      in[0] *= exp(A * 0.25) * (1 - invN);
      if (!extraSample)
         --NumSamples;
      for (int ii = 1; ii < (int)NumSamples; ++ii) {
         const float iOverN = ii * invN;
         in[ii] *= exp(A * (0.25 + (iOverN * iOverN) - iOverN)) * (2 * ii * invNN - invN);
      }
      if (extraSample)
         in[NumSamples] *= exp(A * 0.25) * (invN - 1);
      else {
         // Slightly different
         const float iOverN = NumSamples * invN;
         in[NumSamples] *= exp(A * (0.25 + (iOverN * iOverN) - iOverN)) * (2 * NumSamples * invNN - invN - 1);
      }
   }
      break;
   default:
      fprintf(stderr, "FFT::DerivativeOfWindowFunc - Invalid window function: %d\n", whichFunction);
   }
}

}
