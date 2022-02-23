//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) 2017-21, Lawrence Livermore National Security, LLC
// and RAJA Performance Suite project contributors.
// See the RAJAPerf/LICENSE file for details.
//
// SPDX-License-Identifier: (BSD-3-Clause)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

#include "HALOEXCHANGE.hpp"

#include "RAJA/RAJA.hpp"

#if defined(RAJA_ENABLE_CUDA)

#include "common/CudaDataUtils.hpp"

#include <iostream>

namespace rajaperf
{
namespace apps
{

#define HALOEXCHANGE_DATA_SETUP_CUDA \
  for (Index_type v = 0; v < m_num_vars; ++v) { \
    allocAndInitCudaDeviceData(vars[v], m_vars[v], m_var_size); \
  } \
  for (Index_type l = 0; l < num_neighbors; ++l) { \
    allocAndInitCudaDeviceData(buffers[l], m_buffers[l], m_num_vars*m_pack_index_list_lengths[l]); \
    allocAndInitCudaDeviceData(pack_index_lists[l], m_pack_index_lists[l], m_pack_index_list_lengths[l]); \
    allocAndInitCudaDeviceData(unpack_index_lists[l], m_unpack_index_lists[l], m_unpack_index_list_lengths[l]); \
  }

#define HALOEXCHANGE_DATA_TEARDOWN_CUDA \
  for (Index_type l = 0; l < num_neighbors; ++l) { \
    deallocCudaDeviceData(unpack_index_lists[l]); \
    deallocCudaDeviceData(pack_index_lists[l]); \
    deallocCudaDeviceData(buffers[l]); \
  } \
  for (Index_type v = 0; v < m_num_vars; ++v) { \
    getCudaDeviceData(m_vars[v], vars[v], m_var_size); \
    deallocCudaDeviceData(vars[v]); \
  }

template < size_t block_size >
__launch_bounds__(block_size)
__global__ void haloexchange_pack(Real_ptr buffer, Int_ptr list, Real_ptr var,
                                  Index_type len)
{
   Index_type i = threadIdx.x + blockIdx.x * block_size;

   if (i < len) {
     HALOEXCHANGE_PACK_BODY;
   }
}

template < size_t block_size >
__launch_bounds__(block_size)
__global__ void haloexchange_unpack(Real_ptr buffer, Int_ptr list, Real_ptr var,
                                    Index_type len)
{
   Index_type i = threadIdx.x + blockIdx.x * block_size;

   if (i < len) {
     HALOEXCHANGE_UNPACK_BODY;
   }
}


template < size_t block_size >
void HALOEXCHANGE::runCudaVariantImpl(VariantID vid)
{
  const Index_type run_reps = getRunReps();

  HALOEXCHANGE_DATA_SETUP;

  if ( vid == Base_CUDA ) {

    HALOEXCHANGE_DATA_SETUP_CUDA;

    startTimer();
    for (RepIndex_type irep = 0; irep < run_reps; ++irep) {

      for (Index_type l = 0; l < num_neighbors; ++l) {
        Real_ptr buffer = buffers[l];
        Int_ptr list = pack_index_lists[l];
        Index_type  len  = pack_index_list_lengths[l];
        for (Index_type v = 0; v < num_vars; ++v) {
          Real_ptr var = vars[v];
          dim3 nthreads_per_block(block_size);
          dim3 nblocks((len + block_size-1) / block_size);
          haloexchange_pack<block_size><<<nblocks, nthreads_per_block>>>(buffer, list, var, len);
          cudaErrchk( cudaGetLastError() );
          buffer += len;
        }
      }
      synchronize();

      for (Index_type l = 0; l < num_neighbors; ++l) {
        Real_ptr buffer = buffers[l];
        Int_ptr list = unpack_index_lists[l];
        Index_type  len  = unpack_index_list_lengths[l];
        for (Index_type v = 0; v < num_vars; ++v) {
          Real_ptr var = vars[v];
          dim3 nthreads_per_block(block_size);
          dim3 nblocks((len + block_size-1) / block_size);
          haloexchange_unpack<block_size><<<nblocks, nthreads_per_block>>>(buffer, list, var, len);
          cudaErrchk( cudaGetLastError() );
          buffer += len;
        }
      }
      synchronize();

    }
    stopTimer();

    HALOEXCHANGE_DATA_TEARDOWN_CUDA;

  } else if ( vid == RAJA_CUDA ) {

    HALOEXCHANGE_DATA_SETUP_CUDA;

    using EXEC_POL = RAJA::cuda_exec<block_size, true /*async*/>;

    startTimer();
    for (RepIndex_type irep = 0; irep < run_reps; ++irep) {

      for (Index_type l = 0; l < num_neighbors; ++l) {
        Real_ptr buffer = buffers[l];
        Int_ptr list = pack_index_lists[l];
        Index_type  len  = pack_index_list_lengths[l];
        for (Index_type v = 0; v < num_vars; ++v) {
          Real_ptr var = vars[v];
          auto haloexchange_pack_base_lam = [=] __device__ (Index_type i) {
                HALOEXCHANGE_PACK_BODY;
              };
          RAJA::forall<EXEC_POL>(
              RAJA::TypedRangeSegment<Index_type>(0, len),
              haloexchange_pack_base_lam );
          buffer += len;
        }
      }
      synchronize();

      for (Index_type l = 0; l < num_neighbors; ++l) {
        Real_ptr buffer = buffers[l];
        Int_ptr list = unpack_index_lists[l];
        Index_type  len  = unpack_index_list_lengths[l];
        for (Index_type v = 0; v < num_vars; ++v) {
          Real_ptr var = vars[v];
          auto haloexchange_unpack_base_lam = [=] __device__ (Index_type i) {
                HALOEXCHANGE_UNPACK_BODY;
              };
          RAJA::forall<EXEC_POL>(
              RAJA::TypedRangeSegment<Index_type>(0, len),
              haloexchange_unpack_base_lam );
          buffer += len;
        }
      }
      synchronize();

    }
    stopTimer();

    HALOEXCHANGE_DATA_TEARDOWN_CUDA;

  } else {
     getCout() << "\n HALOEXCHANGE : Unknown Cuda variant id = " << vid << std::endl;
  }
}

void HALOEXCHANGE::runCudaVariant(VariantID vid, size_t tid)
{
  if ( !gpu_block_size::invoke_or(
           gpu_block_size::RunCudaBlockSize<HALOEXCHANGE>(*this, vid), gpu_block_sizes_type()) ) {
    std::cout << "\n  HALOEXCHANGE : Unsupported Cuda block_size " << getActualGPUBlockSize()
              <<" for variant id = " << vid << std::endl;
  }
}

} // end namespace apps
} // end namespace rajaperf

#endif  // RAJA_ENABLE_CUDA
