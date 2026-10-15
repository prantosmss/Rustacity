/**********************************************************************

  Rustacity: A Digital Audio Editor

  CommandContext.h

  Created by Paul Licameli on 4/22/16.

**********************************************************************/

#ifndef __RUSTACITY_COMMAND_CONTEXT__
#define __RUSTACITY_COMMAND_CONTEXT__

#include <memory>
#include "rustacity/Types.h"

class RustacityProject;
class RustacityApp;
class wxEvent;
class CommandOutputTargets;
using CommandParameter = CommandID;

class RUSTACITY_DLL_API CommandContext {
public:
   CommandContext(
      RustacityProject &p
      , const wxEvent *e = nullptr
      , int ii = 0
      , const CommandParameter &param = CommandParameter{}
   );

   CommandContext(
      RustacityProject &p,
      std::unique_ptr<CommandOutputTargets> target);

   ~CommandContext();

   virtual void Status( const wxString &message, bool bFlush = false ) const;
   virtual void Error(  const wxString &message ) const;
   virtual void Progress( double d ) const;

   // Output formatting...
   void StartArray() const;
   void EndArray() const;
   void StartStruct() const;
   void EndStruct() const;
   void StartField(const wxString &name) const;
   void EndField() const;
   void AddItem(const wxString &value , const wxString &name = {} ) const;
   void AddBool(const bool value      , const wxString &name = {} ) const;
   void AddItem(const double value    , const wxString &name = {} ) const;

   RustacityProject &project;
   std::unique_ptr<CommandOutputTargets> pOutput;
   const wxEvent *pEvt;
   int index;
   CommandParameter parameter;
   RustacityApp *GetApp() const;
};
#endif
