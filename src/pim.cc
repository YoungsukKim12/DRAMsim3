#include "pim.h"
// #include <algorithm>

namespace dramsim3 {

int NMP::GetPendingTransfers()
{
    return nmp_values.total_transfers;
}

void NMP::SetTotalTransfers(int transfers)
{
    nmp_values.total_transfers = transfers;
}

bool NMP::CheckNMPDone()
{
    return (nmp_values.total_transfers > 0 || nmp_values.nmp_buffer_queue > 0 || nmp_values.nmp_cycle_left > 0);
}

bool NMP::RunNMPLogic(int complete_transactions)
{
    bool transaction_processed = false;
    int& nmp_cycle_left = nmp_values.nmp_cycle_left;
    int& nmp_buffer_queue = nmp_values.nmp_buffer_queue;
    int& memory_transfers = nmp_values.total_transfers;

    if(nmp_cycle_left > 0)
        nmp_cycle_left--;
    else
    {
        if(nmp_buffer_queue > 0)
        {
            nmp_buffer_queue--;
            nmp_cycle_left = add_cycle_;
        }
    }

    if(complete_transactions > 0)
    {
        if(nmp_cycle_left > 0)
            nmp_buffer_queue += complete_transactions;
        else
        {
            nmp_cycle_left = add_cycle_;
            if(complete_transactions > 1)
                nmp_buffer_queue += (complete_transactions-1);
        }
        memory_transfers -= complete_transactions;
        transaction_processed = true;
    }        

    ClockTick();        

    return transaction_processed;
}


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
        std::map<uint64_t, int> read_queue;
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



void PIM::InsertPIMInst(Transaction trans)
{
    instruction_queue[trans.pim_values.batch_tag].push_back(trans);
}

void PIM::AddPIMCycle(Transaction trans)
{
    pim_cycle_left[trans.pim_values.batch_tag] += pim_cycle;
}


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

bool PIM::CommandIssuable(Transaction trans, uint64_t clk)
{
    std::vector<Transaction>& inst_queue = instruction_queue[trans.pim_values.batch_tag];
    for (auto it = inst_queue.begin(); it != inst_queue.end(); it++) 
    {
        if(trans.addr == it->addr && std::max((it->pim_values).skewed_cycle,(it->pim_values).decode_cycle) <= clk)
            return true;
    }
    return false;
}

Transaction PIM::FetchInstructionToIssue(Transaction trans, uint64_t clk)
{
    std::vector<Transaction>& inst_queue = instruction_queue[trans.pim_values.batch_tag];
    for (auto it = inst_queue.begin(); it != inst_queue.end(); it++) 
    {
        if(trans.addr == it->addr && std::max((it->pim_values).skewed_cycle,(it->pim_values).decode_cycle) <= clk)
        {
            pim_read_queue[trans.pim_values.batch_tag].insert({it->addr, 0});
            Transaction trans = *it;
            inst_queue.erase(it);

            return trans;
        }
    }
    return Transaction();
}

bool PIM::DecodeInstruction(Transaction trans)
{
    // make trans to command and insert to pim dedicated command queue (just use command queue script)
    // this might not be possible due to original cmd_queue functions related to the controller
}

Command PIM::GetCommandToIssue()
{
    // get command from pim dedicated command queue
}

std::pair<uint64_t, int> PIM::PullTransferTrans()
{
    if(transfer_complete)
    {
        transfer_complete = false;
        // std::cout << transferTrans.addr << std::endl;
        return std::make_pair(transferTrans.addr, transferTrans.is_write);
    }
    return std::make_pair(-1, -1);
}

bool PIM::RunALULogic(Transaction done_inst)
{
    if(IsTransferTrans(done_inst))
    {
        if(AllSubVecReadComplete(done_inst))
        {
            if(!LastAdditionInProgress(done_inst))
            {
                AddPIMCycle(done_inst);
                return false;
            }

            if(PIMCycleComplete(done_inst))
            {
                LastAdditionComplete(done_inst);
                EraseFromReadQueue(done_inst);
                transferTrans = done_inst;
                transfer_complete = true;
                return true;
            }
            return false;
        }
        else
            return false;
    }
    else
    {
        if(done_inst.pim_values.is_last_subvec)
        {
            if(done_inst.pim_values.is_r_vec)
            {
                AddPIMCycle(done_inst);
                return true;
            }
            else
            {
                if(AllSubVecReadComplete(done_inst))
                {
                    AddPIMCycle(done_inst);
                    EraseFromReadQueue(done_inst);
                    return true;
                }
                else
                    return false;
            }
        }
        else
        {
            if(done_inst.pim_values.is_r_vec)
                return true;
            else
            {
                IncrementSubVecCount(done_inst);
                EraseFromReadQueue(done_inst);
                return true;
            }
        }
    }
}


bool PIM::IsRVector(Transaction trans)
{
    if(trans.pim_values.is_r_vec)
        return true;
    return false;
}

bool PIM::IsTransferTrans(Transaction trans)
{
    if(trans.pim_values.vector_transfer && trans.pim_values.is_r_vec)
        return true;

    std::map<uint64_t,int> bg_read_queue = pim_read_queue[trans.pim_values.batch_tag];
    for (auto it = bg_read_queue.begin(); it != bg_read_queue.end(); it++) 
    {
        if(trans.addr == it->first && trans.pim_values.vector_transfer)
            return true;
    }
    return false;
}

void PIM::IncrementSubVecCount(Transaction trans)
{
    std::map<uint64_t,int>& bg_read_queue = pim_read_queue[trans.pim_values.batch_tag];

    int i=0;
    for(auto it=bg_read_queue.begin(); it!=bg_read_queue.end(); it++)
    {
        if(trans.pim_values.start_addr == it->first)
        {
            it->second++;
            break;
        }
        i++;
    }
}

bool PIM::AllSubVecReadComplete(Transaction trans)
{
    std::map<uint64_t,int>& bg_read_queue = pim_read_queue[trans.pim_values.batch_tag];

    for(auto it=bg_read_queue.begin(); it!=bg_read_queue.end(); it++)
    {
        if(trans.addr == it->first)
        {
            // if(trans.pim_values.is_last_subvec)
            //     std::cout << trans.pim_values.start_addr << " " << bg_read_queue.size() << " " << it->second << std::endl;

            if(it->second == (trans.pim_values.num_rds-1))
                return true;            
        }
    }
    return false;
}

// erase command from the read queue when read command is completed
void PIM::EraseFromReadQueue(Transaction trans)
{
    std::map<uint64_t, int>& bg_read_queue = pim_read_queue[trans.pim_values.batch_tag];

    for (auto it = bg_read_queue.begin(); it != bg_read_queue.end(); it++) 
    {
        if(trans.addr == it->first)
        {
            bg_read_queue.erase(it);
            break;
        }
    }
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
    // std::cout << "left trans on compelete bg pim : " << pim_read_queue[trans.pim_values.batch_tag].size() << std::endl;
}


bool PIM::PIMCycleComplete(Transaction trans)
{
    if(pim_cycle_left[trans.pim_values.batch_tag] == 0 && pim_read_queue[trans.pim_values.batch_tag].size() == 1)
        return true;
    return false;
}


}
