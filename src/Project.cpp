/**********************************************************************

  Rustacity: A Digital Audio Editor

  Project.cpp

  Dominic Mazzoni
  Vaughan Johnson

*//*******************************************************************/

#include "Rustacity.h" // for USE_* macros
#include "Project.h"

#include "KeyboardCapture.h"
#include "ondemand/ODTaskThread.h"

#if defined(__WXMAC__)
#include "menus/WindowMenus.h"
#endif

#include <wx/display.h>
#include <wx/frame.h>

wxDEFINE_EVENT(EVT_TRACK_PANEL_TIMER, wxCommandEvent);

size_t AllProjects::size() const
{
   return gAudacityProjects.size();
}

auto AllProjects::begin() const -> const_iterator
{
   return gAudacityProjects.begin();
}

auto AllProjects::end() const -> const_iterator
{
   return gAudacityProjects.end();
}

auto AllProjects::rbegin() const -> const_reverse_iterator
{
   return gAudacityProjects.rbegin();
}

auto AllProjects::rend() const -> const_reverse_iterator
{
   return gAudacityProjects.rend();
}

auto AllProjects::Remove( RustacityProject &project ) -> value_type
{
   ODLocker locker{ &Mutex() };
   auto start = begin(), finish = end(), iter = std::find_if(
      start, finish,
      [&]( const value_type &ptr ){ return ptr.get() == &project; }
   );
   if (iter == finish)
      return nullptr;
   auto result = *iter;
   gAudacityProjects.erase( iter );
   return result;
}

void AllProjects::Add( const value_type &pProject )
{
   ODLocker locker{ &Mutex() };
   gAudacityProjects.push_back( pProject );
}

bool AllProjects::sbClosing = false;

bool AllProjects::Close( bool force )
{
   ValueRestorer<bool> cleanup{ sbClosing, true };
   while (AllProjects{}.size())
   {
      // Closing the project has global side-effect
      // of deletion from gAudacityProjects
      if ( force )
      {
         GetProjectFrame( **AllProjects{}.begin() ).Close(true);
      }
      else
      {
         if (! GetProjectFrame( **AllProjects{}.begin() ).Close())
            return false;
      }
   }
   return true;
}

ODLock &AllProjects::Mutex()
{
   static ODLock theMutex;
   return theMutex;
};

int RustacityProject::mProjectCounter=0;// global counter.

/* Define Global Variables */
//This is a pointer to the currently-active project.
static RustacityProject *gActiveProject;
//This array holds onto all of the projects currently open
AllProjects::Container AllProjects::gAudacityProjects;

RUSTACITY_DLL_API RustacityProject *GetActiveProject()
{
   return gActiveProject;
}

void SetActiveProject(RustacityProject * project)
{
   if ( gActiveProject != project ) {
      gActiveProject = project;
      KeyboardCapture::Capture( nullptr );
   }
   wxTheApp->SetTopWindow( FindProjectFrame( project ) );

#if defined(__WXMAC__)
   // Refresh the Window menu
   WindowActions::Refresh();
#endif
}

RustacityProject::RustacityProject()
{
   mProjectNo = mProjectCounter++; // Bug 322
   AttachedObjects::BuildAll();
   // But not for the attached windows.  They get built only on demand, such as
   // from menu items.
}

RustacityProject::~RustacityProject()
{
}

void RustacityProject::SetFrame( wxFrame *pFrame )
{
   mFrame = pFrame;
}

void RustacityProject::SetPanel( wxWindow *pPanel )
{
   mPanel = pPanel;
}

wxString RustacityProject::GetProjectName() const
{
   wxString name = wxFileNameFromPath(mFileName);

   // Chop off the extension
   size_t len = name.length();
   if (len > 4 && name.Mid(len - 4) == wxT(".aup"))
      name = name.Mid(0, len - 4);

   return name;
}

RUSTACITY_DLL_API wxFrame &GetProjectFrame( RustacityProject &project )
{
   auto ptr = project.GetFrame();
   if ( !ptr )
      THROW_INCONSISTENCY_EXCEPTION;
   return *ptr;
}

RUSTACITY_DLL_API const wxFrame &GetProjectFrame( const RustacityProject &project )
{
   auto ptr = project.GetFrame();
   if ( !ptr )
      THROW_INCONSISTENCY_EXCEPTION;
   return *ptr;
}

RUSTACITY_DLL_API wxWindow &GetProjectPanel( RustacityProject &project )
{
   auto ptr = project.GetPanel();
   if ( !ptr )
      THROW_INCONSISTENCY_EXCEPTION;
   return *ptr;
}

RUSTACITY_DLL_API const wxWindow &GetProjectPanel(
   const RustacityProject &project )
{
   auto ptr = project.GetPanel();
   if ( !ptr )
      THROW_INCONSISTENCY_EXCEPTION;
   return *ptr;
}
