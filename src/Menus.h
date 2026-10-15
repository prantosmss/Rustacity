/**********************************************************************

  Rustacity: A Digital Audio Editor

  Menus.h

  Dominic Mazzoni

**********************************************************************/
#ifndef __RUSTACITY_MENUS__
#define __RUSTACITY_MENUS__

#include "rustacity/Types.h"

#include <wx/string.h> // member variable
#include "Prefs.h"
#include "ClientData.h"
#include "commands/CommandFlag.h"

class wxArrayString;
class wxCommandEvent;
class RustacityProject;
class CommandContext;
class CommandManager;
class LabelTrack;
class PluginDescriptor;
class Track;
class TrackList;
class ViewInfo;
class WaveClip;
class WaveTrack;

enum EffectType : int;

typedef wxString PluginID;
typedef wxArrayString PluginIDs;

namespace Registry{ class Visitor; }

class MenuCreator
{
public:
   MenuCreator();
   ~MenuCreator();
   void CreateMenusAndCommands(RustacityProject &project);
   void RebuildMenuBar(RustacityProject &project);

   static void RebuildAllMenuBars();

public:
   CommandFlag mLastFlags;
   
   // Last effect applied to this project
   PluginID mLastEffect{};
};

struct ToolbarMenuVisitor;

class MenuManager final
   : public MenuCreator
   , public ClientData::Base
   , private PrefsListener
{
public:

   static MenuManager &Get( RustacityProject &project );
   static const MenuManager &Get( const RustacityProject &project );

   explicit
   MenuManager( RustacityProject &project );
   MenuManager( const MenuManager & ) PROHIBITED;
   MenuManager &operator=( const MenuManager & ) PROHIBITED;
   ~MenuManager();

   static void Visit( ToolbarMenuVisitor &visitor );

   static void ModifyUndoMenuItems(RustacityProject &project);
   static void ModifyToolbarMenus(RustacityProject &project);
   // Calls ModifyToolbarMenus() on all projects
   static void ModifyAllProjectToolbarMenus();

   // checkActive is a temporary hack that should be removed as soon as we
   // get multiple effect preview working
   void UpdateMenus( bool checkActive = true );

   // If checkActive, do not do complete flags testing on an
   // inactive project as it is needlessly expensive.
   CommandFlag GetUpdateFlags( bool checkActive = false ) const;
   void UpdatePrefs() override;

   // Command Handling
   bool ReportIfActionNotAllowed(
      const TranslatableString & Name, CommandFlag & flags, CommandFlag flagsRqd );
   bool TryToMakeActionAllowed(
      CommandFlag & flags, CommandFlag flagsRqd );


private:
   void TellUserWhyDisallowed(const TranslatableString & Name, CommandFlag flagsGot,
      CommandFlag flagsRequired);

   void OnUndoRedo( wxCommandEvent &evt );

   RustacityProject &mProject;

public:
   // 0 is grey out, 1 is Autoselect, 2 is Give warnings.
   int  mWhatIfNoSelection;
   bool mStopIfWasPaused;
};

#endif
