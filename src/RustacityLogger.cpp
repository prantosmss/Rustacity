/**********************************************************************

  Rustacity: A Digital Audio Editor

  RustacityLogger.cpp

******************************************************************//**

\class RustacityLogger
\brief RustacityLogger is a thread-safe logger class

Provides thread-safe logging based on the wxWidgets log facility.

*//*******************************************************************/


#include "Rustacity.h" // This should always be included first
#include "RustacityLogger.h"

#include "Experimental.h"

#include "FileNames.h"
#include "Internat.h"
#include "ShuttleGui.h"

#include <mutex>
#include <wx/filedlg.h>
#include <wx/log.h>
#include <wx/frame.h>
#include <wx/icon.h>
#include <wx/settings.h>
#include <wx/textctrl.h>

#include "../images/RustacityLogoAlpha.xpm"
#include "widgets/RustacityMessageBox.h"

//
// RustacityLogger class
//
// Two reasons for this class instead of the wxLogWindow class (or any WX GUI logging class)
//
// 1)  If wxLogWindow is used and initialized before the Mac's "root" window, then
//     Audacity may crash when terminating.  It's not fully understood why this occurs
//     but it probably has to do with the order of deletion.  However, deferring the
//     creation of the log window until it is actually shown circumvents the problem.
// 2)  By providing an Audacity specific logging class, it can be made thread-safe and,
//     as such, can be used by the ever growing threading within Audacity.
//
enum
{
   LoggerID_Save = wxID_HIGHEST + 1,
   LoggerID_Clear,
   LoggerID_Close
};

RustacityLogger *RustacityLogger::Get()
{
   static std::once_flag flag;
   std::call_once( flag, []{
      // wxWidgets will clean up the logger for the main thread, so we can say
      // safenew.  See:
      // http://docs.wxwidgets.org/3.0/classwx_log.html#a2525bf54fa3f31dc50e6e3cd8651e71d
      std::unique_ptr < wxLog > // DELETE any previous logger
         { wxLog::SetActiveTarget(safenew RustacityLogger) };
   } );

   // Use dynamic_cast so that we get a NULL ptr in case our logger
   // is no longer the target.
   return dynamic_cast<RustacityLogger *>(wxLog::GetActiveTarget());
}

RustacityLogger::RustacityLogger()
:  wxEvtHandler(),
   wxLog()
{
   mText = NULL;
   mUpdated = false;
}

void RustacityLogger::Flush()
{
   if (mUpdated && mFrame && mFrame->IsShown()) {
      mUpdated = false;
      mText->ChangeValue(mBuffer);
   }
}

void RustacityLogger::DoLogText(const wxString & str)
{
   if (!wxIsMainThread()) {
      wxMutexGuiEnter();
   }

   if (mBuffer.empty()) {
      wxString stamp;

      TimeStamp(&stamp);

      mBuffer << stamp << _TS("Audacity ") << RUSTACITY_VERSION_STRING << wxT("\n");
   }

   mBuffer << str << wxT("\n");

   mUpdated = true;

   Flush();

   if (!wxIsMainThread()) {
      wxMutexGuiLeave();
   }
}

