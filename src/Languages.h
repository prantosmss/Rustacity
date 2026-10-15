/**********************************************************************

  Rustacity: A Digital Audio Editor

  Languages.h

  Dominic Mazzoni

**********************************************************************/

#ifndef __RUSTACITY_LANGUAGES__
#define __RUSTACITY_LANGUAGES__

class wxArrayString;
class wxString;

#include "rustacity/Types.h"

void GetLanguages(
   wxArrayString &langCodes, TranslatableStrings &langNames);

wxString GetSystemLanguageCode();

#endif // __RUSTACITY_LANGUAGES__
