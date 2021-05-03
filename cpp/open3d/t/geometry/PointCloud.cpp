// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include "open3d/t/geometry/PointCloud.h"

#include <Eigen/Core>
#include <limits>
#include <string>
#include <unordered_map>

#include "open3d/core/EigenConverter.h"
#include "open3d/core/ShapeUtil.h"
#include "open3d/core/Tensor.h"
#include "open3d/core/hashmap/Hashmap.h"
#include "open3d/core/linalg/Matmul.h"
#include "open3d/t/geometry/TensorMap.h"
#include "open3d/t/geometry/kernel/PointCloud.h"

namespace open3d {
namespace t {
namespace geometry {

PointCloud::PointCloud(const core::Device &device)
    : Geometry(Geometry::GeometryType::PointCloud, 3),
      device_(device),
      point_attr_(TensorMap("points")) {
    ;
}

PointCloud::PointCloud(const core::Tensor &points)
    : PointCloud(points.GetDevice()) {
    points.AssertShapeCompatible({utility::nullopt, 3});
    SetPoints(points);
}

PointCloud::PointCloud(const std::unordered_map<std::string, core::Tensor>
                               &map_keys_to_tensors)
    : Geometry(Geometry::GeometryType::PointCloud, 3),
      point_attr_(TensorMap("points")) {
    if (map_keys_to_tensors.count("points") == 0) {
        utility::LogError("\"points\" attribute must be specified.");
    }
    device_ = map_keys_to_tensors.at("points").GetDevice();
    map_keys_to_tensors.at("points").AssertShapeCompatible(
            {utility::nullopt, 3});
    point_attr_ = TensorMap("points", map_keys_to_tensors.begin(),
                            map_keys_to_tensors.end());
}

core::Tensor PointCloud::GetMinBound() const { return GetPoints().Min({0}); }

core::Tensor PointCloud::GetMaxBound() const { return GetPoints().Max({0}); }

core::Tensor PointCloud::GetCenter() const { return GetPoints().Mean({0}); }

PointCloud PointCloud::To(const core::Device &device, bool copy) const {
    if (!copy && GetDevice() == device) {
        return *this;
    }
    PointCloud pcd(device);
    for (auto &kv : point_attr_) {
        pcd.SetPointAttr(kv.first, kv.second.To(device, /*copy=*/true));
    }
    return pcd;
}

PointCloud PointCloud::Clone() const { return To(GetDevice(), /*copy=*/true); }

PointCloud &PointCloud::Transform(const core::Tensor &transformation) {
    transformation.AssertShape({4, 4});
    transformation.AssertDevice(device_);

    if (transformation.AllClose(core::Tensor::Eye(
                4, transformation.GetDtype(), transformation.GetDevice()))) {
        return *this;
    }

    core::Tensor R = transformation.Slice(0, 0, 3).Slice(1, 0, 3);
    core::Tensor t = transformation.Slice(0, 0, 3).Slice(1, 3, 4);
    // TODO: Make it more generalised [4x4][4xN] transformation.

    // TODO: Consider adding a new op extending MatMul to support `AB + C`
    // GEMM operation. Also, a parallel joint optimimsed kernel for
    // independent MatMul operation with common matrix like AB and AC
    // with fusion based cache optimisation.
    core::Tensor &points = GetPoints();
    points = (R.Matmul(points.T())).Add_(t).T();

    if (HasPointNormals()) {
        core::Tensor &normals = GetPointNormals();
        normals = (R.Matmul(normals.T())).T();
    }
    return *this;
}

PointCloud &PointCloud::Translate(const core::Tensor &translation,
                                  bool relative) {
    translation.AssertShape({3});
    translation.AssertDevice(device_);

    core::Tensor transform = translation;
    if (!relative) {
        transform -= GetCenter();
    }
    GetPoints() += transform;
    return *this;
}

PointCloud &PointCloud::Scale(double scale, const core::Tensor &center) {
    center.AssertShape({3});
    center.AssertDevice(device_);

    core::Tensor points = GetPoints();
    points.Sub_(center).Mul_(scale).Add_(center);
    return *this;
}

PointCloud &PointCloud::Rotate(const core::Tensor &R,
                               const core::Tensor &center) {
    R.AssertShape({3, 3});
    R.AssertDevice(device_);
    center.AssertShape({3});
    center.AssertDevice(device_);

    core::Tensor Rot = R;
    core::Tensor &points = GetPoints();
    points = ((Rot.Matmul((points.Sub_(center)).T())).T()).Add_(center);

    if (HasPointNormals()) {
        core::Tensor &normals = GetPointNormals();
        normals = (Rot.Matmul(normals.T())).T();
    }
    return *this;
}

PointCloud PointCloud::VoxelDownSample(
        double voxel_size, const core::HashmapBackend &backend) const {
    if (voxel_size <= 0) {
        utility::LogError("voxel_size must be positive.");
    }
    core::Tensor points_voxeld = GetPoints() / voxel_size;
    core::Tensor points_voxeli = points_voxeld.Floor().To(core::Dtype::Int64);

    core::Hashmap points_voxeli_hashmap(points_voxeli.GetLength(),
                                        core::Dtype::Int64, core::Dtype::Int32,
                                        {3}, {1}, device_, backend);

    core::Tensor addrs, masks;
    points_voxeli_hashmap.Activate(points_voxeli, addrs, masks);

    PointCloud pcd_down(GetPoints().GetDevice());
    for (auto &kv : point_attr_) {
        if (kv.first == "points") {
            pcd_down.SetPointAttr(kv.first, points_voxeli.IndexGet({masks}).To(
                                                    GetPoints().GetDtype()) *
                                                    voxel_size);
        } else {
            pcd_down.SetPointAttr(kv.first, kv.second.IndexGet({masks}));
        }
    }

    return pcd_down;
}

PointCloud PointCloud::CreateFromDepthImage(const Image &depth,
                                            const core::Tensor &intrinsics,
                                            const core::Tensor &extrinsics,
                                            float depth_scale,
                                            float depth_max,
                                            int stride) {
    core::Dtype dtype = depth.AsTensor().GetDtype();
    if (dtype != core::Dtype::UInt16 && dtype != core::Dtype::Float32) {
        utility::LogError(
                "Unsupported dtype for CreateFromDepthImage, expected UInt16 "
                "or Float32, but got {}.",
                dtype.ToString());
    }

    core::Tensor points;
    kernel::pointcloud::Unproject(depth.AsTensor(), utility::nullopt, points,
                                  utility::nullopt, intrinsics, extrinsics,
                                  depth_scale, depth_max, stride);
    return PointCloud(points);
}

PointCloud PointCloud::CreateFromRGBDImage(const RGBDImage &rgbd_image,
                                           const core::Tensor &intrinsics,
                                           const core::Tensor &extrinsics,
                                           float depth_scale,
                                           float depth_max,
                                           int stride) {
    auto dtype = rgbd_image.depth_.AsTensor().GetDtype();
    if (dtype != core::Dtype::UInt16 && dtype != core::Dtype::Float32) {
        utility::LogError(
                "Unsupported dtype for CreateFromRGBDImage, expected UInt16 "
                "or Float32, but got {}.",
                dtype.ToString());
    }

    core::Tensor image_colors =
            rgbd_image.color_.To(core::Dtype::Float32, /*copy=*/false)
                    .AsTensor();

    core::Tensor points, colors;
    kernel::pointcloud::Unproject(rgbd_image.depth_.AsTensor(), image_colors,
                                  points, colors, intrinsics, extrinsics,
                                  depth_scale, depth_max, stride);

    return PointCloud({{"points", points}, {"colors", colors}});
}

geometry::Image PointCloud::ProjectDepth(int width,
                                         int height,
                                         const core::Tensor &intrinsics,
                                         const core::Tensor &extrinsics,
                                         float depth_scale,
                                         float depth_max) {
    core::Tensor depth = core::Tensor::Zeros({height, width, 1},
                                             core::Dtype::Float32, device_);
    core::Tensor color_placeholder({1, 1, 3}, core::Dtype::UInt8);
    core::Tensor point_colors_placeholder({1, 3}, core::Dtype::Float32);
    kernel::pointcloud::Project(depth, color_placeholder, GetPoints(),
                                point_colors_placeholder, intrinsics,
                                extrinsics, depth_scale, depth_max);
    return geometry::Image(depth);
}

std::pair<geometry::Image, geometry::Image> PointCloud::ProjectRGBD(
        int width,
        int height,
        const core::Tensor &intrinsics,
        const core::Tensor &extrinsics,
        float depth_scale,
        float depth_max) {
    if (!HasPointColors()) {
        utility::LogError(
                "Unable to project to RGBD without the Color attribute in the "
                "point cloud.");
    }

    core::Tensor depth = core::Tensor::Zeros({height, width, 1},
                                             core::Dtype::Float32, device_);
    core::Tensor color = core::Tensor::Zeros({height, width, 3},
                                             core::Dtype::UInt8, device_);
    kernel::pointcloud::Project(depth, color, GetPoints(), GetPointColors(),
                                intrinsics, extrinsics, depth_scale, depth_max);

    return std::make_pair(geometry::Image(depth), geometry::Image(color));
}

PointCloud PointCloud::FromLegacyPointCloud(
        const open3d::geometry::PointCloud &pcd_legacy,
        core::Dtype dtype,
        const core::Device &device) {
    geometry::PointCloud pcd(device);
    if (pcd_legacy.HasPoints()) {
        pcd.SetPoints(core::eigen_converter::EigenVector3dVectorToTensor(
                pcd_legacy.points_, dtype, device));
    } else {
        utility::LogWarning("Creating from an empty legacy PointCloud.");
    }
    if (pcd_legacy.HasColors()) {
        pcd.SetPointColors(core::eigen_converter::EigenVector3dVectorToTensor(
                pcd_legacy.colors_, dtype, device));
    }
    if (pcd_legacy.HasNormals()) {
        pcd.SetPointNormals(core::eigen_converter::EigenVector3dVectorToTensor(
                pcd_legacy.normals_, dtype, device));
    }
    return pcd;
}

open3d::geometry::PointCloud PointCloud::ToLegacyPointCloud() const {
    open3d::geometry::PointCloud pcd_legacy;
    if (HasPoints()) {
        pcd_legacy.points_ =
                core::eigen_converter::TensorToEigenVector3dVector(GetPoints());
    }
    if (HasPointColors()) {
        bool dtype_is_supported_for_conversion = true;
        double normalization_factor = 1.0;
        core::Dtype point_color_dtype = GetPointColors().GetDtype();

        if (point_color_dtype == core::Dtype::UInt8) {
            normalization_factor =
                    1.0 /
                    static_cast<double>(std::numeric_limits<uint8_t>::max());
        } else if (point_color_dtype == core::Dtype::UInt16) {
            normalization_factor =
                    1.0 /
                    static_cast<double>(std::numeric_limits<uint16_t>::max());
        } else if (point_color_dtype != core::Dtype::Float32 &&
                   point_color_dtype != core::Dtype::Float64) {
            utility::LogWarning(
                    "Dtype {} of color attribute is not supported for "
                    "conversion to LegacyPointCloud and will be skipped. "
                    "Supported dtypes include UInt8, UIn16, Float32, and "
                    "Float64",
                    point_color_dtype.ToString());
            dtype_is_supported_for_conversion = false;
        }

        if (dtype_is_supported_for_conversion) {
            if (normalization_factor != 1.0) {
                core::Tensor rescaled_colors =
                        GetPointColors().To(core::Dtype::Float64) *
                        normalization_factor;
                pcd_legacy.colors_ =
                        core::eigen_converter::TensorToEigenVector3dVector(
                                rescaled_colors);
            } else {
                pcd_legacy.colors_ =
                        core::eigen_converter::TensorToEigenVector3dVector(
                                GetPointColors());
            }
        }
    }
    if (HasPointNormals()) {
        pcd_legacy.normals_ =
                core::eigen_converter::TensorToEigenVector3dVector(
                        GetPointNormals());
    }
    return pcd_legacy;
}

}  // namespace geometry
}  // namespace t
}  // namespace open3d
