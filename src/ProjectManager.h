/**********************************************************************

Rustacity: A Digital Audio Editor

ProjectManager.h

Paul Licameli split from RustacityProject.h

**********************************************************************/

#ifndef __RUSTACITY_PROJECT_MANAGER__
#define __RUSTACITY_PROJECT_MANAGER__

#include <memory>

#include <wx/event.h> // to inherit
#include "ClientData.h" // to inherit

class wxTimer;
class wxTimerEvent;

class RustacityProject;
struct AudioIOStartStreamOptions;

///\brief Object associated with a project for high-level management of the
/// project's lifetime, including creation, destruction, opening from file,
/// importing, pushing undo states, and reverting to saved states
class ProjectManager final
   : public wxEvtHandler
   , public ClientData::Base
{
public:
   static ProjectManager &Get( RustacityProject &project );
   static const ProjectManager &Get( const RustacityProject &project );

   explicit ProjectManager( RustacityProject &project );
   ProjectManager( const ProjectManager & ) PROHIBITED;
   ProjectManager &operator=( const ProjectManager & ) PROHIBITED;
   ~ProjectManager() override;

   // This is the factory for projects:
   static RustacityProject *New();

   // The function that imports files can act as a factory too, and for that
   // reason remains in this class, not in ProjectFileManager
   static void OpenFiles(RustacityProject *proj);

   // Return the given project if that is not NULL, else create a project.
   // Then open the given project path.
   // But if an exception escapes this function, create no NEW project.
   static RustacityProject *OpenProject(
      RustacityProject *pProject,
      const FilePath &fileNameArg, bool addtohistory = true);

   void ResetProjectToEmpty();

   static void SaveWindowSize();

   // Routine to estimate how many minutes of recording time are left on disk
   int GetEstimatedRecordingMinsLeftOnDisk(long lCaptureChannels = 0);
   // Converts number of minutes to human readable format
   TranslatableString GetHoursMinsString(int iMinutes);

   void SetStatusText( const TranslatableString &text, int number );
   void SetSkipSavePrompt(bool bSkip) { sbSkipPromptingForSave = bSkip; };

private:
   void OnCloseWindow(wxCloseEvent & event);
   void OnTimer(wxTimerEvent & event);
   void OnOpenAudioFile(wxCommandEvent & event);
   void OnStatusChange( wxCommandEvent& );

   void RestartTimer();

   // non-static data members
   RustacityProject &mProject;

   std::unique_ptr<wxTimer> mTimer;

   DECLARE_EVENT_TABLE()

   static bool sbWindowRectAlreadySaved;
   static bool sbSkipPromptingForSave;
};

#endif
