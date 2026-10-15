/**********************************************************************
 
 Rustacity: A Digital Audio Editor
 
 SelectUtilities.h
 
 Paul Licameli split from SelectMenus.h
 
 **********************************************************************/

#ifndef __RUSTACITY_SELECT_UTILITIES__
#define __RUSTACITY_SELECT_UTILITIES__

class RustacityProject;
class Track;

/// Namespace for functions for Select menu
namespace SelectUtilities {

void DoSelectTimeAndTracks(
   RustacityProject &project, bool bAllTime, bool bAllTracks);
void SelectAllIfNone( RustacityProject &project );
void SelectNone( RustacityProject &project );
void DoListSelection(
   RustacityProject &project, Track *t,
   bool shift, bool ctrl, bool modifyState );
void DoSelectAll( RustacityProject &project );
void DoSelectAllAudio( RustacityProject &project );
void DoSelectSomething( RustacityProject &project );

}

#endif
