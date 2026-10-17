/**********************************************************************

Rustacity: A Digital Audio Editor

Biquad.h

Norm C
Max Maisel

***********************************************************************/

#ifndef __BIQUAD_H__
#define __BIQUAD_H__

#include "MemoryX.h"

/// \brief Represents a biquad digital filter.
struct Biquad
{
   Biquad();
   void Reset();
   void Process(float* pfIn, float* pfOut, int iNumSamples);

   enum
   {
      /// Numerator coefficient indices
      B0=0, B1, B2,
      /// Denominator coefficient indices
      A1=0, A2,

      /// Possible filter orders for the Calc...Filter(...) functions
      MIN_Order = 1,
      MAX_Order = 10
   };

   double fNumerCoeffs[3]; // B0 B1 B2
   double fDenomCoeffs[2]; // A1 A2, A0 == 1.0
   double fPrevIn;
   double fPrevPrevIn;
   double fPrevOut;
   double fPrevPrevOut;

   enum kSubTypes
   {
      kLowPass,
      kHighPass,
      nSubTypes
   };

   static ArrayOf<Biquad> CalcButterworthFilter(int order, double fn, double fc, int type);
   static ArrayOf<Biquad> CalcChebyshevType1Filter(int order, double fn, double fc, double ripple, int type);
   static ArrayOf<Biquad> CalcChebyshevType2Filter(int order, double fn, double fc, double ripple, int type);

   static double ChebyPoly(int Order, double NormFreq);
};

#endif
