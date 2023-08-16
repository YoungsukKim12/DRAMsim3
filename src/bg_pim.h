#ifndef __BG_PIM
#define __BG_PIM

#include <vector>
#include "common.h"
#include "configuration.h"

namespace dramsim3 {

class BGPIM {
    public:
        BGPIM(const Config &config);
        bool CommandIssuable(Command& cmd, uint64_t clk);
        bool IsRVector(Transaction trans);
        void InsertPIMInst(Transaction trans, Command cmd);
        bool IsTransferTrans(Transaction trans);
        void ReleaseCommand(Command& cmd, uint64_t clk);
        void PrintAddress();
        void ClockTick();

    private:
        std::vector<Transaction> instruction_queue;
        std::vector<int> pim_cycle_left;
        int decode_cycle;
        int pim_cycle;
        int batch_size;
        const Config &config_;

};

}


#endif
