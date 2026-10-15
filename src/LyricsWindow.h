/**********************************************************************

  Rustacity: A Digital Audio Editor

  LyricsWindow.h

  Vaughan Johnson
  Dominic Mazzoni

**********************************************************************/

#ifndef __RUSTACITY_LYRICS_WINDOW__
#define __RUSTACITY_LYRICS_WINDOW__

#include <wx/frame.h> // to inherit

class RustacityProject;
class LyricsPanel;

class LyricsWindow final : public wxFrame {

 public:
   LyricsWindow(RustacityProject* parent);

   LyricsPanel *GetLyricsPanel() { return mLyricsPanel; };

 private:
   void OnCloseWindow(wxCloseEvent & WXUNUSED(event));

   void OnStyle_BouncingBall(wxCommandEvent &evt);
   void OnStyle_Highlight(wxCommandEvent &evt);
   void OnTimer(wxCommandEvent &event);

   RustacityProject *mProject;
   LyricsPanel *mLyricsPanel;

 public:
   DECLARE_EVENT_TABLE()
};

#endif
