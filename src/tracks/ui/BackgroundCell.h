/**********************************************************************

Rustacity: A Digital Audio Editor

BackgroundCell.h

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#ifndef __RUSTACITY_BACKGROUND_CELL__
#define __RUSTACITY_BACKGROUND_CELL__

#include "../../ClientData.h"
#include "CommonTrackPanelCell.h"

class RustacityProject;

class BackgroundHandle;
class ZoomHandle;


/// \brief Class representing the background of a Track.  It
/// provides the hit test function that tells us what was hit.
class BackgroundCell final
   : public CommonTrackPanelCell
   , public ClientData::Base
{
public:
   static BackgroundCell &Get( RustacityProject &project );
   static const BackgroundCell &Get( const RustacityProject &project );

   explicit
   BackgroundCell(RustacityProject *pProject)
      : mpProject(pProject)
   {}

   virtual ~BackgroundCell();

protected:
   std::vector<UIHandlePtr> HitTest
      (const TrackPanelMouseState &state,
       const RustacityProject *) override;

   std::shared_ptr<Track> DoFindTrack() override;

private:
   // TrackPanelDrawable implementation
   void Draw(
      TrackPanelDrawingContext &context,
      const wxRect &rect, unsigned iPass ) override;

   wxRect DrawingArea(
      TrackPanelDrawingContext &,
      const wxRect &rect, const wxRect &panelRect, unsigned iPass ) override;
   
   RustacityProject *mpProject;

   std::weak_ptr<BackgroundHandle> mHandle;

public:
   // For want of a better place...
   mutable std::weak_ptr<ZoomHandle> mZoomHandle;
};

#endif
