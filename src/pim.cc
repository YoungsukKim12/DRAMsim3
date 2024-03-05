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
    : config_(config),
    RankCache(128, 64, 4)
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

void PIM::ReadyPIMCommand()
{
    for(int i=0; i<batch_size; i++)
    {
        for(auto it = instruction_queue[i].begin(); it != instruction_queue[i].end(); it++)
        {
            if(CommandIssuable(*it, clk_))
            {
                // std::cout << "insert : " << it->addr << " " << it->pim_values.batch_tag << "  " << clk_ << std::endl;
                // Transaction issue_trans = FetchInstructionToIssue(*it, clk_);
                pim_read_queue[it->pim_values.batch_tag].insert({it->addr, 0});
                issue_queue.push_back(*it);
                instruction_queue[i].erase(it);
                break;
            }
        }
    }
}

bool PIM::RunALULogic(Transaction done_inst)
{
    // states
    bool is_last_subvec = done_inst.pim_values.is_last_subvec;
    bool is_q_vec = !done_inst.pim_values.is_r_vec;
    bool all_subvec_read_complete = false;
    bool last_addition_in_progress = false;
    bool pim_cycle_complete = false;
    bool process_complete = false;
    bool is_transfer_trans = IsTransferTrans(done_inst);
    if(is_last_subvec)
        all_subvec_read_complete = AllSubVecReadComplete(done_inst);
    if(is_transfer_trans)
    {
        pim_cycle_complete = PIMCycleComplete(done_inst);
        if(all_subvec_read_complete)
            last_addition_in_progress = LastAdditionInProgress(done_inst);
    }

    //logic
    if(is_last_subvec)
    {
        if(is_q_vec)
        {
            if(all_subvec_read_complete)
            {
                if(is_transfer_trans && !last_addition_in_progress)
                    AddPIMCycle(done_inst);
                else if(!is_transfer_trans)
                {
                    AddPIMCycle(done_inst);
                    EraseFromReadQueue(done_inst);
                    process_complete = true;                
                }
                else if (is_transfer_trans && pim_cycle_complete)
                {
                    LastAdditionComplete(done_inst);
                    EraseFromReadQueue(done_inst);
                    transferTrans = done_inst;
                    transfer_complete = true;
                    process_complete = true;                    
                }
            }
        }
        else
        {
            // regard last r subvec issue as whole r vec issue
            AddPIMCycle(done_inst);
            process_complete = true;                
        }
    }
    else
    {
        if(is_q_vec)
        {
            IncrementSubVecCount(done_inst);
            EraseFromReadQueue(done_inst);
            process_complete = true;                
        }
        else
        {
            process_complete = true;                
        }
    }
    return process_complete;
}

Transaction PIM::DecompressPIMInst(Transaction trans, uint64_t clk_, std::string inst_type, int subvec_idx)
{
    Transaction sub_vec_trans = trans;
    sub_vec_trans.addr = trans.addr - 64*subvec_idx; 
    sub_vec_trans.pim_values.start_addr = trans.addr;
    if(subvec_idx==0)
        sub_vec_trans.pim_values.is_last_subvec = true;
    else
    {
        sub_vec_trans.pim_values.is_last_subvec = false;
        if(trans.pim_values.vector_transfer)
            sub_vec_trans.pim_values.vector_transfer = false;
    }

    // set specific values
    if(inst_type == "q")
    {
        sub_vec_trans.pim_values.skewed_cycle = clk_ + config_.skewed_cycle;
        sub_vec_trans.pim_values.decode_cycle = clk_ + config_.decode_cycle;
    }
    else if(inst_type == "r")
    {
        sub_vec_trans.complete_cycle = clk_ + 1*(subvec_idx+1);
    }

    return sub_vec_trans;
}

std::vector<Transaction> PIM::IssueRVector(Transaction& trans, uint64_t clk_, bool ca_compression)
{
    std::vector<Transaction> return_trans = std::vector<Transaction>();
    if(IsRVector(trans))
    {
        // if(ca_compression)
        // {
            for(int i=0; i<trans.pim_values.num_rds; i++)
            {
                Transaction sub_trans = DecompressPIMInst(trans, clk_, "r", i);
                return_trans.push_back(sub_trans);
            }
        // }
        // else
        // {
        //     trans.complete_cycle = clk_+ 1;
        //     return_trans.push_back(trans);
        //     // std::cout << trans << std::endl;
        // }
    }
    return return_trans;
}

bool PIM::TryInsertPIMInst(Transaction trans, uint64_t clk_, bool ca_compression)
{
    // if(ca_compression)
    // {
        for(int i=0; i<trans.pim_values.num_rds; i++)
        {
            Transaction sub_trans = DecompressPIMInst(trans, clk_, "q", i);
            if(!AddressInInstructionQueue(sub_trans))
                instruction_queue[trans.pim_values.batch_tag].push_back(sub_trans);
        }
        return true;
    // }
    // else
    // {
    //     if(!AddressInInstructionQueue(trans))
    //     {
    //         trans.pim_values.skewed_cycle = clk_ + config_.skewed_cycle;
    //         trans.pim_values.decode_cycle = clk_ + config_.decode_cycle;
    //         instruction_queue[trans.pim_values.batch_tag].push_back(trans);
    //         return true;
    //     }
    // }
    // return false;

}

Transaction PIM::IssueFromPIM()
{
    if(issue_queue.size() == 0)
        return Transaction();
    Transaction trans = issue_queue.front();
    // issue_queue.erase(issue_queue.begin());

    // std::cout << issue_queue.size() << std::endl;

    return trans;
}

void PIM::IssueComplete()
{
    issue_queue.erase(issue_queue.begin());
}

void PIM::AddPIMCycle(Transaction trans)
{
    pim_cycle_left[trans.pim_values.batch_tag] += pim_cycle;
}

void PIM::InsertPIMInst(Transaction trans)
{
    instruction_queue[trans.pim_values.batch_tag].push_back(trans);
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

std::pair<uint64_t, int> PIM::PullTransferTrans()
{
    if(transfer_complete)
    {
        transfer_complete = false;
        return std::make_pair(transferTrans.addr, transferTrans.is_write);
    }
    return std::make_pair(-1, -1);
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
    for(auto it=bg_read_queue.begin(); it!=bg_read_queue.end(); it++)
    {
        if(trans.pim_values.start_addr == it->first)
        {
            it->second++;
            break;
        }
    }
}

bool PIM::AllSubVecReadComplete(Transaction trans)
{
    std::map<uint64_t,int>& bg_read_queue = pim_read_queue[trans.pim_values.batch_tag];

    for(auto it=bg_read_queue.begin(); it!=bg_read_queue.end(); it++)
    {
        if(trans.addr == it->first)
        {   
            // std::cout << it->second << "  " << trans.pim_values.start_addr << std::endl;

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
}


bool PIM::PIMCycleComplete(Transaction trans)
{
    // std::cout << pim_read_queue[trans.pim_values.batch_tag].size() << pim_read_queue[trans.pim_values.batch_tag][0] << std::endl;
    // std::cout << pim_read_queue[trans.pim_values.batch_tag].size() << std::endl;

    if(pim_cycle_left[trans.pim_values.batch_tag] == 0 && pim_read_queue[trans.pim_values.batch_tag].size() == 1)
    {
        return true;
    }
    return false;
}


}
