/**********************************************************************

  Rustacity: A Digital Audio Editor

  Screenshot.h

  Dominic Mazzoni

*******************************************************************//**

\file Screenshot.h

  Opens a modeless dialog that aids in rearranging the project window
  to a canonical size and state and then capturing full and partial
  screenshots to disk.

*//*******************************************************************/

#ifndef __RUSTACITY_SCREENSHOT__
#define __RUSTACITY_SCREENSHOT__

#include "Rustacity.h"

#include <wx/defs.h>

class RustacityProject;

void OpenScreenshotTools( RustacityProject &project );
void CloseScreenshotTools();

#endif // __RUSTACITY_SCREENSHOT__
