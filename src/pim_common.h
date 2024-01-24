#ifndef __PIMCOMMON_H
#define __PIMCOMMO_H

#include <stdint.h>
#include <vector>

struct EmbDataInfo{
  EmbDataInfo()
        : target_addr(0),
        vec_class('o'),
        subvec_idx(0),
        is_transfer_vec(true),
        is_last_subvec(true),
        is_r_vec(vec_class == 'r'),
        start_addr(0) {}
  EmbDataInfo(uint64_t target_addr, int vec_class, int subvec_idx, bool is_transfer_vec, int num_rds, bool use_r)
        : target_addr(target_addr),
        vec_class(vec_class),
        subvec_idx(subvec_idx),
        is_transfer_vec(is_transfer_vec),
        is_last_subvec(subvec_idx == (num_rds-1)),
        is_r_vec(vec_class == 'r' && use_r),
        start_addr(target_addr + 64*((num_rds-1) - subvec_idx)) {}

  uint64_t target_addr;
  char vec_class;
  int subvec_idx;
  bool is_transfer_vec;
  bool is_last_subvec;
  bool is_r_vec;
  uint64_t start_addr;
};

struct NMPValues{
    NMPValues()
          : nmp_cycle_left(0),
            nmp_buffer_queue(0),
            total_transfers(0) {}
    NMPValues(int nmpCycleLeft, int nmpBufferQueue, int totalTransfers)
        : nmp_cycle_left(nmpCycleLeft), 
          nmp_buffer_queue(nmpBufferQueue),
          total_transfers(totalTransfers) {}

    int nmp_cycle_left;
    int nmp_buffer_queue;
    int total_transfers;
};

struct PimValues {
    PimValues()
        : skewed_cycle(0), 
          vector_transfer(false), 
          is_r_vec(false), 
          is_locality_bit(false),
          batch_tag(false),
          decode_cycle(0),
          num_rds(0),
          is_last_subvec(false),
          start_addr(0) {} // Default constructor
    
    PimValues(uint64_t skewedCycle, bool vectorTransfer, bool isRVec, bool isLocalityBit, int batchTag, int numRDs, bool isLastSubVec, uint64_t startAddr)
        : skewed_cycle(skewedCycle), 
          vector_transfer(vectorTransfer), 
          is_r_vec(isRVec), 
          is_locality_bit(isLocalityBit),
          batch_tag(batchTag),
          decode_cycle(0),
          num_rds(numRDs),
          is_last_subvec(isLastSubVec),
          start_addr(startAddr) {}

    PimValues(const PimValues& pim_values)
        : skewed_cycle(pim_values.skewed_cycle), 
          vector_transfer(pim_values.vector_transfer), 
          is_r_vec(pim_values.is_r_vec), 
          is_locality_bit(pim_values.is_locality_bit),
          batch_tag(pim_values.batch_tag),
          decode_cycle(pim_values.decode_cycle),
          num_rds(pim_values.num_rds),
          is_last_subvec(pim_values.is_last_subvec),
          start_addr(pim_values.start_addr) {}

    uint64_t skewed_cycle;
    bool vector_transfer;
    bool is_r_vec;
    bool is_locality_bit;
    int batch_tag;
    uint64_t decode_cycle;
    int num_rds;
    bool is_last_subvec;
    uint64_t start_addr;

    // int subvec_idx;
};
#endif
