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
    // std::cout << nmp_values.total_transfers << " "<< nmp_values.nmp_buffer_queue << " "<< nmp_values.nmp_cycle_left << std::endl;
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
    clk_++;
    ReadyPIMCommand();
}

void PIM::AddPIMCycle(Transaction trans)
{
    pim_cycle_left[trans.pim_values.batch_tag] += pim_cycle;
}


void PIM::InsertPIMInst(Transaction trans)
{
    instruction_queue[trans.pim_values.batch_tag].push_back(trans);
}

Transaction PIM::DecompressPIMInst(Transaction& trans, uint64_t clk_, std::string inst_type, int subvec_idx)
{
    // set common values
    Transaction sub_vec_trans = trans;
    sub_vec_trans.addr = trans.addr - 64*subvec_idx; 
    sub_vec_trans.pim_values.start_addr = trans.addr;
    if(subvec_idx==0)
        sub_vec_trans.pim_values.is_last_subvec = true;
    else
        sub_vec_trans.pim_values.is_last_subvec = false;

    // set specific values
    if(inst_type == "q")
    {
        sub_vec_trans.pim_values.skewed_cycle = clk_ + config_.skewed_cycle;
        sub_vec_trans.pim_values.decode_cycle = clk_ + config_.decode_cycle;
    }
    else if(inst_type == "r")
    {
        sub_vec_trans.complete_cycle = clk_+ 1*(subvec_idx+1);
    }

    return sub_vec_trans;
}

std::vector<Transaction> PIM::IssueRVector(Transaction& trans, uint64_t clk_, bool ca_compression)
{
    std::vector<Transaction> return_trans = std::vector<Transaction>();
    if(IsRVector(trans))
    {
        if(ca_compression)
        {
            for(int i=0; i<trans.pim_values.num_rds; i++)
            {
                Transaction trans = DecompressPIMInst(trans, clk_, "r", i);
                return_trans.push_back(trans);
            }
        }
        else
        {
            trans.complete_cycle = clk_+ 1;
            return_trans.push_back(trans);
        }
    }
    return return_trans;
}

bool PIM::TryInsertPIMInst(Transaction trans, uint64_t clk_, bool ca_compression)
{
    if(!AddressInInstructionQueue(trans))
    {
        if(ca_compression)
        {
            for(int i=0; i<trans.pim_values.num_rds; i++)
            {
                Transaction trans = DecompressPIMInst(trans, clk_, "q", i);
                instruction_queue[trans.pim_values.batch_tag].push_back(trans);
            }
        }
        else
        {
            trans.pim_values.skewed_cycle = clk_ + config_.skewed_cycle;
            trans.pim_values.decode_cycle = clk_ + config_.decode_cycle;
            instruction_queue[trans.pim_values.batch_tag].push_back(trans);
        }
        return true;
    }
    return false;
}

void PIM::ReadyPIMCommand()
{
    for(int i=0; i<batch_size; i++)
    {
        for(auto it = instruction_queue[i].begin(); it != instruction_queue[i].end(); it++)
        {
            if(CommandIssuable(*it, clk_))
            {
                // Transaction issue_trans = FetchInstructionToIssue(*it, clk_);
                pim_read_queue[it->pim_values.batch_tag].insert({it->addr, 0});
                instruction_queue[i].erase(it);
                issue_queue.push_back(*it);
                break;
            }
        }
    }
}

Transaction PIM::IssueFromPIM()
{
    if(issue_queue.size() == 0)
        return Transaction();

    Transaction trans = issue_queue.front();
    issue_queue.erase(issue_queue.begin());

    return trans;
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
        // if(trans.addr == it->addr)
        // {
        //     std::cout << trans.addr << std::endl;
        //     std::cout << std::max((it->pim_values).skewed_cycle,(it->pim_values).decode_cycle) << " " << clk << std::endl; 
        // }
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

// Command PIM::GetCommandToIssue()
// {
//     // get command from pim dedicated command queue
// }

std::pair<uint64_t, int> PIM::PullTransferTrans()
{
    // int tot = 0;
    // for (int i=0; i<4; i++)
    // {
    //     std::map<uint64_t, int>& bg_read_queue = pim_read_queue[i];
    //     tot += bg_read_queue.size();
    // }
    // if(tot > 300)
    //     std::cout << tot << std::endl;


    if(transfer_complete)
    {
        transfer_complete = false;
        // std:: cout << transferTrans.addr << std::endl;
        return std::make_pair(transferTrans.addr, transferTrans.is_write);
    }
    return std::make_pair(-1, -1);
}

bool PIM::RunALULogic(Transaction done_inst)
{
    if(IsTransferTrans(done_inst))
    {
        // if(IsRVector(done_inst) && done_inst.pim_values.is_last_subvec)
        // {
        //     // std::cout << "r vec done inst" << std::endl;
        //     return true;            
        // }
        if(AllSubVecReadComplete(done_inst))
        {

            if(!LastAdditionInProgress(done_inst))
                AddPIMCycle(done_inst);

            if(PIMCycleComplete(done_inst))
            {
                LastAdditionComplete(done_inst);
                EraseFromReadQueue(done_inst);
                transferTrans = done_inst;
                transfer_complete = true;
                return true;
            }
        }
        return false;
    }
    else
    {
        if(done_inst.pim_values.is_last_subvec)
        {
            if(done_inst.pim_values.is_r_vec)
            {
                // potential problem
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
