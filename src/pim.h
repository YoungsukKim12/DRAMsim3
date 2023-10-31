#ifndef __PIM
#define __PIM

#include <vector>
#include "common.h"
#include "configuration.h"

namespace dramsim3 {

class PIM {
    public:
        PIM(const Config &config);
        bool CommandIssuable(Transaction trans, uint64_t clk);
        bool IsRVector(Transaction trans);
        void InsertPIMInst(Transaction trans);
        void EraseFromReadQueue(Transaction trans);
        void AddPIMCycle(Transaction trans);
        bool IsTransferTrans(Transaction trans);
        bool PIMCycleComplete(Transaction trans);
        bool AddressInInstructionQueue(Transaction trans);
        bool LastAdditionInProgress(Transaction trans);
        void LastAdditionComplete(Transaction trans);
        void ClockTick();

    private:
        std::vector<std::vector<Transaction>> instruction_queue;
        std::vector<std::vector<Transaction>> pim_read_queue;
        std::vector<int> pim_cycle_left;
        std::vector<bool> processing_transfer_vec;
        int decode_cycle;
        int pim_cycle;
        int batch_size;
        const Config &config_;

};

}


#endif
