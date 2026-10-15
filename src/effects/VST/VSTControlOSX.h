/**********************************************************************

  Rustacity: A Digital Audio Editor

  VSTControlOSX.h

  Leland Lucius

**********************************************************************/

#ifndef RUSTACITY_VSTCONTROLOSX_H
#define RUSTACITY_VSTCONTROLOSX_H

#if !defined(_LP64)
#include <Carbon/Carbon.h>
#endif

#include <wx/osx/private.h> // to inherit wxWidgetCocoaImpl

#include "VSTControl.h"

class VSTControlImpl final : public wxWidgetCocoaImpl
{
public :
   VSTControlImpl(wxWindowMac *peer, NSView *view);
   ~VSTControlImpl();
};

class VSTControl : public VSTControlBase
{
public:
   VSTControl();
   ~VSTControl();

   bool Create(wxWindow *parent, VSTEffectLink *link);
   void Close();

private:
   void CreateCocoa();

#if !defined(_LP64)
   void CreateCarbon();
   void OnSize(wxSizeEvent & evt);
#endif

private:
   NSView *mVSTView;
   NSView *mView;

#if !defined(_LP64)
   WindowRef mWindowRef;
   HIViewRef mHIView;
#endif
};

#endif
