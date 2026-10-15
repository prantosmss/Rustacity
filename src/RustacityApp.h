/**********************************************************************

  Rustacity: A Digital Audio Editor

  RustacityApp.h

  Dominic Mazzoni

  This is the main source file for Audacity which handles
  initialization and termination by subclassing wxApp.

**********************************************************************/

#ifndef __RUSTACITY_APP__
#define __RUSTACITY_APP__

#include "Rustacity.h"
#include "rustacity/Types.h"

#include "Experimental.h"

#include <wx/app.h> // to inherit
#include <wx/timer.h> // member variable

#if defined(EXPERIMENTAL_CRASH_REPORT)
#include <wx/debugrpt.h> // for wxDebugReport::Context
#endif

#include "MemoryX.h"

class wxSingleInstanceChecker;
class wxSocketEvent;
class wxSocketServer;

class IPCServ;
class Importer;
class CommandHandler;
class AppCommandEvent;
class RustacityProject;

class AliasBlockFile;

class RustacityApp final : public wxApp {
 public:
   RustacityApp();
   ~RustacityApp();
   bool OnInit(void) override;
   int OnExit(void) override;
   void OnFatalException() override;
   bool OnExceptionInMainLoop() override;

   // These are currently only used on Mac OS, where it's
   // possible to have a menu bar but no windows open.  It doesn't
   // hurt any other platforms, though.
   void OnMenuAbout(wxCommandEvent & event);
   void OnMenuNew(wxCommandEvent & event);
   void OnMenuOpen(wxCommandEvent & event);
   void OnMenuPreferences(wxCommandEvent & event);
   void OnMenuExit(wxCommandEvent & event);

   void OnQueryEndSession(wxCloseEvent & event);
   void OnEndSession(wxCloseEvent & event);

   // Most Recently Used File support (for all platforms).
   void OnMRUClear(wxCommandEvent &event);
   void OnMRUFile(wxCommandEvent &event);
   // Backend for above - returns true for success, false for failure
   bool MRUOpen(const FilePath &fileName);
   // A wrapper of the above that does not throw
   bool SafeMRUOpen(const wxString &fileName);

#if defined(__WXMAC__)
   void OnWindow(wxCommandEvent &event);
#endif

   void OnReceiveCommand(AppCommandEvent &event);

   void OnKeyDown(wxKeyEvent &event);

   void OnTimer(wxTimerEvent & event);

   // IPC communication
   void OnServerEvent(wxSocketEvent & evt);
   void OnSocketEvent(wxSocketEvent & evt);

   #ifdef __WXMAC__
    // In response to Apple Events
    void MacOpenFile(const wxString &fileName)  override;
    void MacPrintFile(const wxString &fileName)  override;
    void MacNewFile()  override;
   #endif

   #if defined(__WXMSW__) && !defined(__WXUNIVERSAL__) && !defined(__CYGWIN__)
    void AssociateFileTypes();
   #endif

#ifdef __WXMAC__

   void MacActivateApp();

#endif


 private:
   std::unique_ptr<CommandHandler> mCmdHandler;

   std::unique_ptr<wxSingleInstanceChecker> mChecker;

   wxTimer mTimer;

   void InitCommandHandler();

   bool InitTempDir();
   bool CreateSingleInstanceChecker(const wxString &dir);

   std::unique_ptr<wxCmdLineParser> ParseCommandLine();

#if defined(__WXMSW__)
   std::unique_ptr<IPCServ> mIPCServ;
#else
   std::unique_ptr<wxSocketServer> mIPCServ;
#endif

 public:
    DECLARE_EVENT_TABLE()
};

extern RustacityApp & wxGetApp();

#endif
