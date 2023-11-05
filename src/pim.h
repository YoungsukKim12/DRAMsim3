#ifndef __PIM
#define __PIM

#include <vector>
#include "common.h"
#include "configuration.h"
#include <unordered_map>

namespace dramsim3 {

class PIM {
    public:
        PIM(const Config &config);
        void ClockTick();

        // buffer functions
        void InsertPIMInst(Transaction trans);
        bool AddressInInstructionQueue(Transaction trans);

        // decode functions
        bool CommandIssuable(Transaction trans, uint64_t clk);
        Transaction FetchCommandToIssue(Transaction trans, uint64_t clk);

        // pim logics (input)
        bool IsRVector(Transaction trans);
        bool IsTransferTrans(Transaction trans);

        // pim support logics (alu)
        bool AllSubVecReadComplete(Transaction trans);
        void IncrementSubVecCount(Transaction trans);
        bool LastAdditionInProgress(Transaction trans);
        void LastAdditionComplete(Transaction trans);

        // pim logics
        void AddPIMCycle(Transaction trans);
        bool PIMCycleComplete(Transaction trans);
        void EraseFromReadQueue(Transaction trans);

    private:
        std::vector<std::vector<Transaction>> instruction_queue;
        std::vector<std::map<uint64_t,int>> pim_read_queue;
        // std::vector<std::vector<int>> pim_read_queue_subvec_count;
        std::vector<int> pim_cycle_left;
        std::vector<bool> processing_transfer_vec;
        int decode_cycle;
        int pim_cycle;
        int batch_size;
        const Config &config_;

};

}


#endif
