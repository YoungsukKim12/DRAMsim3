#include "bg_pim.h"

namespace dramsim3 {

// TODO
// 1. add skewed cycle & vector transfer bit in Transaction Class - complete
// 2. add PIM_enabled inside configuration - complete
// 3. add vector_transfer marking logic at the NMP - complete
// -------------- Makefile Debugging In Progress ----------------- // -> complete!

// 4. fix the updateState, updateTiming logic -- In Progress!!!

// 5. write PIM logics
    // if not isRVec -> add command to the command queue, add the transaction inside instruction queue - complete
    // if command is issuable, issue the command - complete
    // if the issued command is the transfer vector, add it inside the return queue - complete
    // check whether PIM has been complete when calling ReturnDoneTrans - not necessary
    // add timing for PIM when using PIM - not necessary

    // need to debug...

BGPIM::BGPIM()
{
}

// if command has been issued, add it inside the accumulation register - is it needed?
// it might be sufficient to just check vector transfer command
// void BGPIM::ClockTick()
// {

// }

// check whether requested command is able to issue, regarding the skewed cycle of the instruction inside PIM instruction queue
bool BGPIM::CommandIssuable(Command& cmd, uint64_t clk)
{
    for (auto it = instruction_queue.begin(); it != instruction_queue.end(); it++) 
    {
        if(cmd.hex_addr == it->addr && cmd.IsReadWrite() && it->skewed_cycle <= clk)
            return true;
    }
    return false;
}

// erase command from the instruction buffer
void BGPIM::ReleaseCommand(Command& cmd, uint64_t clk)
{
    for (auto it = instruction_queue.begin(); it != instruction_queue.end(); it++) 
    {
        if(cmd.hex_addr == it->addr)
        {
            instruction_queue.erase(it);
            return;
        }
    }
}

// add PIM instruction to the PIM instruction queue
void BGPIM::InsertPIMInst(Transaction trans, Command cmd)
{
    instruction_queue.push_back(trans);
}

void BGPIM::PrintAddress()
{
    // for (auto it = instruction_queue.begin(); it != instruction_queue.end(); it++) 
    // {
    //     std::cout << "addr : " << it->addr << std::endl;
    // }
    // std::cout << "----------------------------------------------" << std::endl;
    return;
}

// check whether requested transaction is r vector or not. If it is r vector, do nothing inside the controller
// checking r vector is determined by looking its address
bool BGPIM::IsRVector(Transaction trans)
{
    // std::cout <<  "check addr : " << trans.addr << " is r vec : " << trans.is_r_vec << std::endl;
    // this code is for temporary use; must think of better idea
    if(trans.is_r_vec)
        return true;
    return false;
}

// check cmd's address to determine if the transaction with the same address inside PIM instruction queue has its vector transfer bit marked as 1
bool BGPIM::IsTransferTrans(Transaction trans)
{
    if(trans.vector_transfer)
    {
    // std::cout << "inside bg pim : " << trans.addr << " " << trans.vector_transfer << std :: endl; 
        return true;
    }
    return false;
}



// // PIM computation is complete
// bool BGPIM::PIMComplete()
// {

// }

// // update bank state, considering that PIM is enabled
// void BGPIM::UpdateState(const Command& cmd) {
//     if (cmd.IsRankCMD()) {
//         for (auto j = 0; j < config_.bankgroups; j++) {
//             for (auto k = 0; k < config_.banks_per_group; k++) {
//                 bank_states_[cmd.Rank()][j][k].UpdateState(cmd);
//             }
//         }
//         if (cmd.IsRefresh()) {
//             RankNeedRefresh(cmd.Rank(), false);
//         } else if (cmd.cmd_type == CommandType::SREF_ENTER) {
//             rank_is_sref_[cmd.Rank()] = true;
//         } else if (cmd.cmd_type == CommandType::SREF_EXIT) {
//             rank_is_sref_[cmd.Rank()] = false;
//         }
//     } else {
//         bank_states_[cmd.Rank()][cmd.Bankgroup()][cmd.Bank()].UpdateState(cmd);
//         if (cmd.IsRefresh()) {
//             BankNeedRefresh(cmd.Rank(), cmd.Bankgroup(), cmd.Bank(), false);
//         }
//     }
//     return;
// }

// // update bank timing, considering that PIM is enabled
// void BGPIM::UpdateTiming(const Command& cmd, uint64_t clk) {
//     switch (cmd.cmd_type) {
//         case CommandType::ACTIVATE:
//             UpdateActivationTimes(cmd.Rank(), clk);
//         case CommandType::READ:
//         case CommandType::READ_PRECHARGE:
//         case CommandType::WRITE:
//         case CommandType::WRITE_PRECHARGE:
//         case CommandType::PRECHARGE:
//         case CommandType::REFRESH_BANK:
//             // TODO - simulator speed? - Speciazlize which of the below
//             // functions to call depending on the command type  Same Bank
//             UpdateSameBankTiming(
//                 cmd.addr, timing_.same_bank[static_cast<int>(cmd.cmd_type)],
//                 clk);

//             // Same Bankgroup other banks
//             UpdateOtherBanksSameBankgroupTiming(
//                 cmd.addr,
//                 timing_
//                     .other_banks_same_bankgroup[static_cast<int>(cmd.cmd_type)],
//                 clk);

//             // Other bankgroups
//             UpdateOtherBankgroupsSameRankTiming(
//                 cmd.addr,
//                 timing_
//                     .other_bankgroups_same_rank[static_cast<int>(cmd.cmd_type)],
//                 clk);

//             // Other ranks
//             UpdateOtherRanksTiming(
//                 cmd.addr, timing_.other_ranks[static_cast<int>(cmd.cmd_type)],
//                 clk);
//             break;
//         case CommandType::REFRESH:
//         case CommandType::SREF_ENTER:
//         case CommandType::SREF_EXIT:
//             UpdateSameRankTiming(
//                 cmd.addr, timing_.same_rank[static_cast<int>(cmd.cmd_type)],
//                 clk);
//             break;
//         default:
//             AbruptExit(__FILE__, __LINE__);
//     }
//     return;

// }

}