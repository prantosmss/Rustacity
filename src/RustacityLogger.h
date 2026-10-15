/**********************************************************************

  Rustacity: A Digital Audio Editor

  RustacityLogger.h

  Dominic Mazzoni

  This is the main source file for Audacity which handles
  initialization and termination by subclassing wxApp.

**********************************************************************/

#ifndef __RUSTACITY_LOGGER__
#define __RUSTACITY_LOGGER__

#include "Rustacity.h"

#include "Experimental.h"

#include "MemoryX.h"
#include <wx/log.h> // to inherit
#include <wx/event.h> // to inherit wxEvtHandler

class wxFrame;
class wxTextCtrl;

class RustacityLogger final : public wxEvtHandler, public wxLog {
 public:
 
   // Get the singleton instance or null
   static RustacityLogger *Get();

   void Show(bool show = true);

#if defined(EXPERIMENTAL_CRASH_REPORT)
   wxString GetLog();
#endif

 protected:
   void Flush()  override;
   void DoLogText(const wxString & msg) override;

 private:
   RustacityLogger();

   void OnCloseWindow(wxCloseEvent & e);
   void OnClose(wxCommandEvent & e);
   void OnClear(wxCommandEvent & e);
   void OnSave(wxCommandEvent & e);

   Destroy_ptr<wxFrame> mFrame;
   wxTextCtrl *mText;
   wxString mBuffer;
   bool mUpdated;
};

#endif
