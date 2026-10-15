/**********************************************************************

  Rustacity: A Digital Audio Editor

  Clipboard.h

  Paul Licameli

**********************************************************************/

#ifndef __RUSTACITY_CLIPBOARD__
#define __RUSTACITY_CLIPBOARD__

#include "Rustacity.h"

#include <memory>
#include <wx/event.h> // to inherit wxEvtHandler

class RustacityProject;
class TrackList;

// An event emitted by the clipboard whenever its contents change.
wxDECLARE_EXPORTED_EVENT( RUSTACITY_DLL_API,
                          EVT_CLIPBOARD_CHANGE, wxCommandEvent );

class Clipboard final
   : public wxEvtHandler
{
public:
   static Clipboard &Get();

   const TrackList &GetTracks() const;

   double T0() const { return mT0; }
   double T1() const { return mT1; }
   double Duration() const { return mT1 - mT0; }

   RustacityProject *Project() const { return mProject; }

   void Clear();
   
   void Assign(
     TrackList && newContents, double t0, double t1,
     RustacityProject *pProject );

private:
   Clipboard();
   ~Clipboard();

   std::shared_ptr<TrackList> mTracks;
   RustacityProject *mProject{};
   double mT0{ 0 };
   double mT1{ 0 };
};

#endif
