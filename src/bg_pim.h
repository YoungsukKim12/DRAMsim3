#ifndef __BG_PIM
#define __BG_PIM

#include <vector>
#include "common.h"

namespace dramsim3 {

class BGPIM {
    public:
        BGPIM();
        bool CommandIssuable(Command cmd, uint64_t clk);
        bool IsRVector(Transaction trans);
        void InsertPIMInst(Transaction trans);
        bool IsTransferTrans(Transaction trans);
        void ReleaseCommand(Command cmd, uint64_t clk);

    private:
        std::vector<Transaction> instruction_queue;
};

}


#endif
