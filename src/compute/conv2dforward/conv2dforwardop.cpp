#include "compute/conv2dforward/conv2dforwardop.h"

#include <iostream>

#include "magmadnn/config.h"

namespace magmadnn {
namespace op {

template <typename T>
Conv2DForwardOp<T>::Conv2DForwardOp(Operation<T> *input, Operation<T> *filter, int pad_h, int pad_w,
                                    int vertical_stride, int horizontal_stride, int dilation_h, int dilation_w,
                                    bool use_cross_correlation, bool needs_grad)
    : Operation<T>::Operation({input, filter}, needs_grad),
      input(input),
      filter(filter),
      pad_h(pad_h),
      pad_w(pad_w),
      vertical_stride(vertical_stride),
      horizontal_stride(horizontal_stride),
      dilation_h(dilation_h),
      dilation_w(dilation_w),
      use_cross_correlation(use_cross_correlation) {
    /* setup code in here */
    this->mem_type = input->get_memory_type();

    /* initialize all the conv settings */
    this->input_tensor = this->input->get_output_tensor();
    this->init_settings();
}

template <typename T>
Conv2DForwardOp<T>::~Conv2DForwardOp() {
    if (this->mem_type == HOST) {
    }
#if defined(MAGMADNN_HAVE_CUDA)
    else {

        cudaErrchk(cudaFree(this->cudnn_settings.workspace));
        cudaErrchk(cudaFree(this->cudnn_settings.grad_data_workspace));
        cudaErrchk(cudaFree(this->cudnn_settings.grad_filter_workspace));

        cudnnErrchk(cudnnDestroyFilterDescriptor(this->cudnn_settings.filter_desc));
        cudnnErrchk(cudnnDestroyConvolutionDescriptor(this->cudnn_settings.conv_desc));
    }
#endif
}

template <typename T>
Tensor<T> *Conv2DForwardOp<T>::_eval(bool recompute) {
    input_tensor = input->eval(recompute);
    filter_tensor = filter->eval(recompute);

    if (this->mem_type == HOST) {
        ::magmadnn::math::conv2d(this->input_tensor, this->filter_tensor, this->output_tensor, this->pad_h, this->pad_w,
                                 this->vertical_stride, this->horizontal_stride, this->dilation_h, this->dilation_w);
    }
#if defined(MAGMADNN_HAVE_CUDA)
    else {
        this->cudnn_settings.handle = this->get_cudnn_handle();
        ::magmadnn::math::conv2d_device(this->input_tensor, this->filter_tensor, this->output_tensor,
                                        this->cudnn_settings);

        if (!this->get_async()) cudaStreamSynchronize(this->get_custream());
    }
#endif

    return this->output_tensor;
}

template <typename T>
Tensor<T> *Conv2DForwardOp<T>::_grad(Operation<T> *consumer, Operation<T> *var, Tensor<T> *grad) {
    /* return gradient in here ... */
    Tensor<T> *out = this->_grad_cache[(uintptr_t) var];

    if (var == this->input) {
        if (out == NULL) {
            out = new Tensor<T>(this->input->get_output_shape(), {NONE, {}}, this->mem_type);
#if defined(MAGMADNN_HAVE_CUDA)
            out->set_custream(this->get_custream());
            out->set_cublas_handle(this->get_cublas_handle());
#endif
            this->_grad_cache[(uintptr_t) var] = out;
        }

        this->filter_tensor = this->filter->eval(false);

        if (this->mem_type == HOST) {
            ::magmadnn::math::conv2d_grad_data(this->filter_tensor, grad, out);
        }
#if defined(MAGMADNN_HAVE_CUDA)
        else {
            this->cudnn_settings.handle = this->get_cudnn_handle();
            ::magmadnn::math::conv2d_grad_data_device(this->filter_tensor, grad, out, this->cudnn_settings);
            if (!this->get_async()) cudaStreamSynchronize(this->get_custream());
        }
#endif

    } else if (var == this->filter) {
        if (out == NULL) {
            out = new Tensor<T>(this->filter->get_output_shape(), {NONE, {}}, this->mem_type);
#if defined(MAGMADNN_HAVE_CUDA)
            out->set_custream(this->get_custream());
            out->set_cublas_handle(this->get_cublas_handle());
#endif
            this->_grad_cache[(uintptr_t) var] = out;
        }

        this->input_tensor = this->input->eval(false);

        if (this->mem_type == HOST) {
            ::magmadnn::math::conv2d_grad_filter(this->input_tensor, grad, out);
        }
#if defined(MAGMADNN_HAVE_CUDA)
        else {
            this->cudnn_settings.handle = this->get_cudnn_handle();
            ::magmadnn::math::conv2d_grad_filter_device(this->input_tensor, grad, out, this->cudnn_settings);
            if (!this->get_async()) cudaStreamSynchronize(this->get_custream());
        }
#endif

    } else {
        std::fprintf(stderr, "Error: bad conv2d grad\n");
    }

    return out;
}

template <typename T>
void Conv2DForwardOp<T>::init_settings() {
    if (this->mem_type == HOST) {
        this->calculate_and_set_output_shape();
    }
#if defined(MAGMADNN_HAVE_CUDA)
    else {

        this->cudnn_settings.handle = this->get_cudnn_handle();

        /* init the conv descriptor */
        cudnnErrchk(cudnnCreateConvolutionDescriptor(&this->cudnn_settings.conv_desc));

        /* set the convolution description */
        cudnnErrchk(cudnnSetConvolution2dDescriptor(
            this->cudnn_settings.conv_desc, pad_h, pad_w, vertical_stride, horizontal_stride, dilation_h, dilation_w,
            (use_cross_correlation) ? CUDNN_CROSS_CORRELATION : CUDNN_CONVOLUTION,
            ::magmadnn::internal::get_cudnn_data_type(static_cast<T>(0))));

        /* init and create the filter descriptor */
        int filter_dims[4];
        const std::vector<unsigned int> &filter_shape = this->filter->get_output_shape();
        for (unsigned int i = 0; i < 4; i++) {
            if (i >= filter_shape.size())
                filter_dims[i] = 1;
            else
                filter_dims[i] = filter_shape[i];
        }
        cudnnErrchk(cudnnCreateFilterDescriptor(&this->cudnn_settings.filter_desc));
        cudnnErrchk(cudnnSetFilter4dDescriptor(
            this->cudnn_settings.filter_desc, ::magmadnn::internal::get_cudnn_data_type(static_cast<T>(0)),
            CUDNN_TENSOR_NCHW, filter_dims[0], filter_dims[1], filter_dims[2], filter_dims[3]));

        this->calculate_and_set_output_shape();

        /* use CUDNN to get the correct/optimal convolution algorithm */
        cudnnErrchk(cudnnGetConvolutionForwardAlgorithm(
            this->cudnn_settings.handle, this->input->get_output_tensor()->get_cudnn_tensor_descriptor(),
            this->cudnn_settings.filter_desc, this->cudnn_settings.conv_desc,
            this->output_tensor->get_cudnn_tensor_descriptor(), CUDNN_CONVOLUTION_FWD_PREFER_FASTEST, 0,
            &this->cudnn_settings.algo));

        /* use CuDNN to get the necessary workspace size and allocate that memory */
        cudnnErrchk(cudnnGetConvolutionForwardWorkspaceSize(
            this->cudnn_settings.handle, this->input->get_output_tensor()->get_cudnn_tensor_descriptor(),
            this->cudnn_settings.filter_desc, this->cudnn_settings.conv_desc,
            this->output_tensor->get_cudnn_tensor_descriptor(), this->cudnn_settings.algo,
            &this->cudnn_settings.workspace_size));
        // std::cout << "Conv2DForwardOp<T>::init_settings, forward workspace size (MB) = "
        //           << (float) ((float) this->cudnn_settings.workspace_size / ((float) 1024.0 * 1024.0))  << std::endl;
        cudaErrchk(cudaMalloc((void **) &this->cudnn_settings.workspace, this->cudnn_settings.workspace_size));

        /* INIT the grad settings */
        cudnnErrchk(cudnnGetConvolutionBackwardDataAlgorithm(
            this->cudnn_settings.handle, this->cudnn_settings.filter_desc,
            this->output_tensor->get_cudnn_tensor_descriptor(),                                /* use output for dy */
            this->cudnn_settings.conv_desc, this->input_tensor->get_cudnn_tensor_descriptor(), /* use input for dx */
            CUDNN_CONVOLUTION_BWD_DATA_PREFER_FASTEST, 0, &this->cudnn_settings.bwd_data_algo));

        cudnnErrchk(cudnnGetConvolutionBackwardFilterAlgorithm(
            this->cudnn_settings.handle, this->input_tensor->get_cudnn_tensor_descriptor(),
            this->output_tensor->get_cudnn_tensor_descriptor(), this->cudnn_settings.conv_desc,
            this->cudnn_settings.filter_desc, CUDNN_CONVOLUTION_BWD_FILTER_PREFER_FASTEST, 0,
            &this->cudnn_settings.bwd_filter_algo));

        /* get the workspaces for each of the backward algorithms */
        cudnnErrchk(cudnnGetConvolutionBackwardDataWorkspaceSize(
            this->cudnn_settings.handle, this->cudnn_settings.filter_desc,
            this->output_tensor->get_cudnn_tensor_descriptor(), this->cudnn_settings.conv_desc,
            this->input_tensor->get_cudnn_tensor_descriptor(), this->cudnn_settings.bwd_data_algo,
            &this->cudnn_settings.grad_data_workspace_size));
        // std::cout << "Conv2DForwardOp<T>::init_settings, backward workspace size (MB) = "
        //           << (float) ((float) this->cudnn_settings.grad_data_workspace_size / ((float) 1024.0 * 1024.0))
        //           << std::endl;
        cudaErrchk(cudaMalloc((void **) &this->cudnn_settings.grad_data_workspace,
                              this->cudnn_settings.grad_data_workspace_size));

        cudnnErrchk(cudnnGetConvolutionBackwardFilterWorkspaceSize(
            this->cudnn_settings.handle, this->input_tensor->get_cudnn_tensor_descriptor(),
            this->output_tensor->get_cudnn_tensor_descriptor(), this->cudnn_settings.conv_desc,
            this->cudnn_settings.filter_desc, this->cudnn_settings.bwd_filter_algo,
            &this->cudnn_settings.grad_filter_workspace_size));
        // std::cout << "Conv2DForwardOp<T>::init_settings, filter workspace size (MB) = "
        //           << (float) ((float) this->cudnn_settings.grad_filter_workspace_size / ((float) 1024.0 * 1024.0))
        //           << std::endl;
        cudaErrchk(cudaMalloc((void **) &this->cudnn_settings.grad_filter_workspace,
                              this->cudnn_settings.grad_filter_workspace_size));

        // std::cout << "Conv2DForwardOp<T>::init_settings, cuDNN workspace size (MB) = "
        //           << (float) ((float) (this->cudnn_settings.grad_filter_workspace_size
        //                                + this->cudnn_settings.grad_data_workspace_size
        //                                + this->cudnn_settings.workspace_size) / ((float) 1024.0 * 1024.0))
        //           << std::endl;
    }
#endif
}

template <typename T>
void Conv2DForwardOp<T>::calculate_and_set_output_shape() {
    /* calculate the correct output shape here */
    if (this->mem_type == HOST) {
        unsigned int No, Co, Ho, Wo, Cf, Nf, Hf, Wf, Ni, Ci, Hi, Wi;  // shorthand for tensor dims
        const int p_h_x2 = this->pad_h * 2;                           // total padding to add on top and bottom
        const int p_w_x2 = this->pad_w * 2;                           // total padding to add on left and right
        std::vector<unsigned int> in_shape = this->input_tensor->get_shape();
        std::vector<unsigned int> filter_shape = this->filter->get_output_shape();

        if (filter_shape.size() == 4 && in_shape.size() == 4) {
            Ni = in_shape[0];
            Ci = in_shape[1];
            Hi = in_shape[2];
            Wi = in_shape[3];

            Nf = filter_shape[0];
            Cf = filter_shape[1];
            Hf = filter_shape[2];
            Wf = filter_shape[3];
        } else if (filter_shape.size() == 4 && in_shape.size() == 3) {
            Ni = 1;
            Ci = in_shape[0];
            Hi = in_shape[1];
            Wi = in_shape[2];

            Nf = filter_shape[0];
            Cf = filter_shape[1];
            Hf = filter_shape[2];
            Wf = filter_shape[3];
        } else if (filter_shape.size() == 3 && in_shape.size() == 3) {
            Ni = 1;
            Ci = in_shape[0];
            Hi = in_shape[1];
            Wi = in_shape[2];

            Nf = 1;
            Cf = filter_shape[0];
            Hf = filter_shape[1];
            Wf = filter_shape[2];
        } else {
            fprintf(stderr, "Error invalid shapes\n");
        }

        if (Ci != Cf) {
            fprintf(stderr, "Error: Conv2d_cpu filter channels(%i) must equal input channels(%i).\n", Cf, Ci);
        }

        No = Ni;
        Co = Nf;
        Ho = (Hi - (Hf * this->dilation_h) + p_h_x2 + this->horizontal_stride) / this->horizontal_stride;
        Wo = (Wi - (Wf * this->dilation_w) + p_w_x2 + this->vertical_stride) / this->vertical_stride;

        this->output_tensor = new Tensor<T>({No, Co, Ho, Wo}, {NONE, {}}, this->mem_type);

        this->output_tensor->squeeze();

        this->output_shape = this->output_tensor->get_shape();

    }
#if defined(MAGMADNN_HAVE_CUDA)
    else {
        int n, c, h, w;

        cudnnErrchk(cudnnGetConvolution2dForwardOutputDim(this->cudnn_settings.conv_desc,
                                                          this->input_tensor->get_cudnn_tensor_descriptor(),
                                                          this->cudnn_settings.filter_desc, &n, &c, &h, &w));

        this->output_shape = {static_cast<unsigned int>(n), static_cast<unsigned int>(c), static_cast<unsigned int>(h),
                              static_cast<unsigned int>(w)};
        this->output_tensor = new Tensor<T>(this->output_shape, {NONE, {}}, this->mem_type);
    }
#endif

#if defined(MAGMADNN_HAVE_CUDA)
    this->output_tensor->set_custream(this->get_custream());
    this->output_tensor->set_cublas_handle(this->get_cublas_handle());
#endif
}

template class Conv2DForwardOp<int>;
template class Conv2DForwardOp<float>;
template class Conv2DForwardOp<double>;

template <typename T>
Conv2DForwardOp<T> *conv2dforward(Operation<T> *input, Operation<T> *filter, int pad_h, int pad_w, int vertical_stride,
                                  int horizontal_stride, int dilation_h, int dilation_w, bool use_cross_correlation,
                                  bool needs_grad) {
    return new Conv2DForwardOp<T>(input, filter, pad_h, pad_w, vertical_stride, horizontal_stride, dilation_h,
                                  dilation_w, use_cross_correlation, needs_grad);
}
template Conv2DForwardOp<int> *conv2dforward(Operation<int> *input, Operation<int> *filter, int pad_h, int pad_w,
                                             int vertical_stride, int horizontal_stride, int dilation_h, int dilation_w,
                                             bool use_cross_correlation, bool needs_grad);
template Conv2DForwardOp<float> *conv2dforward(Operation<float> *input, Operation<float> *filter, int pad_h, int pad_w,
                                               int vertical_stride, int horizontal_stride, int dilation_h,
                                               int dilation_w, bool use_cross_correlation, bool needs_grad);
template Conv2DForwardOp<double> *conv2dforward(Operation<double> *input, Operation<double> *filter, int pad_h,
                                                int pad_w, int vertical_stride, int horizontal_stride, int dilation_h,
                                                int dilation_w, bool use_cross_correlation, bool needs_grad);

}  // namespace op
}  // namespace magmadnn
