#include "cpu.h"
#include <sstream>
#include <string>
namespace dramsim3 {

void RandomCPU::ClockTick() {
    // Create random CPU requests at full speed
    // this is useful to exploit the parallelism of a DRAM protocol
    // and is also immune to address mapping and scheduling policies
    memory_system_.ClockTick();
    if (get_next_) {
        last_addr_ = gen();
        last_write_ = (gen() % 3 == 0);
    }
    get_next_ = memory_system_.WillAcceptTransaction(last_addr_, last_write_);
    if (get_next_) {
        PimValues pim_values;
        memory_system_.AddTransaction(last_addr_, last_write_, pim_values);
    }
    clk_++;
    return;
}

void StreamCPU::ClockTick() {
    // stream-add, read 2 arrays, add them up to the third array
    // this is a very simple approximate but should be able to produce
    // enough buffer hits

    // moving on to next set of arrays
    memory_system_.ClockTick();
    if (offset_ >= array_size_ || clk_ == 0) {
        addr_a_ = gen();
        addr_b_ = gen();
        addr_c_ = gen();
        offset_ = 0;
    }
    PimValues pim_values;
    if (!inserted_a_ &&
        memory_system_.WillAcceptTransaction(addr_a_ + offset_, false)) {
        memory_system_.AddTransaction(addr_a_ + offset_, false, pim_values);
        inserted_a_ = true;
    }
    if (!inserted_b_ &&
        memory_system_.WillAcceptTransaction(addr_b_ + offset_, false)) {
        memory_system_.AddTransaction(addr_b_ + offset_, false, pim_values);
        inserted_b_ = true;
    }
    if (!inserted_c_ &&
        memory_system_.WillAcceptTransaction(addr_c_ + offset_, true)) {
        memory_system_.AddTransaction(addr_c_ + offset_, true, pim_values);
        inserted_c_ = true;
    }
    // moving on to next element
    if (inserted_a_ && inserted_b_ && inserted_c_) {
        offset_ += stride_;
        inserted_a_ = false;
        inserted_b_ = false;
        inserted_c_ = false;
    }
    clk_++;
    return;
}

TraceBasedCPU::TraceBasedCPU(const std::string& config_file,
                             const std::string& output_dir,
                             const std::string& trace_file)
    : CPU(config_file, output_dir) {
    trace_file_.open(trace_file);
    if (trace_file_.fail()) {
        std::cerr << "Trace file does not exist" << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }
}

void TraceBasedCPU::ClockTick() {
    memory_system_.ClockTick();
    if (!trace_file_.eof()) {
        if (get_next_) {
            get_next_ = false;
            trace_file_ >> trans_;
        }
        if (trans_.added_cycle <= clk_) {
            get_next_ = memory_system_.WillAcceptTransaction(trans_.addr,
                                                             trans_.is_write);
            if (get_next_) {
                PimValues pim_values;
                memory_system_.AddTransaction(trans_.addr, trans_.is_write, pim_values);
            }
        }
    }
    clk_++;
    return;
}

TraceBasedCPUForHeterogeneousMemory::TraceBasedCPUForHeterogeneousMemory(const std::string& config_file_HBM, 
                                                                            const std::string& config_file_DIMM, 
                                                                            const std::string& output_dir,
                                                                            const std::string& trace_file)
    : CPU(config_file_HBM, output_dir),
    memory_system_HBM(config_file_HBM, output_dir,
        std::bind(&TraceBasedCPUForHeterogeneousMemory::ReadCallBack_HBM, this, std::placeholders::_1),
        std::bind(&TraceBasedCPUForHeterogeneousMemory::WriteCallBack_HBM, this, std::placeholders::_1)),
    memory_system_DIMM(config_file_DIMM, output_dir,
            std::bind(&TraceBasedCPUForHeterogeneousMemory::ReadCallBack_DIMM, this, std::placeholders::_1),
            std::bind(&TraceBasedCPUForHeterogeneousMemory::WriteCallBack_DIMM, this, std::placeholders::_1)),
    clk_HBM(0),
    clk_DIMM(0),
    HBM_complete_addr(-1),
    DIMM_complete_addr(-1),
    num_rds(1),
    num_ca_in_cycle(1)
{
    hbm_config = memory_system_HBM.config_copy;
    channels = hbm_config->channels;
    bankgroups = hbm_config->bankgroups;
    is_using_HEAM = hbm_config->PIM_enabled;
    is_using_LUT = hbm_config->LUT_enabled;
    batch_size = hbm_config->batch_size;
    addrmapping = hbm_config->address_mapping;
    if(!is_using_HEAM)
        batch_size=1;

    for(int i=0; i < channels*bankgroups; i++)
    {
        loads_per_bg.push_back(0);
        loads_per_bg_for_q.push_back(0);
    }

    CA_compression = hbm_config->CA_compression;
    if(CA_compression)
    {
        if (trace_file.find("64") != std::string::npos)
            num_rds = 1;
        else if (trace_file.find("128") != std::string::npos)
            num_rds = 2;
        else if (trace_file.find("256") != std::string::npos)
            num_rds = 4;
        else if (trace_file.find("512") != std::string::npos)
            num_rds = 8;

        num_ca_in_cycle = 3;
    }

    LoadTrace(trace_file);

    std::cout << "------------ Info ------------" << std::endl;
    std::cout << "- using HEAM : " << is_using_HEAM << std::endl;
    std::cout << "- using LUT : " << is_using_LUT << std::endl;
    std::cout << "- CA compression : " << CA_compression << std::endl;
    std::cout << "- batch size : " << batch_size << std::endl;
    std::cout << "- addr mapping : " << addrmapping << std::endl;

    int total_cycle = RunHEAM();

    std::cout << "- total cycle : " << total_cycle << std::endl;
    std::cout << "-----------------------------" << std::endl;

}
int TraceBasedCPUForHeterogeneousMemory::RunTRiM() {
    std::vector<int> DIMM_vectors_left;
    std::vector<std::unordered_map<int, uint64_t>> pim_transfer_address;
    int batch_start_index = 0;
    int memory_transfers = 0;
    int nmp_cycle_left = 0;
    int nmp_buffer_queue = 0;
    int batch_tag = 0;
    int total_batch = (int)HBM_transaction.size()/batch_size;

    for(int i=0; i < total_batch-1; i++)
    {
        batch_start_index = i*batch_size;
        batch_tag = 0;
        DIMM_vectors_left.clear();
        pim_transfer_address.clear();
        // GetNextBatchInformation(batch_start_index, memory_transfers, HBM_vectors_left, DIMM_vectors_left, pim_transfer_address);
        // std::cout << "mem transfers left : " << memory_transfers << std::endl;

        while(memory_transfers > 0 || nmp_buffer_queue > 0 || nmp_cycle_left > 0)
        {
            ClockTick();
            // AddBatchTransactions(batch_start_index, batch_tag, HBM_vectors_left, DIMM_vectors_left, pim_transfer_address);

            if(nmp_cycle_left > 0)
                nmp_cycle_left--;
            else
            {
                if(nmp_buffer_queue > 0)
                {
                    nmp_buffer_queue--;
                    nmp_cycle_left = add_cycle;
                }
            }

            if(complete_transactions > 0)
            {
                if(nmp_cycle_left > 0)
                    nmp_buffer_queue += complete_transactions;
                else
                {
                    nmp_cycle_left = add_cycle;
                    if(complete_transactions > 1)
                        nmp_buffer_queue += (complete_transactions-1);
                }
                memory_transfers -= complete_transactions;
                complete_transactions = 0;
            }                
        // std::cout << "mem transfers left : " << memory_transfers << std::endl;
        }
        ////////// For DEBUG ////////////
        // // if(i%1024 == 0)
        std::cout << i << " of " << total_batch << " processed" << std::endl;

        // for(int i=0; i < loads_per_bg.size(); i++)
        // {
        //     std::cout << "bg " << i << " / load : " << loads_per_bg[i] << std::endl;
        //     std::cout << "bg " << i << " / q load : " << loads_per_bg_for_q[i] << std::endl;
        //     loads_per_bg[i] = 0;
        //     loads_per_bg_for_q[i] = 0;
        // }
    }

    return clk_HBM;

}

int TraceBasedCPUForHeterogeneousMemory::RunHEAM() {

    std::vector<int> HBM_vectors_left;
    std::vector<int> DIMM_vectors_left;
    std::vector<std::unordered_map<int, uint64_t>> pim_transfer_address;
    int batch_start_index = 0;
    int memory_transfers = 0;
    int nmp_cycle_left = 0;
    int nmp_buffer_queue = 0;
    int batch_tag = 0;
    int total_batch = (int)HBM_transaction.size()/batch_size;

    for(int i=0; i < total_batch-1; i++)
    {
        batch_start_index = i*batch_size;
        batch_tag = 0;
        HBM_vectors_left.clear();
        DIMM_vectors_left.clear();
        pim_transfer_address.clear();
        GetNextBatchInformation(batch_start_index, memory_transfers, HBM_vectors_left, DIMM_vectors_left, pim_transfer_address);
        // std::cout << "mem transfers left : " << memory_transfers << std::endl;

        while(memory_transfers > 0 || nmp_buffer_queue > 0 || nmp_cycle_left > 0)
        {
            ClockTick();
            AddBatchTransactions(batch_start_index, batch_tag, HBM_vectors_left, DIMM_vectors_left, pim_transfer_address);

            if(nmp_cycle_left > 0)
                nmp_cycle_left--;
            else
            {
                if(nmp_buffer_queue > 0)
                {
                    nmp_buffer_queue--;
                    nmp_cycle_left = add_cycle;
                }
            }

            if(complete_transactions > 0)
            {
                if(nmp_cycle_left > 0)
                    nmp_buffer_queue += complete_transactions;
                else
                {
                    nmp_cycle_left = add_cycle;
                    if(complete_transactions > 1)
                        nmp_buffer_queue += (complete_transactions-1);
                }
                memory_transfers -= complete_transactions;
                complete_transactions = 0;
            }                
        // std::cout << "mem transfers left : " << memory_transfers << std::endl;
        }
        ////////// For DEBUG ////////////
        // // if(i%1024 == 0)
        std::cout << i << " of " << total_batch << " processed" << std::endl;

        // for(int i=0; i < loads_per_bg.size(); i++)
        // {
        //     std::cout << "bg " << i << " / load : " << loads_per_bg[i] << std::endl;
        //     std::cout << "bg " << i << " / q load : " << loads_per_bg_for_q[i] << std::endl;
        //     loads_per_bg[i] = 0;
        //     loads_per_bg_for_q[i] = 0;
        // }
    }

    return clk_HBM;
}

void TraceBasedCPUForHeterogeneousMemory::GetNextBatchInformation(int batch_start_idx, int& memory_transfers, std::vector<int>& HBM_vectors_left, std::vector<int>& DIMM_vectors_left, std::vector<std::unordered_map<int, uint64_t>>& pim_transfer_address){
    int total_memory_transfers = 0;
    int total_transfers = 0;
    for(int i=0; i < batch_size; i++)
    {
        int emb_pool_idx = batch_start_idx + i;
        int HBM_vectors = HBM_transaction[emb_pool_idx].size();
        int DIMM_vectors = DIMM_transaction[emb_pool_idx].size();
        
        DIMM_vectors = 0; // eliminate DIMM access
        HBM_vectors_left.push_back(HBM_vectors);
        DIMM_vectors_left.push_back(DIMM_vectors);

        if(is_using_HEAM)
        {
            std::unordered_map<int, uint64_t> transfer_vectors_per_batch;
            ProfileVectorToTransfer(transfer_vectors_per_batch, batch_start_idx, i);
            pim_transfer_address.push_back(transfer_vectors_per_batch);
            total_transfers = GetTotalPIMTransfers(transfer_vectors_per_batch);
        }
        else
            total_transfers = HBM_vectors;

        total_memory_transfers += (total_transfers + DIMM_vectors);
    }
    memory_transfers = total_memory_transfers;
}

void TraceBasedCPUForHeterogeneousMemory::ProfileVectorToTransfer(std::unordered_map<int, uint64_t>& transfer_vectors, int batch_start_idx, int batch_idx) {
    int emb_pool_idx = batch_start_idx + batch_idx;
    for (int i = 0; i < HBM_transaction[emb_pool_idx].size(); i++) {
        uint64_t addr = std::get<0>(HBM_transaction[emb_pool_idx][i]);
        int bg_idx = GetChannel(addr)*bankgroups + GetBankGroup(addr);
        if(is_using_LUT) 
        {
            if(std::get<1>(HBM_transaction[emb_pool_idx][i]) != 'r') // check only q vectors
                transfer_vectors[bg_idx] = addr; // Storing the last address for each bank group
        }
        else
            transfer_vectors[bg_idx] = addr;
    }
}

void TraceBasedCPUForHeterogeneousMemory::ReorderHBMTransaction(int emb_pool_idx)
{
    std::vector<std::tuple<uint64_t, char, int>> reorderedTransaction;
    std::vector<std::tuple<uint64_t, char, int>> workCopy = HBM_transaction[emb_pool_idx];
    
    while (!workCopy.empty()) {
        std::unordered_set<int> uniqueElements;
        std::vector<std::tuple<uint64_t, char, int>> currentBatch;
        
        auto it = std::stable_partition(workCopy.begin(), workCopy.end(),
            [this, &uniqueElements](const std::tuple<uint64_t, char, int>& trans) {
                return uniqueElements.insert(GetChannel(std::get<0>(trans))).second;
            });

        currentBatch.insert(currentBatch.end(), std::make_move_iterator(workCopy.begin()), std::make_move_iterator(it));

        std::sort(currentBatch.begin(), currentBatch.end(),
            [this](const std::tuple<uint64_t, char, int>& a, const std::tuple<uint64_t, char, int>& b) {
                return GetChannel(std::get<0>(a)) < GetChannel(std::get<0>(b));
            });


        workCopy.erase(workCopy.begin(), it);
        reorderedTransaction.insert(reorderedTransaction.end(), std::make_move_iterator(currentBatch.begin()), std::make_move_iterator(currentBatch.end()));
    }
    
    HBM_transaction[emb_pool_idx] = std::move(reorderedTransaction);
}

void TraceBasedCPUForHeterogeneousMemory::AddBatchTransactions(int batch_index, int& batch_tag, std::vector<int>& HBM_vectors_left, std::vector<int>& DIMM_vectors_left, 
                                                                std::vector<std::unordered_map<int, uint64_t>> pim_transfer_address)
{
    if(batch_tag >= batch_size)
        return;

    AddTransactionsToHBM(batch_index, batch_tag, HBM_vectors_left[batch_tag], pim_transfer_address[batch_tag]);
    AddTransactionsToDIMM(batch_index, batch_tag, DIMM_vectors_left[batch_tag]);
    if((HBM_vectors_left[batch_tag] == 0) && (DIMM_vectors_left[batch_tag]==0))
        batch_tag++;
}

void TraceBasedCPUForHeterogeneousMemory::AddTransactionsToHBM(int batch_idx, int batch_tag, int& HBM_vectors_left, std::unordered_map<int, uint64_t> pim_transfer_address)
{
    if(HBM_vectors_left <= 0)
        return;

    int emb_pool_idx = batch_idx+batch_tag;
    int total_trans = HBM_transaction[emb_pool_idx].size();
    ReorderHBMTransaction(emb_pool_idx);
    bool insert_to_all_channel = false;
    int last_channel = -1;

    while(true)
    {
        if(insert_to_all_channel)
            break;

        for(int i=0; i<num_ca_in_cycle; i++)
        {
            int target_vec_idx = total_trans-HBM_vectors_left;
            if(target_vec_idx >= total_trans)
                return;


            uint64_t target_addr = std::get<0>(HBM_transaction[emb_pool_idx][target_vec_idx]);
            char vec_class = std::get<1>(HBM_transaction[emb_pool_idx][target_vec_idx]);
            int subvec_idx = std::get<2>(HBM_transaction[emb_pool_idx][target_vec_idx]);
            HBM_get_next_ = memory_system_HBM.WillAcceptTransaction(target_addr, false);

            int curr_channel = GetChannel(target_addr);
            if(curr_channel < last_channel)
            {
                insert_to_all_channel = true;
                break;
            }
            last_channel = curr_channel;

            if (HBM_get_next_) {
                int bg_idx = GetChannel(target_addr)*bankgroups + GetBankGroup(target_addr);
                bool is_r_vec = is_using_LUT && vec_class == 'r';
                bool is_transfer_vec = !is_r_vec && (pim_transfer_address.at(bg_idx) == target_addr);
                bool is_last_subvec = (subvec_idx == (num_rds-1));
                uint64_t start_addr = target_addr + 64*((num_rds-1) - subvec_idx);

                PimValues pim_values(0, is_transfer_vec, is_r_vec, false, batch_tag, num_rds, is_last_subvec, start_addr);
                memory_system_HBM.AddTransaction(target_addr, false, pim_values);
                if(is_using_HEAM)
                {
                    if(is_transfer_vec)
                        HBM_address_in_processing.push_back(target_addr);
                }
                else
                    HBM_address_in_processing.push_back(target_addr);

                loads_per_bg[bg_idx] += 1;
                if(!is_r_vec)
                    loads_per_bg_for_q[bg_idx] += 1;

                HBM_vectors_left--;

                ///////// DEBUG /////////
                // std::cout << "batch num  " << batch_idx << " batch : " << batch_tag << " vectors left : "<<  HBM_vectors_left << std::endl;
                // if(is_transfer_vec)
                //     std::cout << " \t addr : " << HBM_transaction[batch_idx+batch_tag][target_vec_idx] << " / r_vec : "  << is_r_vec << " / vector transfer : " <<  is_transfer_vec <<  " batch tag : " << batch_tag << std::endl;

                // std::cout << "----- processing on HEAM -----" << std::endl;
                // std::cout << "\t hbm vecs left : " << HBM_vectors_left<< std::endl;
                // std::cout << "\t addr : " << HBM_transaction[batch_idx+batch_tag][target_vec_idx] << " / r_vec : "  << is_r_vec << " / vector transfer : " <<  is_transfer_vec <<  std::endl;
                // std::cout << "\t channel, bg : " << GetChannel(target_addr) << " " << GetBankGroup(target_addr) << std::endl; 
                // std::cout << "------------------------------\n" << std::endl;

            }
        }
    }

}

void TraceBasedCPUForHeterogeneousMemory::AddTransactionsToDIMM(int batch_idx, int batch_tag, int& DIMM_vectors_left)
{
    if(DIMM_vectors_left <= 0)
        return;

    uint64_t target_addr = std::get<0>(DIMM_transaction[batch_idx+batch_tag][DIMM_vectors_left-1]);
    DIMM_get_next_ = memory_system_DIMM.WillAcceptTransaction(target_addr, false);
    if (DIMM_get_next_) {
        PimValues pim_values;
        // std::cout << "DIMM in cycle : " << clk_DIMM << std::endl;
        memory_system_DIMM.AddTransaction(target_addr, false, pim_values);
        DIMM_address_in_processing.push_back(target_addr);
        DIMM_vectors_left--;
    }
}

void TraceBasedCPUForHeterogeneousMemory::ReadCallBack_HBM(uint64_t addr)
{
    bool transaction_complete = UpdateInProcessTransactionList(addr, HBM_address_in_processing, true);
    if(transaction_complete)
        complete_transactions++;
    HBM_complete_addr = addr;
}

void TraceBasedCPUForHeterogeneousMemory::ReadCallBack_DIMM(uint64_t addr)
{
    bool transaction_complete = UpdateInProcessTransactionList(addr, DIMM_address_in_processing, false);
    if(transaction_complete)
        complete_transactions++;
    DIMM_complete_addr = addr;
}

bool TraceBasedCPUForHeterogeneousMemory::UpdateInProcessTransactionList(uint64_t addr, std::list<uint64_t>& transactionlist, bool hbm)
{
    bool address_found=false;
    for (std::list<uint64_t>::iterator i=transactionlist.begin(); i!=transactionlist.end(); i++)
    {
       if(*i == addr)
       {
            address_found=true;
            break;
       }
    }

    // this logic considers duplicate address case
    if(address_found)
    {
        int original_length = transactionlist.size();
        transactionlist.remove(addr);
        int changed_length = transactionlist.size();
        int repush = original_length - changed_length - 1;
        for(int i=0; i<repush; i++)
            transactionlist.push_back(addr);

        return true;
    }
    else
        return false;
}

void TraceBasedCPUForHeterogeneousMemory::ClockTick(){
    memory_system_HBM.ClockTick();
    // calculated considering clock frequency difference
    if(clk_HBM %4 == 0)
    {
        memory_system_DIMM.ClockTick();
        memory_system_DIMM.ClockTick();
        memory_system_DIMM.ClockTick();
        clk_DIMM = clk_DIMM + 3;
    }    
    clk_HBM++;
}

int TraceBasedCPUForHeterogeneousMemory::GetBankGroup(uint64_t address) {
    Address addr = hbm_config->AddressMapping(address);
    return addr.bankgroup;
}

int TraceBasedCPUForHeterogeneousMemory::GetChannel(uint64_t address) {
    Address addr = hbm_config->AddressMapping(address);
    return addr.channel;
}

int TraceBasedCPUForHeterogeneousMemory::GetTotalPIMTransfers(std::unordered_map<int, uint64_t> lastAddressInBankGroup)
{
    int total_pim_transfers = 0;
    for(int i=0; i < channels*bankgroups; i++)
    {
        if(lastAddressInBankGroup[i] != 0)
            total_pim_transfers++;
    }

    return total_pim_transfers;
}

void TraceBasedCPUForHeterogeneousMemory::LoadTrace(string filename)
{
    std::string line, str_tmp;
    std::ifstream file(filename);
    std::stringstream ss;
    int count = 0;

    std::vector<std::tuple<uint64_t, char, int>> HBM_pooling;
    std::vector<std::tuple<uint64_t, char, int>> DIMM_pooling;
    HBM_transaction.push_back(HBM_pooling);
    DIMM_transaction.push_back(DIMM_pooling);
    HBM_transaction[count].clear();
    DIMM_transaction[count].clear();

    std::cout << "Using following trace file : " << filename << std::endl;

    bool is_collision = false;
    // bool last_was_DIMM = false;

    if(filename.find("col_"))
        is_collision = true;

    if(file.is_open())
    {
        while (std::getline(file, line, '\n'))
        {
            if(line.empty())
            {
                count++;
                std::vector<std::tuple<uint64_t, char, int>> HBM_pooling;
                std::vector<std::tuple<uint64_t, char, int>> DIMM_pooling;
                HBM_transaction.push_back(HBM_pooling);
                DIMM_transaction.push_back(DIMM_pooling);
                HBM_transaction[count].clear();
                DIMM_transaction[count].clear();
                continue;
            }

            ss.str(line);
            std::string mode = "DIMM";
            char vec_class = 'o';
            int subvec_idx = 0;
            uint64_t addr = 0;
            int str_count = 0;

            while (ss >> str_tmp)
            {
                if(str_count == 0)
                {
                    if(str_tmp == "\n")
                        count++;
                    else
                        mode = str_tmp;
                }
                else if(str_count == 1)
                    addr = std::stoull(str_tmp);
                else if(str_count == 2)
                    vec_class = str_tmp[0];
                else if(str_count == 3)
                    subvec_idx = std::stoi(str_tmp);
                str_count++;
            }

            if(mode == "HBM")
            {
                if(CA_compression)
                {
                    if(subvec_idx == (num_rds-1))
                        HBM_transaction[count].push_back(std::make_tuple(addr, vec_class, subvec_idx));
                }
                else
                    HBM_transaction[count].push_back(std::make_tuple(addr, vec_class, subvec_idx));
            }
            else
                DIMM_transaction[count].push_back(std::make_tuple(addr, vec_class, subvec_idx));

            ss.clear();
        }
    }

    // std::cout << "Done loading trace file" << std::endl;

}

}  // namespace dramsim3
