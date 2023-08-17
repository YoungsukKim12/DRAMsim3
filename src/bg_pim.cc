#include "bg_pim.h"
// #include <algorithm>

namespace dramsim3 {

BGPIM::BGPIM(const Config &config)
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
        transfer_vec_in_progress.push_back(false);
    }
}

void BGPIM::ClockTick()
{
    for(int i=0; i< config_.batch_size; i++)
    {
        if(pim_cycle_left[i] > 0)
            pim_cycle_left[i]--;
    }
}

// add PIM instruction to the PIM instruction queue
void BGPIM::InsertPIMInst(Transaction trans)
{
    // std::cout << "inst q size : " << "batch tag : "<< trans.pim_values.batch_tag << " " <<  instruction_queue[trans.pim_values.batch_tag].size() << std::endl;

    instruction_queue[trans.pim_values.batch_tag].push_back(trans);
    // if(trans.pim_values.vector_transfer)
    //     std::cout << "insertion : " << trans.addr << " batch : " << trans.pim_values.batch_tag << std::endl;
    // std::cout << "pim values : " << trans.pim_values.skewed_cycle << trans.pim_values.is_r_vec << trans.pim_values.vector_transfer << std::endl;
}

// check whether command's address is in the instruction queue or not
bool BGPIM::AddressInInstructionQueue(Transaction trans)
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
bool BGPIM::CommandIssuable(Transaction trans, uint64_t clk)
{
    std::vector<Transaction>& inst_queue = instruction_queue[trans.pim_values.batch_tag];

    for (auto it = inst_queue.begin(); it != inst_queue.end(); it++) 
    {
        if(trans.addr == it->addr && std::max((it->pim_values).skewed_cycle,(it->pim_values).decode_cycle) <= clk)
        {
            pim_read_queue[trans.pim_values.batch_tag].push_back(*it);
            // if(trans.pim_values.vector_transfer)
                // std::cout << "to read queue : " << it->addr << " batch : " << trans.pim_values.batch_tag << std::endl;

            // std::cout << "read queue addr : " << it->addr << " batch tag : "<< trans.pim_values.batch_tag << std::endl;
            inst_queue.erase(it);
            // std::cout << "insert : batch " << trans.pim_values.batch_tag << " read queue size : " << pim_read_queue[trans.pim_values.batch_tag].size() << std::endl;

            return true;
        }
    }


    return false;
}

// erase command from the read queue when read command is completed
void BGPIM::EraseFromReadQueue(Transaction trans)
{
    std::vector<Transaction>& read_q = pim_read_queue[trans.pim_values.batch_tag];

    for (auto it = read_q.begin(); it != read_q.end(); it++) 
    {
        if(trans.addr == it->addr)
        {
            // std::cout << "to erase read queue addr : " << it->addr << " batch tag : " << trans.pim_values.batch_tag << std::endl;
            // std::cout << "erase  : batch " << trans.pim_values.batch_tag << " read queue size : " << read_q.size() << std::endl;
            read_q.erase(it);
            // std::cout << "after erase  : batch " << trans.pim_values.batch_tag << " read queue size : " << read_q.size() << std::endl;

            break;
        }
    }
    // for(int i=0; i<batch_size; i++)
    // {
    //     std::cout << "read queue size : " << i << " " << pim_read_queue[i].size() << std::endl;
    // }
}

void BGPIM::AddPIMCycle(Transaction trans)
{
    pim_cycle_left[trans.pim_values.batch_tag] += pim_cycle;
}

bool BGPIM::IsTransferTrans(Transaction trans)
{
    std::vector<Transaction> bg_read_queue = pim_read_queue[trans.pim_values.batch_tag];
    for (auto it = bg_read_queue.begin(); it != bg_read_queue.end(); it++) 
    {
        if(trans.addr == it->addr && it->pim_values.vector_transfer)
            return true;
    }
    return false;
}

bool BGPIM::PIMCycleCompleted(Transaction trans)
{
    if(pim_cycle_left[trans.pim_values.batch_tag] == 0)
        return true;
    return false;
}

bool BGPIM::LastAdditionInProgress(Transaction trans)
{
    if(!transfer_vec_in_progress[trans.pim_values.batch_tag])
    {
        transfer_vec_in_progress[trans.pim_values.batch_tag] = true;
        return false;
    }

    return true;
}

void BGPIM::LastAdditionComplete(Transaction trans)
{
    transfer_vec_in_progress[trans.pim_values.batch_tag] = false;
}

// check whether requested transaction is r vector or not. If it is r vector, do nothing inside the controller
bool BGPIM::IsRVector(Transaction trans)
{
    // std::cout <<  "check addr : " << trans.addr << " is r vec : " << trans.is_r_vec << std::endl;
    if(trans.pim_values.is_r_vec)
        return true;
    return false;
}


// // check cmd's address to determine if the transaction with the same address inside PIM instruction queue has its vector transfer bit marked as 1
// bool BGPIM::IsTransferTrans(Transaction trans)
// {
//     if(trans.pim_values.vector_transfer)
//     {
//     // std::cout << "inside bg pim : " << trans.addr << " " << trans.vector_transfer << std :: endl; 
//         return true;
//     }
//     return false;
// }


void BGPIM::PrintAddress()
{
    // for (auto it = instruction_queue.begin(); it != instruction_queue.end(); it++) 
    // {
    //     std::cout << "addr : " << it->addr << std::endl;
    // }
    // std::cout << "----------------------------------------------" << std::endl;
    return;
}

}
