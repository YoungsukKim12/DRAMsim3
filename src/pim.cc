#include "pim.h"
// #include <algorithm>

namespace dramsim3 {

PIM::PIM(const Config &config)
    : config_(config)
{
    batch_size = config_.batch_size;
    pim_cycle = config_.pim_cycle;
    decode_cycle = config_.decode_cycle;
    for(int i=0; i<config_.batch_size; i++)
    {
        pim_cycle_left.push_back(0);
    }
    for(int i = 0; i < batch_size; i++)
    {
        std::vector<Transaction> inst_queue;
        instruction_queue.push_back(inst_queue);
        std::vector<Transaction> read_queue;
        pim_read_queue.push_back(read_queue);
        processing_transfer_vec.push_back(false);
    }
}

void PIM::ClockTick()
{
    for(int i=0; i< config_.batch_size; i++)
    {
        if(pim_cycle_left[i] > 0)
            pim_cycle_left[i]--;
    }
}

// add PIM instruction to the PIM instruction queue
void PIM::InsertPIMInst(Transaction trans)
{
    instruction_queue[trans.pim_values.batch_tag].push_back(trans);
}

// check whether command's address is in the instruction queue or not
bool PIM::AddressInInstructionQueue(Transaction trans)
{
    std::vector<Transaction>& inst_queue = instruction_queue[trans.pim_values.batch_tag];
    for (auto it = inst_queue.begin(); it != inst_queue.end(); it++) 
    {
        if(trans.addr == it->addr)
            return true;
    }
    return false;
}

// check whether command is issuable to the device or not
bool PIM::CommandIssuable(Transaction trans, uint64_t clk)
{
    std::vector<Transaction>& inst_queue = instruction_queue[trans.pim_values.batch_tag];

    for (auto it = inst_queue.begin(); it != inst_queue.end(); it++) 
    {
        if(trans.addr == it->addr && std::max((it->pim_values).skewed_cycle,(it->pim_values).decode_cycle) <= clk)
        {
            pim_read_queue[trans.pim_values.batch_tag].push_back(*it);
            inst_queue.erase(it);
            return true;
        }
    }


    return false;
}

// erase command from the read queue when read command is completed
void PIM::EraseFromReadQueue(Transaction trans)
{
    std::vector<Transaction>& read_q = pim_read_queue[trans.pim_values.batch_tag];

    for (auto it = read_q.begin(); it != read_q.end(); it++) 
    {
        if(trans.addr == it->addr)
        {
            read_q.erase(it);
            break;
        }
    }
}

void PIM::AddPIMCycle(Transaction trans)
{
    pim_cycle_left[trans.pim_values.batch_tag] += pim_cycle;
}

bool PIM::IsTransferTrans(Transaction trans)
{
    if(trans.pim_values.vector_transfer && trans.pim_values.is_r_vec)
        return true;

    std::vector<Transaction> bg_read_queue = pim_read_queue[trans.pim_values.batch_tag];
    for (auto it = bg_read_queue.begin(); it != bg_read_queue.end(); it++) 
    {
        if(trans.addr == it->addr && it->pim_values.vector_transfer)
            return true;
    }
    return false;
}

bool PIM::PIMCycleComplete(Transaction trans)
{
    if(pim_cycle_left[trans.pim_values.batch_tag] == 0)
        return true;
    return false;
}

bool PIM::LastAdditionInProgress(Transaction trans)
{
    if(!processing_transfer_vec[trans.pim_values.batch_tag])
    {
        processing_transfer_vec[trans.pim_values.batch_tag] = true;
        return false;
    }

    return true;
}

void PIM::LastAdditionComplete(Transaction trans)
{
    processing_transfer_vec[trans.pim_values.batch_tag] = false;
}

// check whether requested transaction is r vector or not. If it is r vector, do nothing inside the controller
bool PIM::IsRVector(Transaction trans)
{
    // std::cout <<  "check addr : " << trans.addr << " is r vec : " << trans.is_r_vec << std::endl;
    if(trans.pim_values.is_r_vec)
        return true;
    return false;
}


}
