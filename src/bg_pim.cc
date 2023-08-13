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
        if(cmd.hex_addr == it->addr && cmd.IsReadWrite() && (it->pim_values).skewed_cycle <= clk)
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
    // std::cout << "pim values : " << trans.pim_values.skewed_cycle << trans.pim_values.is_r_vec << trans.pim_values.vector_transfer << std::endl;
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
    if(trans.pim_values.is_r_vec)
        return true;
    return false;
}

// check cmd's address to determine if the transaction with the same address inside PIM instruction queue has its vector transfer bit marked as 1
bool BGPIM::IsTransferTrans(Transaction trans)
{
    if(trans.pim_values.vector_transfer)
    {
    // std::cout << "inside bg pim : " << trans.addr << " " << trans.vector_transfer << std :: endl; 
        return true;
    }
    return false;
}

}