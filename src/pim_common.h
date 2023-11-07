#ifndef __PIMCOMMON_H
#define __PIMCOMMO_H

#include <stdint.h>
#include <vector>

struct NMPValues{
    NMPValues(){}
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
