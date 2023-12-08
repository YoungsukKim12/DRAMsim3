#ifndef __PIM
#define __PIM

#include <vector>
#include "common.h"
#include "pim_common.h"
#include "configuration.h"

namespace dramsim3 {

class NMP {
    public:
        NMP(int add_cycle):
            add_cycle_(add_cycle) {}
        // nmp logics
        void ClockTick() {clk_ = clk_ + 1;};
        int GetPendingTransfers();
        void SetTotalTransfers(int transfers);
        bool CheckNMPDone();
        bool RunNMPLogic(int complete_transactions);

    private:
        int clk_;
        NMPValues nmp_values;
        int add_cycle_;

};

class PIM {
    public:
        PIM(const Config &config);
        void ClockTick();

        // buffer functions
        void InsertPIMInst(Transaction trans);
        bool AddressInInstructionQueue(Transaction trans);

        // decode functions
        bool CommandIssuable(Transaction trans, uint64_t clk);
        Transaction FetchInstructionToIssue(Transaction trans, uint64_t clk);
        bool DecodeInstruction(Transaction trans);
        Command GetCommandToIssue();

        // pim logics (input)
        bool IsRVector(Transaction trans);
        bool IsTransferTrans(Transaction trans);

        // pim support logics (alu)
        bool AllSubVecReadComplete(Transaction trans);
        void IncrementSubVecCount(Transaction trans);
        bool LastAdditionInProgress(Transaction trans);
        void LastAdditionComplete(Transaction trans);
        bool RunALULogic(Transaction waiting_inst);

        // pim logics
        void AddPIMCycle(Transaction trans);
        bool PIMCycleComplete(Transaction trans);
        void EraseFromReadQueue(Transaction trans);
        std::pair<uint64_t, int> PullTransferTrans();

        void ReadyPIMCommand();
        bool TryInsertPIMInst(Transaction trans, uint64_t clk_, bool ca_compression);
        Transaction PIM::DecompressPIMInst(Transaction& trans, uint64_t clk_, std::string inst_type, int subvec_idx);
        std::vector<Transaction> IssueRVector(Transaction& trans, uint64_t clk_, bool ca_compression);
        Transaction IssueFromPIM();

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
        bool transfer_complete;
        Transaction transferTrans;
        std::vector<Transaction> issue_queue;
        uint64_t clk_;

};

}


#endif
