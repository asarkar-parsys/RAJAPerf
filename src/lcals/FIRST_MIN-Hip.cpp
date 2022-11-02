//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) 2017-22, Lawrence Livermore National Security, LLC
// and RAJA Performance Suite project contributors.
// See the RAJAPerf/LICENSE file for details.
//
// SPDX-License-Identifier: (BSD-3-Clause)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

#include "FIRST_MIN.hpp"

#include "RAJA/RAJA.hpp"

#if defined(RAJA_ENABLE_HIP)

#include "common/HipDataUtils.hpp"

#include <iostream>

namespace rajaperf
{
namespace lcals
{

#define FIRST_MIN_DATA_SETUP_HIP \
  allocAndInitHipDeviceData(x, m_x, m_N);

#define FIRST_MIN_DATA_TEARDOWN_HIP \
  deallocHipDeviceData(x);

template < size_t block_size >
__launch_bounds__(block_size)
__global__ void first_min(Real_ptr x,
                          MyMinLoc** dminloc,
                          Index_type iend)
{
  extern __shared__ MyMinLoc minloc[ ];

  Index_type i = blockIdx.x * block_size + threadIdx.x;

  minloc[ threadIdx.x ] = *dminloc[blockIdx.x];

  for ( ; i < iend ; i += gridDim.x * block_size ) {
    MyMinLoc& mymin = minloc[ threadIdx.x ];
    FIRST_MIN_BODY;
  }
  __syncthreads();

  for ( i = block_size / 2; i > 0; i /= 2 ) {
    if ( threadIdx.x < i ) {
      if ( minloc[ threadIdx.x + i].val < minloc[ threadIdx.x ].val ) {
        minloc[ threadIdx.x ] = minloc[ threadIdx.x + i];
      }
    }
     __syncthreads();
  }

  if ( threadIdx.x == 0 ) {
    if ( minloc[ 0 ].val < (*dminloc[blockIdx.x]).val ) {
      *dminloc[blockIdx.x] = minloc[ 0 ];
    }
  }
}


template < size_t block_size >
void FIRST_MIN::runHipVariantImpl(VariantID vid)
{
  const Index_type run_reps = getRunReps();
  const Index_type ibegin = 0;
  const Index_type iend = getActualProblemSize();

  FIRST_MIN_DATA_SETUP;

  if ( vid == Base_HIP ) {

    FIRST_MIN_DATA_SETUP_HIP;

    startTimer();
    for (RepIndex_type irep = 0; irep < run_reps; ++irep) {

       FIRST_MIN_MINLOC_INIT;

       const size_t grid_size = RAJA_DIVIDE_CEILING_INT(iend, block_size);
       MyMinLoc** dminloc;
       hipErrchk( hipMalloc( (void**)&dminloc, grid_size * sizeof(MyMinLoc) ) );
       for (Index_type i=0;i<static_cast<Index_type>(grid_size);i++){

   	    hipErrchk( hipMalloc( (void**)&(dminloc[i]), sizeof(MyMinLoc) ) );
            hipErrchk( hipMemcpy( dminloc[i], &mymin, sizeof(MyMinLoc),
                               hipMemcpyHostToDevice ) );      
	}
       hipLaunchKernelGGL((first_min<block_size>), grid_size, block_size,
                   sizeof(MyMinLoc)*block_size, 0, x,
                                                   dminloc,
                                                   iend );
       MyMinLoc mymin_block[grid_size]; //per-block min value
       for (Index_type i=0;i<static_cast<Index_type>(grid_size);i++){
           hipErrchk( hipMemcpy( &(mymin_block[i]), dminloc[i], sizeof(MyMinLoc),
                                  hipMemcpyDeviceToHost ) );
	   hipErrchk( hipGetLastError() );			  
	   m_minloc = RAJA_MAX(m_minloc, (mymin_block[i]).loc);
       }
       
       for (Index_type i=0;i<static_cast<Index_type>(grid_size);i++){
	       hipErrchk( hipFree(dminloc[i]));
       }
       hipErrchk( hipFree( dminloc ) );

    }
    stopTimer();

    FIRST_MIN_DATA_TEARDOWN_HIP;

  } else if ( vid == RAJA_HIP ) {

    FIRST_MIN_DATA_SETUP_HIP;

    startTimer();
    for (RepIndex_type irep = 0; irep < run_reps; ++irep) {

       RAJA::ReduceMinLoc<RAJA::hip_reduce, Real_type, Index_type> loc(
                                                        m_xmin_init, m_initloc);

       RAJA::forall< RAJA::hip_exec<block_size, true /*async*/> >(
         RAJA::RangeSegment(ibegin, iend), [=] __device__ (Index_type i) {
         FIRST_MIN_BODY_RAJA;
       });

       m_minloc = RAJA_MAX(m_minloc, loc.getLoc());

    }
    stopTimer();

    FIRST_MIN_DATA_TEARDOWN_HIP;

  } else {
     getCout() << "\n  FIRST_MIN : Unknown Hip variant id = " << vid << std::endl;
  }
}

RAJAPERF_GPU_BLOCK_SIZE_TUNING_DEFINE_BIOLERPLATE(FIRST_MIN, Hip)

} // end namespace lcals
} // end namespace rajaperf

#endif  // RAJA_ENABLE_HIP
