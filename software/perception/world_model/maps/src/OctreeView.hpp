#ifndef _maps_OctreeView_hpp_
#define _maps_OctreeView_hpp_

#include "ViewBase.hpp"

namespace octomap {
  class OcTree;
}

namespace maps {

class OctreeView : public ViewBase {
public:
  typedef boost::shared_ptr<OctreeView> Ptr;
public:
  OctreeView();
  ~OctreeView();

  void setResolution(const float iResolution);
  boost::shared_ptr<octomap::OcTree> getOctree() const;

  const Type getType() const;
  ViewBase::Ptr clone() const;
  void set(const maps::PointCloud::Ptr& iCloud);
  maps::PointCloud::Ptr getAsPointCloud(const bool iTransform=true) const;

protected:
  boost::shared_ptr<octomap::OcTree> mOctree;
};

}

#endif
