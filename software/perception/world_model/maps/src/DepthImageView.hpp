#ifndef _maps_DepthImageView_hpp_
#define _maps_DepthImageView_hpp_

#include "ViewBase.hpp"

namespace maps {

class DepthImage;

class DepthImageView : public ViewBase {
public:
  typedef boost::shared_ptr<DepthImageView> Ptr;

public:
  DepthImageView();
  ~DepthImageView();

  void setSize(const int iWidth, const int iHeight);
  void set(const DepthImage& iImage);
  boost::shared_ptr<DepthImage> getDepthImage() const;

  const Type getType() const;
  ViewBase::Ptr clone() const;
  void set(const maps::PointCloud::Ptr& iCloud);
  maps::PointCloud::Ptr getAsPointCloud(const bool iTransform=true) const;
  maps::TriangleMesh::Ptr getAsMesh(const bool iTransform=true) const;
  bool getClosest(const Eigen::Vector3f& iPoint,
                  Eigen::Vector3f& oPoint, Eigen::Vector3f& oNormal) const;
  bool intersectRay(const Eigen::Vector3f& iOrigin,
                    const Eigen::Vector3f& iDirection,
                    Eigen::Vector3f& oPoint, Eigen::Vector3f& oNormal) const;

protected:
  bool interpolate(const float iX, const float iY, float& oDepth) const;
  bool unproject(const Eigen::Vector3f& iPoint, Eigen::Vector3f& oPoint,
                 Eigen::Vector3f& oNormal) const;

protected:
  boost::shared_ptr<DepthImage> mImage;
};

}

#endif