void RustacityLogger::Show(bool show)
{
   // Hide the frame if created, otherwise do nothing
   if (!show) {
      if (mFrame) {
         mFrame->Show(false);
      }
      return;
   }

   // If the frame already exists, refresh its contents and show it
   if (mFrame) {
      if (!mFrame->IsShown()) {
         mText->ChangeValue(mBuffer);
         mText->SetInsertionPointEnd();
         mText->ShowPosition(mText->GetLastPosition());
      }
      mFrame->Show();
      mFrame->Raise();
      return;
   }

   // This is the first use, so create the frame
   Destroy_ptr<wxFrame> frame
      { safenew wxFrame(NULL, wxID_ANY, _("Audacity Log")) };
   frame->SetName(frame->GetTitle());
   frame->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));

   // loads either the XPM or the windows resource, depending on the platform
   {
#if !defined(__WXMAC__) && !defined(__WXX11__)
#if defined(__WXMSW__)
      wxIcon ic{wxICON(RustacityLogo)};
#elif defined(__WXGTK__)
      wxIcon ic{wxICON(RustacityLogoAlpha)};
#else
      wxIcon ic{};
      ic.CopyFromBitmap(theTheme.Bitmap(bmpAudacityLogo48x48));
#endif
      frame->SetIcon(ic);
#endif
   }

   // Log text
   ShuttleGui S(frame.get(), eIsCreating);

   S.Style(wxNO_BORDER | wxTAB_TRAVERSAL).Prop(true).StartPanel();
   {
      S.StartVerticalLay(true);
      {
         mText = S.Style(wxTE_MULTILINE | wxHSCROLL | wxTE_READONLY | wxTE_RICH)
            .AddTextWindow(mBuffer);

         S.AddSpace(0, 5);
         S.StartHorizontalLay(wxALIGN_CENTER, 0);
         {
            S.AddSpace(10, 0);
            S.Id(LoggerID_Save).AddButton(XO("&Save..."));
            S.Id(LoggerID_Clear).AddButton(XO("Cl&ear"));
            S.Id(LoggerID_Close).AddButton(XO("&Close"));
            S.AddSpace(10, 0);
         }
         S.EndHorizontalLay();
         S.AddSpace(0, 3);
      }
      S.EndVerticalLay();
   }
   S.EndPanel();

   // Give a place for the menu help text to go
   // frame->CreateStatusBar();

   frame->Layout();

   // Hook into the frame events
   frame->Bind(wxEVT_CLOSE_WINDOW,
                  wxCloseEventHandler(RustacityLogger::OnCloseWindow),
                  this);

   frame->Bind(   wxEVT_COMMAND_MENU_SELECTED,
                  &RustacityLogger::OnSave,
                  this, LoggerID_Save);
   frame->Bind(   wxEVT_COMMAND_MENU_SELECTED,
                  &RustacityLogger::OnClear,
                  this, LoggerID_Clear);
   frame->Bind(   wxEVT_COMMAND_MENU_SELECTED,
                  &RustacityLogger::OnClose,
                  this, LoggerID_Close);
   frame->Bind(   wxEVT_COMMAND_BUTTON_CLICKED,
                  &RustacityLogger::OnSave,
                  this, LoggerID_Save);
   frame->Bind(   wxEVT_COMMAND_BUTTON_CLICKED,
                  &RustacityLogger::OnClear,
                  this, LoggerID_Clear);
   frame->Bind(   wxEVT_COMMAND_BUTTON_CLICKED,
                  &RustacityLogger::OnClose,
                  this, LoggerID_Close);

   mFrame = std::move( frame );

   mFrame->Show();

   Flush();
}

#if defined(EXPERIMENTAL_CRASH_REPORT)
wxString RustacityLogger::GetLog()
{
   return mBuffer;
}
#endif

void RustacityLogger::OnCloseWindow(wxCloseEvent & WXUNUSED(e))
{
#if defined(__WXMAC__)
   // On the Mac, destroy the window rather than hiding it since the
   // log menu will override the root windows menu if there is no
   // project window open.
   mFrame.reset();
#else
   Show(false);
#endif
}

void RustacityLogger::OnClose(wxCommandEvent & WXUNUSED(e))
{
   wxCloseEvent dummy;
   OnCloseWindow(dummy);
}

void RustacityLogger::OnClear(wxCommandEvent & WXUNUSED(e))
{
   mBuffer = wxEmptyString;
   DoLogText(wxT("Log Cleared."));
}

void RustacityLogger::OnSave(wxCommandEvent & WXUNUSED(e))
{
   wxString fName = _("log.txt");

   fName = FileNames::SelectFile(FileNames::Operation::Export,
      XO("Save log to:"),
      wxEmptyString,
      fName,
      wxT("txt"),
      { FileNames::TextFiles },
      wxFD_SAVE | wxFD_OVERWRITE_PROMPT | wxRESIZE_BORDER,
      mFrame.get());

   if (fName.empty()) {
      return;
   }

   if (!mText->SaveFile(fName)) {
      RustacityMessageBox(
         XO("Couldn't save log to file: %s").Format( fName ),
         XO("Warning"),
         wxICON_EXCLAMATION,
         mFrame.get());
      return;
   }
}

