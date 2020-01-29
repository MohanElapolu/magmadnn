/**
 * @file pooling.h
 * @author Sedrick Keh
 * @version 1.0
 * @date 2019-07-08
 *
 * @copyright Copyright (c) 2019
 */
#pragma once

#include "magmadnn/config.h"
#include "tensor/tensor.h"

enum pooling_mode { MAX_POOL, AVERAGE_POOL };

#if defined(MAGMADNN_HAVE_CUDA)
#include "cudnn.h"
#endif

namespace magmadnn {
namespace math {

template <typename T>
void pooling(Tensor<T> *x, Tensor<T> *out, Tensor<int> *max_positions, int filter_h, int filter_w, int pad_h, int pad_w,
             int vertical_stride, int horizontal_stride, int dilation_h, int dilation_w, pooling_mode mode);

template <typename T>
void pooling_grad(Tensor<T> *x, Tensor<T> *y, Tensor<T> *grad, Tensor<int> *max_positions, Tensor<T> *out, int filter_h,
                  int filter_w, int pad_h, int pad_w, int vertical_stride, int horizontal_stride, int dilation_h,
                  int dilation_w, pooling_mode mode);

#if defined(MAGMADNN_HAVE_CUDA)

struct cudnn_pooling_settings_t {
    cudnnPoolingDescriptor_t poolingDesc;
    cudnnHandle_t handle;
};

template <typename T>
void pooling_device(Tensor<T> *x, Tensor<T> *out, cudnn_pooling_settings_t settings);

template <typename T>
void pooling_grad_device(Tensor<T> *x, Tensor<T> *y, Tensor<T> *grad, Tensor<T> *out,
                         cudnn_pooling_settings_t settings);

#endif

}  // namespace math
}  // namespace magmadnn
