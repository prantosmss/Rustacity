/**********************************************************************

Rustacity: A Digital Audio Editor

ProjectWindowBase.h

Paul Licameli split from ProjectWindow.h

**********************************************************************/

#ifndef __RUSTACITY_PROJECT_WINDOW_BASE__
#define __RUSTACITY_PROJECT_WINDOW_BASE__

#include <wx/frame.h> // to inherit

class RustacityProject;

///\brief A top-level window associated with a project
class ProjectWindowBase /* not final */ : public wxFrame
{
public:
   explicit ProjectWindowBase(
      wxWindow * parent, wxWindowID id,
      const wxPoint & pos, const wxSize &size,
      RustacityProject &project );

   ~ProjectWindowBase() override;

   RustacityProject &GetProject() { return mProject; }
   const RustacityProject &GetProject() const { return mProject; }

protected:
   RustacityProject &mProject;
};

RustacityProject *FindProjectFromWindow( wxWindow *pWindow );
const RustacityProject *FindProjectFromWindow( const wxWindow *pWindow );

#endif

