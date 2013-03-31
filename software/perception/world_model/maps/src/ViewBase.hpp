#ifndef _maps_ViewBase_hpp_
#define _maps_ViewBase_hpp_

#include "Types.hpp"

namespace maps {

class ViewBase {
public:
  enum Type {
    TypePointCloud,
    TypeOctree,
    TypeDepthImage,
    TypeVoxelGrid,
    TypeMesh,
    TypeSurfels,
  };

  struct Spec {
    int64_t mMapId;
    int64_t mViewId;
    bool mActive;
    bool mRelativeTime;
    bool mRelativeLocation;
    Type mType;
    float mResolution;
    float mFrequency;
    int64_t mTimeMin;
    int64_t mTimeMax;
    int mWidth;
    int mHeight;
    std::vector<Eigen::Vector4f> mClipPlanes;
    Eigen::Projective3f mTransform;  // reference to view

    Spec();
    bool operator==(const Spec& iSpec) const;
    bool operator!=(const Spec& iSpec) const;
  };

  typedef boost::shared_ptr<ViewBase> Ptr;

public:
  ViewBase();
  virtual ~ViewBase();

  virtual const Type getType() const = 0;

  void setId(const int64_t iId);
  const int64_t getId() const;

  void setTransform(const Eigen::Projective3f& iTransform);
  const Eigen::Projective3f getTransform() const;

  void setUpdateTime(const int64_t iTime);
  const int64_t getUpdateTime() const;

  virtual Ptr clone() const = 0;

  virtual void set(const maps::PointCloud::Ptr& iCloud) = 0;

  virtual maps::PointCloud::Ptr
  getAsPointCloud(const bool iTransform=true) const = 0;

  virtual maps::TriangleMesh::Ptr
  getAsMesh(const bool iTransform=true) const;

  virtual bool getClosest(const Eigen::Vector3f& iPoint,
                          Eigen::Vector3f& oPoint,
                          Eigen::Vector3f& oNormal) const;

  virtual bool intersectRay(const Eigen::Vector3f& iOrigin,
                            const Eigen::Vector3f& iDirection,
                            Eigen::Vector3f& oPoint,
                            Eigen::Vector3f& oNormal) const;

protected:
  int64_t mId;
  int64_t mUpdateTime;
  Eigen::Projective3f mTransform;  // from reference frame to this view
};

}

#endif
