#ifndef __PIMCOMMON_H
#define __PIMCOMMON_H

#include <stdint.h>
#include <vector>

// struct NMPValues{
//     NMPValues()
//           : nmp_cycle_left(0),
//             nmp_buffer_queue(0),
//             total_transfers(0) {}
//     NMPValues(int nmpCycleLeft, int nmpBufferQueue, int totalTransfers)
//         : nmp_cycle_left(nmpCycleLeft), 
//           nmp_buffer_queue(nmpBufferQueue),
//           total_transfers(totalTransfers) {}

//     int nmp_cycle_left;
//     int nmp_buffer_queue;
//     int total_transfers;
// };

struct PimValues {
    PimValues()
        : vlen(0),
          prefetch_cmd(false),
          transfer_cmd(false),
          read_all_cmd(false),
          skewed_cycle(0), 
          decode_cycle(0)
          {} // Default constructor
    
    PimValues(int vlen, bool prefetchCmd, bool transferCmd, bool readAllCmd, int skewedCycle, int decodeCycle)
        : vlen(vlen),
          prefetch_cmd(prefetchCmd),
          transfer_cmd(transferCmd),
          read_all_cmd(readAllCmd),
          skewed_cycle(skewedCycle), 
          decode_cycle(decodeCycle)
          {}

    PimValues(const PimValues& pim_values)
        : vlen(pim_values.vlen),
          prefetch_cmd(pim_values.prefetch_cmd),
          transfer_cmd(pim_values.transfer_cmd),
          read_all_cmd(pim_values.read_all_cmd),
          skewed_cycle(pim_values.skewed_cycle), 
          decode_cycle(pim_values.decode_cycle)
          {}

    int vlen;
    bool prefetch_cmd;
    bool transfer_cmd;
    bool read_all_cmd;
    int skewed_cycle;
    int decode_cycle;

};
#endif
