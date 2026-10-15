/**********************************************************************
 
 Rustacity: A Digital Audio Editor
 
 CrashReport.h
 
 Paul Licameli
 split from RustacityApp.h
 
 **********************************************************************/

#ifndef __RUSTACITY_CRASH_REPORT__
#define __RUSTACITY_CRASH_REPORT__

#include "Rustacity.h"
#include "Experimental.h"

#if defined(EXPERIMENTAL_CRASH_REPORT)

#include <wx/debugrpt.h>

namespace CrashReport
{
   void Generate(wxDebugReport::Context ctx);
}

#endif

#endif
