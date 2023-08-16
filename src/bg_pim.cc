#include "bg_pim.h"
// #include <algorithm>

namespace dramsim3 {

BGPIM::BGPIM(const Config &config)
    : config_(config)
{
    batch_size = config_.batch_size;
    pim_cycle = config_.pim_cycle;
    decode_cycle = config_.decode_cycle;
    for(int i=0; i< config_.batch_size; i++)
    {
        pim_cycle_left.push_back(0);
    }
}

BGPIM::ClockTick()
{
    for(int i=0; i< config_.batch_size; i++)
    {
        if(pim_cycle_left[i] > 0)
            pim_cycle_left[i]--;
    }
}

// check whether requested command is able to issue, regarding the skewed cycle of the instruction inside PIM instruction queue
bool BGPIM::CommandIssuable(Command& cmd, uint64_t clk)
{
    for (auto it = instruction_queue.begin(); it != instruction_queue.end(); it++) 
    {
        if(cmd.hex_addr == it->addr && cmd.IsReadWrite() && std::max((it->pim_values).skewed_cycle,(it->pim_values).decode_cycle) <= clk)
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
            pim_cycle_left[cmd.Bankgroup()] = pim_cycle;
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