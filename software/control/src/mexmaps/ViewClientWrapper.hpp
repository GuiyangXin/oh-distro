#ifndef _mexmaps_ViewClientWrapper_hpp_
#define _mexmaps_ViewClientWrapper_hpp_

#include <memory>
#include <string>
#include <vector>
#include <Eigen/Geometry>

namespace lcm {
  class LCM;
}

namespace maps {
  class BotWrapper;
  class ViewClient;
  class ViewBase;
  class DepthImageView;
}

namespace mexmaps {

class MapHandle;
class FillMethods;

struct ViewClientWrapper {
  struct Listener;

  int mId;
  std::shared_ptr<maps::ViewClient> mViewClient;
  std::shared_ptr<lcm::LCM> mLcm;
  std::shared_ptr<maps::BotWrapper> mBotWrapper;
  std::shared_ptr<Listener> mListener;
  std::shared_ptr<FillMethods> mFillMethods;
  int mHeightMapViewId;
  std::shared_ptr<MapHandle> mHandle;
  int64_t mLastReceiptTime;
  std::shared_ptr<maps::DepthImageView> mCurrentView;

  int mNormalRadius;
  bool mShouldFill;

  ViewClientWrapper(const int iId, const std::shared_ptr<lcm::LCM>& iLcm);
  ~ViewClientWrapper();

  void setHeightMapChannel(const std::string& iChannel, const int iViewId);
  void setMapMode(const int iMode);

  bool start();
  bool stop();

  std::shared_ptr<maps::ViewBase> getView();

protected:
  void requestHeightMap();
};

}

#endif
