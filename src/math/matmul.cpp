/**
 * @file matmul.cpp
 * @author Daniel Nichols
 * @author Florent Lopez
 * @version 0.1
 * @date 2019-06-06
 *
 * @copyright Copyright (c) 2019
 */
#include "math/matmul.h"

#include "magmadnn/config.h"
#include "math/wrappers.h"

#if defined(MAGMADNN_HAVE_CUDA)
#include "cublas_v2.h"
#endif

namespace magmadnn {
namespace math {

template <>
void matmul(int alpha, bool trans_A, Tensor<int> *A, bool trans_B, Tensor<int> *B, int beta, Tensor<int> *C) {}

template <>
void matmul(float alpha, bool trans_A, Tensor<float> *A, bool trans_B, Tensor<float> *B, float beta, Tensor<float> *C) {
    // unsigned int M, N, K, ldda, lddb, lddc;
    int M, N, K, ldda, lddb, lddc;

    /* op(A) : MxK ; op(B) : KxN ; C : MxN */

    M = C->get_shape(0);                 /* rows of C and op(A) */
    N = C->get_shape(1);                 /* columns of C and op(B) */
    K = A->get_shape((trans_A) ? 0 : 1); /* columns of op(A) and rows of op(B) */
    ldda = (trans_A) ? M : K;            /* leading dimension of op(A) */
    lddb = (trans_B) ? K : N;            /* leading dimension of op(B) */
    lddc = N;

    // A: MxK  B: KxN  C: MxN
    // (MxR)(RxN) + (MxN) = (MxN) + (MxN) = (MxN)

    if (A->get_memory_type() == HOST) {
        // CBLAS_TRANSPOSE a_trans = (trans_A) ? CblasTrans : CblasNoTrans;
        // CBLAS_TRANSPOSE b_trans = (trans_B) ? CblasTrans : CblasNoTrans;

        // specify ROW MAJOR, since tensors are stored in row-major
        // cblas_sgemm(CblasRowMajor, a_trans, b_trans, M, N, K, alpha, A->get_ptr(), ldda, B->get_ptr(), lddb, beta,
        //             C->get_ptr(), lddc);

       // Assuming row-major storage 
       operation a_trans = (trans_A) ? OP_T : OP_N;
       operation b_trans = (trans_B) ? OP_T : OP_N;
       
       gemm(b_trans, a_trans, N, M, K, alpha, B->get_ptr(), lddb, A->get_ptr(), ldda, beta, C->get_ptr(), lddc);

    }
#if defined(MAGMADNN_HAVE_CUDA)
    else {
        // since magma is column-major we'll need the transpose of everything
        // i.e. (AB)^T = (C)^T and the fact that (AB)^T = (B^T)(A^T)

        if (C->get_cublas_handle()) {
           cublasOperation_t a_trans = (trans_A) ? CUBLAS_OP_T : CUBLAS_OP_N;
           cublasOperation_t b_trans = (trans_B) ? CUBLAS_OP_T : CUBLAS_OP_N;

           cublasSgemm(C->get_cublas_handle(), b_trans, a_trans, N, M, K, &alpha, B->get_ptr(), lddb, A->get_ptr(), ldda, &beta, C->get_ptr(), lddc);
        }
        else {
           magma_trans_t a_trans = (trans_A) ? MagmaTrans : MagmaNoTrans;
           magma_trans_t b_trans = (trans_B) ? MagmaTrans : MagmaNoTrans;

           MAGMA_SGEMM_ROWMAJOR(A->get_ptr(), B->get_ptr(), C->get_ptr(), M, N, K, alpha, beta, a_trans, b_trans, ldda,
                                lddb, lddc);
        }
        // TODO: CUDA error management 
    }
#endif
}

template <>
void matmul(double alpha, bool trans_A, Tensor<double> *A, bool trans_B, Tensor<double> *B, double beta,
            Tensor<double> *C) {
    // unsigned int M, N, K, ldda, lddb, lddc;
   int M, N, K, ldda, lddb, lddc;

    /* op(A) : MxK ; op(B) : KxN ; C : MxN */
    /* lddX is the leading dimension of X or the number of elements between rows.  */

    M = C->get_shape(0);                 /* rows of C and op(A) */
    N = C->get_shape(1);                 /* columns of C and op(B) */
    K = A->get_shape((trans_A) ? 0 : 1); /* columns of op(A) and rows of op(B) */
    ldda = (trans_A) ? M : K;            /* leading dimension of op(A) */
    lddb = (trans_B) ? K : N;            /* leading dimension of op(B) */
    lddc = N;

    // A: MxK  B: KxN  C: MxN
    // (MxR)(RxN) + (MxN) = (MxN) + (MxN) = (MxN)

    if (A->get_memory_type() == HOST) {
        // CBLAS_TRANSPOSE a_trans = (trans_A) ? CblasTrans : CblasNoTrans;
        // CBLAS_TRANSPOSE b_trans = (trans_B) ? CblasTrans : CblasNoTrans;

        // // specify ROW MAJOR, since tensors are stored in row-major
        // cblas_dgemm(CblasRowMajor, a_trans, b_trans, M, N, K, alpha, A->get_ptr(), ldda, B->get_ptr(), lddb, beta,
        //             C->get_ptr(), lddc);

        operation a_trans = (trans_A) ? OP_T : OP_N;
        operation b_trans = (trans_B) ? OP_T : OP_N;
       
        gemm(b_trans, a_trans, N, M, K, alpha, B->get_ptr(), lddb, A->get_ptr(), ldda, beta, C->get_ptr(), lddc);

    }
#if defined(MAGMADNN_HAVE_CUDA)
    else {
        // since magma is column-major we'll need the transpose of everything
        // i.e. (AB)^T = (C)^T and the fact that (AB)^T = (B^T)(A^T)

        if (C->get_cublas_handle()) {
           cublasOperation_t a_trans = (trans_A) ? CUBLAS_OP_T : CUBLAS_OP_N;
           cublasOperation_t b_trans = (trans_B) ? CUBLAS_OP_T : CUBLAS_OP_N;

           cublasDgemm(C->get_cublas_handle(), b_trans, a_trans, N, M, K, &alpha, B->get_ptr(), lddb, A->get_ptr(), ldda, &beta, C->get_ptr(), lddc);
        }
        else {
           magma_trans_t a_trans = (trans_A) ? MagmaTrans : MagmaNoTrans;
           magma_trans_t b_trans = (trans_B) ? MagmaTrans : MagmaNoTrans;

           MAGMA_DGEMM_ROWMAJOR(A->get_ptr(), B->get_ptr(), C->get_ptr(), M, N, K, alpha, beta, a_trans, b_trans, ldda,
                                lddb, lddc);
        }
    }
#endif
}

}  // namespace math
}  // namespace magmadnn
