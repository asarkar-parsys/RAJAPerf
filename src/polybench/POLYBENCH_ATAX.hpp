//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) 2017-21, Lawrence Livermore National Security, LLC
// and RAJA Performance Suite project contributors.
// See the RAJAPerf/LICENSE file for details.
//
// SPDX-License-Identifier: (BSD-3-Clause)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

///
/// POLYBENCH_ATAX kernel reference implementation:
///
/// for (int i = 0; i < N; i++) {
///   y[i] = 0;
///   tmp[i] = 0;
///   for (int j = 0; j < N; j++) {
///     tmp[i] += A[i][j] * x[j];
///   }
/// }
/// for (int j = 0; j < N; j++) {
///   for (int i = 0; i < N; i++) {
///     y[j] += A[i][j] * tmp[i];
///   }
/// }


#ifndef RAJAPerf_POLYBENCH_ATAX_HPP
#define RAJAPerf_POLYBENCH_ATAX_HPP

#define POLYBENCH_ATAX_DATA_SETUP \
  Real_ptr tmp = m_tmp; \
  Real_ptr y = m_y; \
  Real_ptr x = m_x; \
  Real_ptr A = m_A; \
\
  const Index_type N = m_N;


#define POLYBENCH_ATAX_BODY1 \
  y[i] = 0.0; \
  Real_type dot = 0.0;

#define POLYBENCH_ATAX_BODY2 \
  dot += A[j + i*N] * x[j];

#define POLYBENCH_ATAX_BODY3 \
  tmp[i] = dot;

#define POLYBENCH_ATAX_BODY4 \
  Real_type dot = y[j];

#define POLYBENCH_ATAX_BODY5 \
  dot += A[j + i*N] * tmp[i];

#define POLYBENCH_ATAX_BODY6 \
  y[j] = dot;


#define POLYBENCH_ATAX_BODY1_RAJA \
  yview(i) = 0.0; \
  dot = 0.0;

#define POLYBENCH_ATAX_BODY2_RAJA \
  dot += Aview(i, j) * xview(j);

#define POLYBENCH_ATAX_BODY3_RAJA \
  tmpview(i) = dot;

#define POLYBENCH_ATAX_BODY4_RAJA \
  dot = yview(j);

#define POLYBENCH_ATAX_BODY5_RAJA \
  dot += Aview(i, j) * tmpview(i);

#define POLYBENCH_ATAX_BODY6_RAJA \
  yview(j) = dot;


#define POLYBENCH_ATAX_VIEWS_RAJA \
  using VIEW_1 = RAJA::View<Real_type, \
                            RAJA::Layout<1, Index_type, 0>>; \
\
  using VIEW_2 = RAJA::View<Real_type, \
                            RAJA::Layout<2, Index_type, 1>>; \
\
  VIEW_1 tmpview(tmp, RAJA::Layout<1>(N)); \
  VIEW_1 xview(x, RAJA::Layout<1>(N)); \
  VIEW_1 yview(y, RAJA::Layout<1>(N)); \
  VIEW_2 Aview(A, RAJA::Layout<2>(N, N));


#include "common/KernelBase.hpp"

namespace rajaperf
{

class RunParams;

namespace polybench
{

class POLYBENCH_ATAX : public KernelBase
{
public:

  POLYBENCH_ATAX(const RunParams& params);

  ~POLYBENCH_ATAX();

  void setUp(VariantID vid, size_t tid);
  void updateChecksum(VariantID vid, size_t tid);
  void tearDown(VariantID vid, size_t tid);

  void runSeqVariant(VariantID vid, size_t tid);
  void runOpenMPVariant(VariantID vid, size_t tid);
  void runCudaVariant(VariantID vid, size_t tid);
  void runHipVariant(VariantID vid, size_t tid);
  void runOpenMPTargetVariant(VariantID vid, size_t tid);

  void setCudaTuningDefinitions(VariantID vid);
  void setHipTuningDefinitions(VariantID vid);
  template < size_t block_size >
  void runCudaVariantImpl(VariantID vid);
  template < size_t block_size >
  void runHipVariantImpl(VariantID vid);

private:
  static const size_t default_gpu_block_size = 256;
  using gpu_block_sizes_type = gpu_block_size::list_type<default_gpu_block_size>;

  Index_type m_N;
  Real_ptr m_tmp;
  Real_ptr m_y;
  Real_ptr m_x;
  Real_ptr m_A;
};

} // end namespace polybench
} // end namespace rajaperf

#endif // closing endif for header file include guard
