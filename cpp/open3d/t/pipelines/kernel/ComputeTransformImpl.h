// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2021 www.open3d.org
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

// Private header. Do not include in Open3d.h.

#pragma once

#include "open3d/core/CUDAUtils.h"
#include "open3d/core/Tensor.h"
#include "open3d/t/pipelines/registration/RobustKernel.h"

namespace open3d {
namespace t {
namespace pipelines {
namespace kernel {

void ComputePosePointToPlaneCPU(const core::Tensor &source_points,
                                const core::Tensor &target_points,
                                const core::Tensor &target_normals,
                                const core::Tensor &correspondence_indices,
                                core::Tensor &pose,
                                float &residual,
                                int &inlier_count,
                                const core::Dtype &dtype,
                                const core::Device &device,
                                const registration::RobustKernel &kernel);

void ComputePoseColoredICPCPU(const core::Tensor &source_points,
                              const core::Tensor &source_colors,
                              const core::Tensor &target_points,
                              const core::Tensor &target_normals,
                              const core::Tensor &target_colors,
                              const core::Tensor &target_color_gradients,
                              const core::Tensor &correspondence_indices,
                              core::Tensor &pose,
                              float &residual,
                              int &inlier_count,
                              const core::Dtype &dtype,
                              const core::Device &device,
                              const registration::RobustKernel &kernel,
                              const float &lambda_geometric);
#ifdef BUILD_CUDA_MODULE
void ComputePosePointToPlaneCUDA(const core::Tensor &source_points,
                                 const core::Tensor &target_points,
                                 const core::Tensor &target_normals,
                                 const core::Tensor &correspondence_indices,
                                 core::Tensor &pose,
                                 float &residual,
                                 int &inlier_count,
                                 const core::Dtype &dtype,
                                 const core::Device &device,
                                 const registration::RobustKernel &kernel);

void ComputePoseColoredICPCUDA(const core::Tensor &source_points,
                               const core::Tensor &source_colors,
                               const core::Tensor &target_points,
                               const core::Tensor &target_normals,
                               const core::Tensor &target_colors,
                               const core::Tensor &target_color_gradients,
                               const core::Tensor &correspondence_indices,
                               core::Tensor &pose,
                               float &residual,
                               int &inlier_count,
                               const core::Dtype &dtype,
                               const core::Device &device,
                               const registration::RobustKernel &kernel,
                               const float &lambda_geometric);
#endif

void ComputeRtPointToPointCPU(const core::Tensor &source_points,
                              const core::Tensor &target_points,
                              const core::Tensor &correspondence_indices,
                              core::Tensor &R,
                              core::Tensor &t,
                              int &inlier_count,
                              const core::Dtype &dtype,
                              const core::Device &device);

template <typename scalar_t>
OPEN3D_HOST_DEVICE inline bool GetJacobianPointToPlane(
        const int64_t workload_idx,
        const scalar_t *source_points_ptr,
        const scalar_t *target_points_ptr,
        const scalar_t *target_normals_ptr,
        const int64_t *correspondence_indices,
        scalar_t *J_ij,
        scalar_t &r) {
    utility::LogError(" GetJacobianPointToPlane: Dtype not supported.");
}

template <>
OPEN3D_HOST_DEVICE inline bool GetJacobianPointToPlane<float>(
        const int64_t workload_idx,
        const float *source_points_ptr,
        const float *target_points_ptr,
        const float *target_normals_ptr,
        const int64_t *correspondence_indices,
        float *J_ij,
        float &r) {
    if (correspondence_indices[workload_idx] == -1) {
        return false;
    }

    const int64_t target_idx = 3 * correspondence_indices[workload_idx];
    const int64_t source_idx = 3 * workload_idx;

    const float &sx = source_points_ptr[source_idx + 0];
    const float &sy = source_points_ptr[source_idx + 1];
    const float &sz = source_points_ptr[source_idx + 2];
    const float &tx = target_points_ptr[target_idx + 0];
    const float &ty = target_points_ptr[target_idx + 1];
    const float &tz = target_points_ptr[target_idx + 2];
    const float &nx = target_normals_ptr[target_idx + 0];
    const float &ny = target_normals_ptr[target_idx + 1];
    const float &nz = target_normals_ptr[target_idx + 2];

    r = (sx - tx) * nx + (sy - ty) * ny + (sz - tz) * nz;

    J_ij[0] = nz * sy - ny * sz;
    J_ij[1] = nx * sz - nz * sx;
    J_ij[2] = ny * sx - nx * sy;
    J_ij[3] = nx;
    J_ij[4] = ny;
    J_ij[5] = nz;

    return true;
}

template <>
OPEN3D_HOST_DEVICE inline bool GetJacobianPointToPlane<double>(
        const int64_t workload_idx,
        const double *source_points_ptr,
        const double *target_points_ptr,
        const double *target_normals_ptr,
        const int64_t *correspondence_indices,
        double *J_ij,
        double &r) {
    if (correspondence_indices[workload_idx] == -1) {
        return false;
    }

    const int64_t target_idx = 3 * correspondence_indices[workload_idx];
    const int64_t source_idx = 3 * workload_idx;

    const double &sx = source_points_ptr[source_idx + 0];
    const double &sy = source_points_ptr[source_idx + 1];
    const double &sz = source_points_ptr[source_idx + 2];
    const double &tx = target_points_ptr[target_idx + 0];
    const double &ty = target_points_ptr[target_idx + 1];
    const double &tz = target_points_ptr[target_idx + 2];
    const double &nx = target_normals_ptr[target_idx + 0];
    const double &ny = target_normals_ptr[target_idx + 1];
    const double &nz = target_normals_ptr[target_idx + 2];

    r = (sx - tx) * nx + (sy - ty) * ny + (sz - tz) * nz;

    J_ij[0] = nz * sy - ny * sz;
    J_ij[1] = nx * sz - nz * sx;
    J_ij[2] = ny * sx - nx * sy;
    J_ij[3] = nx;
    J_ij[4] = ny;
    J_ij[5] = nz;

    return true;
}

template <typename scalar_t>
OPEN3D_HOST_DEVICE inline bool GetJacobianColoredICP(
        const int64_t workload_idx,
        const scalar_t *source_points_ptr,
        const scalar_t *source_colors_ptr,
        const scalar_t *target_points_ptr,
        const scalar_t *target_normals_ptr,
        const scalar_t *target_colors_ptr,
        const scalar_t *target_color_gradients_ptr,
        const int64_t *correspondence_indices,
        const scalar_t &sqrt_lambda_geometric,
        const scalar_t &sqrt_lambda_photometric,
        scalar_t *J_G,
        scalar_t *J_I,
        scalar_t &r_G,
        scalar_t &r_I) {
    utility::LogError(" GetJacobianColoredICP: Dtype not supported.");
}

template <>
OPEN3D_HOST_DEVICE inline bool GetJacobianColoredICP<float>(
        const int64_t workload_idx,
        const float *source_points_ptr,
        const float *source_colors_ptr,
        const float *target_points_ptr,
        const float *target_normals_ptr,
        const float *target_colors_ptr,
        const float *target_color_gradients_ptr,
        const int64_t *correspondence_indices,
        const float &sqrt_lambda_geometric,
        const float &sqrt_lambda_photometric,
        float *J_G,
        float *J_I,
        float &r_G,
        float &r_I) {

    if (correspondence_indices[workload_idx] == -1) {
        return false;
    }

    const int64_t target_idx = 3 * correspondence_indices[workload_idx];
    const int64_t source_idx = 3 * workload_idx;

    float vs[3] = {source_points_ptr[source_idx],
                   source_points_ptr[source_idx + 1],
                   source_points_ptr[source_idx + 2]};

    float vt[3] = {target_points_ptr[target_idx],
                   target_points_ptr[target_idx + 1],
                   target_points_ptr[target_idx + 2]};

    float nt[3] = {target_normals_ptr[target_idx],
                   target_normals_ptr[target_idx + 1],
                   target_normals_ptr[target_idx + 2]};

    float d = (vs[0] - vt[0]) * nt[0] + (vs[1] - vt[1]) * nt[1] +
              (vs[2] - vt[2]) * nt[2];
    
    J_G[0] = (-vs[2] * nt[1] + vs[1] * nt[2]);
    J_G[1] = (vs[2] * nt[0] - vs[0] * nt[2]);
    J_G[2] = (-vs[1] * nt[0] + vs[0] * nt[1]);
    J_G[3] = nt[0];
    J_G[4] = nt[1];
    J_G[5] = nt[2];
    r_G = d;

    float vs_proj[3] = {vs[0] - d * nt[0], vs[1] - d * nt[1],
                        vs[2] - d * nt[2]};

    float is =
            (source_colors_ptr[source_idx] + source_colors_ptr[source_idx + 1] +
             source_colors_ptr[source_idx + 2]) /
            3.0;

    float it =
            (target_colors_ptr[target_idx] + target_colors_ptr[target_idx + 1] +
             target_colors_ptr[target_idx + 2]) /
            3.0;

    float dit[3] = {target_color_gradients_ptr[target_idx],
                    target_color_gradients_ptr[target_idx + 1],
                    target_color_gradients_ptr[target_idx + 2]};

    float is_proj = dit[0] * (vs_proj[0] - vt[0]) +
                    dit[1] * (vs_proj[1] - vt[1]) +
                    dit[2] * (vs_proj[2] - vt[2]) + it;

    float s = dit[0] * nt[0] + dit[1] * nt[1] + dit[2] * nt[2];
    float ditM[3] = {s * nt[0] - dit[0], s * nt[1] - dit[1],
                     s * nt[2] - dit[2]};

    J_I[0] = sqrt_lambda_photometric * (-vs[2] * ditM[1] + vs[1] * ditM[2]);
    J_I[1] = sqrt_lambda_photometric * (vs[2] * ditM[0] - vs[0] * ditM[2]);
    J_I[2] = sqrt_lambda_photometric * (-vs[1] * ditM[0] + vs[0] * ditM[1]);
    J_I[3] = sqrt_lambda_photometric * ditM[0];
    J_I[4] = sqrt_lambda_photometric * ditM[1];
    J_I[5] = sqrt_lambda_photometric * ditM[2];
    r_I = sqrt_lambda_photometric * (is - is_proj);

    return true;
}

template <>
OPEN3D_HOST_DEVICE inline bool GetJacobianColoredICP<double>(
        const int64_t workload_idx,
        const double *source_points_ptr,
        const double *source_colors_ptr,
        const double *target_points_ptr,
        const double *target_normals_ptr,
        const double *target_colors_ptr,
        const double *target_color_gradients_ptr,
        const int64_t *correspondence_indices,
        const double &sqrt_lambda_geometric,
        const double &sqrt_lambda_photometric,
        double *J_G,
        double *J_I,
        double &r_G,
        double &r_I) {
    if (correspondence_indices[workload_idx] == -1) {
        return false;
    }

    const int64_t target_idx = 3 * correspondence_indices[workload_idx];
    const int64_t source_idx = 3 * workload_idx;

    double vs[3] = {source_points_ptr[source_idx],
                    source_points_ptr[source_idx + 1],
                    source_points_ptr[source_idx + 2]};

    double cs[3] = {source_colors_ptr[source_idx],
                    source_colors_ptr[source_idx + 1],
                    source_colors_ptr[source_idx + 2]};

    double is = (cs[0] + cs[1] + cs[2]) / 3.0;

    double vt[3] = {target_points_ptr[target_idx],
                    target_points_ptr[target_idx + 1],
                    target_points_ptr[target_idx + 2]};

    double ct[3] = {target_colors_ptr[target_idx],
                    target_colors_ptr[target_idx + 1],
                    target_colors_ptr[target_idx + 2]};

    double it = (ct[0] + ct[1] + ct[2]) / 3.0;

    double dit[3] = {target_color_gradients_ptr[target_idx],
                     target_color_gradients_ptr[target_idx + 1],
                     target_color_gradients_ptr[target_idx + 2]};

    double nt[3] = {target_normals_ptr[target_idx],
                    target_normals_ptr[target_idx + 1],
                    target_normals_ptr[target_idx + 2]};

    double d = (vs[0] - vt[0]) * nt[0] + (vs[1] - vt[1]) * nt[1] +
               (vs[2] - vt[2]) * nt[2];

    double vs_proj[3] = {vs[0] - d * nt[0], vs[1] - d * nt[1],
                         vs[2] - d * nt[2]};

    double is_proj = dit[0] * (vs_proj[0] - vt[0]) +
                     dit[1] * (vs_proj[1] - vt[1]) +
                     dit[2] * (vs_proj[2] - vt[2]) + it;

    J_G[0] = sqrt_lambda_geometric * (-vs[2] * nt[1] + vs[1] * nt[2]);
    J_G[1] = sqrt_lambda_geometric * (vs[2] * nt[0] - vs[0] * nt[2]);
    J_G[2] = sqrt_lambda_geometric * (-vs[1] * nt[0] + vs[0] * nt[1]);
    J_G[3] = sqrt_lambda_geometric * nt[0];
    J_G[4] = sqrt_lambda_geometric * nt[1];
    J_G[5] = sqrt_lambda_geometric * nt[2];
    r_G = sqrt_lambda_geometric * d;

    double s = dit[0] * nt[0] + dit[1] * nt[1] + dit[2] * nt[2];
    double ditM[3] = {s * nt[0] - dit[0], s * nt[1] - dit[1],
                      s * nt[2] - dit[2]};

    J_I[0] = sqrt_lambda_photometric * (-vs[2] * ditM[1] + vs[1] * ditM[2]);
    J_I[1] = sqrt_lambda_photometric * (vs[2] * ditM[0] - vs[0] * ditM[2]);
    J_I[2] = sqrt_lambda_photometric * (-vs[1] * ditM[0] + vs[0] * ditM[1]);
    J_I[3] = sqrt_lambda_photometric * ditM[0];
    J_I[4] = sqrt_lambda_photometric * ditM[1];
    J_I[5] = sqrt_lambda_photometric * ditM[2];
    r_I = sqrt_lambda_photometric * (is - is_proj);

    return true;
}

}  // namespace kernel
}  // namespace pipelines
}  // namespace t
}  // namespace open3d
