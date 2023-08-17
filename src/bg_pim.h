#ifndef __BG_PIM
#define __BG_PIM

#include <vector>
#include "common.h"
#include "configuration.h"

namespace dramsim3 {

class BGPIM {
    public:
        BGPIM(const Config &config);
        bool CommandIssuable(Transaction trans, uint64_t clk);
        bool IsRVector(Transaction trans);
        void InsertPIMInst(Transaction trans);
        void EraseFromReadQueue(Transaction trans);
        void AddPIMCycle(Transaction trans);
        bool IsTransferTrans(Transaction trans);
        void ReleaseCommand(Command& cmd, uint64_t clk);
        bool PIMCycleCompleted(Transaction trans);
        bool AddressInInstructionQueue(Transaction trans);
        bool LastAdditionInProgress(Transaction trans);
        void LastAdditionComplete(Transaction trans);

        void PrintAddress();
        void ClockTick();

    private:
        std::vector<std::vector<Transaction>> instruction_queue;
        std::vector<std::vector<Transaction>> pim_read_queue;
        std::vector<int> pim_cycle_left;
        std::vector<bool> transfer_vec_in_progress;
        int decode_cycle;
        int pim_cycle;
        int batch_size;
        const Config &config_;

};

}


#endif
