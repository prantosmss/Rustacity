/*
*     Program: REALFFTF.C
*      Author: Philip Van Baren
*        Date: 2 September 1993
*
* Description: These routines perform an FFT on real data to get a conjugate-symmetric
*              output, and an inverse FFT on conjugate-symmetric input to get a real
*              output sequence.
*
*              This code is for floating point data.
*
*              Modified 8/19/1998 by Philip Van Baren
*                 - made the InitializeFFT and EndFFT routines take a structure
*                   holding the length and pointers to the BitReversed and SinTable
*                   tables.
*              Modified 5/23/2009 by Philip Van Baren
*                 - Added GetFFT and ReleaseFFT routines to retain common SinTable
*                   and BitReversed tables so they don't need to be reallocated
*                   and recomputed on every call.
*                 - Added Reorder* functions to undo the bit-reversal
*
*  Copyright (C) 2009  Philip VanBaren
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "Rustacity.h"
#include "RealFFTf.h"

#include <vector>

#include <wx/thread.h>

// The arithmetic lives in FFTMath.cpp, which is free of wxWidgets so that the
// golden-value dumpers can compile it on its own.  What is left here is the
// tables and the pool of tables.
#include "FFTMath.h"

/*
*  Initialize the Sine table and Twiddle pointers (bit-reversed pointers)
*  for the FFT routine.
*/
HFFT InitializeFFT(size_t fftlen)
{
   HFFT h{ safenew FFTParam };

   /*
   *  FFT size is only half the number of data points
   *  The full FFT output can be reconstructed from this FFT's output.
   *  (This optimization can be made since the data is real.)
   */
   h->Points = fftlen / 2;

   h->SinTable.reinit(2*h->Points);

   h->BitReversed.reinit(h->Points);

   fftmath::InitTables(fftlen, h->BitReversed.get(), h->SinTable.get());

   return h;
}

enum : size_t { MAX_HFFT = 10 };

// Maintain a pool:
static std::vector< std::unique_ptr<FFTParam> > hFFTArray(MAX_HFFT);
wxCriticalSection getFFTMutex;

/* Get a handle to the FFT tables of the desired length */
/* This version keeps common tables rather than allocating a NEW table every time */
HFFT GetFFT(size_t fftlen)
{
   // To do:  smarter policy about when to retain in the pool and when to
   // allocate a unique instance.

   wxCriticalSectionLocker locker{ getFFTMutex };
   
   size_t h = 0;
   auto n = fftlen/2;
   auto size = hFFTArray.size();
   for(;
       (h < size) && hFFTArray[h] && (n != hFFTArray[h]->Points);
       h++)
      ;
   if(h < size) {
      if(hFFTArray[h] == NULL) {
         hFFTArray[h].reset( InitializeFFT(fftlen).release() );
      }
      return HFFT{ hFFTArray[h].get() };
   } else {
      // All buffers used, so fall back to allocating a NEW set of tables
      return InitializeFFT(fftlen);
   }
}

/* Release a previously requested handle to the FFT tables */
void FFTDeleter::operator() (FFTParam *hFFT) const
{
   wxCriticalSectionLocker locker{ getFFTMutex };

   auto it = hFFTArray.begin(), end = hFFTArray.end();
   while (it != end && it->get() != hFFT)
      ++it;
   if ( it != end )
      ;
   else
      delete hFFT;
}

/*
*  Forward FFT routine.  Must call GetFFT(fftlen) first!
*
*  Note: Output is BIT-REVERSED! so you must use the BitReversed to
*        get legible output, (i.e. Real_i = buffer[ h->BitReversed[i] ]
*                                  Imag_i = buffer[ h->BitReversed[i]+1 ] )
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
void RealFFTf(fft_type *buffer, const FFTParam *h)
{
   fftmath::Forward(buffer, h->BitReversed.get(), h->SinTable.get(),
                    h->Points * 2);
}


/* Description: This routine performs an inverse FFT to real data.
*              This code is for floating point data.
*
*  Note: Output is BIT-REVERSED! so you must use the BitReversed to
*        get legible output, (i.e. wave[2*i]   = buffer[ BitReversed[i] ]
*                                  wave[2*i+1] = buffer[ BitReversed[i]+1 ] )
*        Input is in normal order, interleaved (real,imaginary) complex data
*        You must call GetFFT(fftlen) first to initialize some buffers!
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
void InverseRealFFTf(fft_type *buffer, const FFTParam *h)
{
   fftmath::Inverse(buffer, h->BitReversed.get(), h->SinTable.get(),
                    h->Points * 2);
}

void ReorderToFreq(const FFTParam *hFFT, const fft_type *buffer,
		   fft_type *RealOut, fft_type *ImagOut)
{
   fftmath::ReorderToFreq(hFFT->BitReversed.get(), hFFT->Points * 2,
                          buffer, RealOut, ImagOut);
}

void ReorderToTime(const FFTParam *hFFT, const fft_type *buffer, fft_type *TimeOut)
{
   fftmath::ReorderToTime(hFFT->BitReversed.get(), hFFT->Points * 2,
                          buffer, TimeOut);
}
