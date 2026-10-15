/**********************************************************************

  Rustacity: A Digital Audio Editor

  RustacityMessageBox.h

  Paul Licameli split this out of ErrorDialog.h

**********************************************************************/

#ifndef __RUSTACITY_MESSAGE_BOX__
#define __RUSTACITY_MESSAGE_BOX__

#include <wx/msgdlg.h>
#include "../Internat.h"

extern TranslatableString RustacityMessageBoxCaptionStr();

// Do not use wxMessageBox!!  Its default window title does not translate!
inline int RustacityMessageBox(const TranslatableString& message,
   const TranslatableString& caption = RustacityMessageBoxCaptionStr(),
   long style = wxOK | wxCENTRE,
   wxWindow *parent = NULL,
   int x = wxDefaultCoord, int y = wxDefaultCoord)
{
   return ::wxMessageBox(message.Translation(), caption.Translation(),
      style, parent, x, y);
}

#endif
