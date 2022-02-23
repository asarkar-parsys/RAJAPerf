//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) 2017-21, Lawrence Livermore National Security, LLC
// and RAJA Performance Suite project contributors.
// See the RAJAPerf/LICENSE file for details.
//
// SPDX-License-Identifier: (BSD-3-Clause)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

#include "DIFF_PREDICT.hpp"

#include "RAJA/RAJA.hpp"

#if defined(RAJA_ENABLE_HIP)

#include "common/HipDataUtils.hpp"

#include <iostream>

namespace rajaperf
{
namespace lcals
{

#define DIFF_PREDICT_DATA_SETUP_HIP \
  allocAndInitHipDeviceData(px, m_px, m_array_length); \
  allocAndInitHipDeviceData(cx, m_cx, m_array_length);

#define DIFF_PREDICT_DATA_TEARDOWN_HIP \
  getHipDeviceData(m_px, px, m_array_length); \
  deallocHipDeviceData(px); \
  deallocHipDeviceData(cx);

template < size_t block_size >
__launch_bounds__(block_size)
__global__ void diff_predict(Real_ptr px, Real_ptr cx,
                             const Index_type offset,
                             Index_type iend)
{
   Index_type i = blockIdx.x * block_size + threadIdx.x;
   if (i < iend) {
     DIFF_PREDICT_BODY;
   }
}


template < size_t block_size >
void DIFF_PREDICT::runHipVariantImpl(VariantID vid)
{
  const Index_type run_reps = getRunReps();
  const Index_type ibegin = 0;
  const Index_type iend = getActualProblemSize();

  DIFF_PREDICT_DATA_SETUP;

  if ( vid == Base_HIP ) {

    DIFF_PREDICT_DATA_SETUP_HIP;

    startTimer();
    for (RepIndex_type irep = 0; irep < run_reps; ++irep) {

       const size_t grid_size = RAJA_DIVIDE_CEILING_INT(iend, block_size);
       hipLaunchKernelGGL((diff_predict<block_size>), dim3(grid_size), dim3(block_size), 0, 0,  px, cx,
                                                offset,
                                                iend );
       hipErrchk( hipGetLastError() );

    }
    stopTimer();

    DIFF_PREDICT_DATA_TEARDOWN_HIP;

  } else if ( vid == RAJA_HIP ) {

    DIFF_PREDICT_DATA_SETUP_HIP;

    startTimer();
    for (RepIndex_type irep = 0; irep < run_reps; ++irep) {

       RAJA::forall< RAJA::hip_exec<block_size, true /*async*/> >(
         RAJA::RangeSegment(ibegin, iend), [=] __device__ (Index_type i) {
         DIFF_PREDICT_BODY;
       });

    }
    stopTimer();

    DIFF_PREDICT_DATA_TEARDOWN_HIP;

  } else {
     getCout() << "\n  DIFF_PREDICT : Unknown Hip variant id = " << vid << std::endl;
  }
}

void DIFF_PREDICT::runHipVariant(VariantID vid, size_t tid)
{
  if ( !gpu_block_size::invoke_or(
           gpu_block_size::RunHipBlockSize<DIFF_PREDICT>(*this, vid), gpu_block_sizes_type()) ) {
    std::cout << "\n  DIFF_PREDICT : Unsupported Hip block_size " << getActualGPUBlockSize()
              <<" for variant id = " << vid << std::endl;
  }
}

} // end namespace lcals
} // end namespace rajaperf

#endif  // RAJA_ENABLE_HIP
