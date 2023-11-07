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
    memory_system_PIM(config_file_HBM, output_dir,
        std::bind(&TraceBasedCPUForHeterogeneousMemory::ReadCallBack_PIMMem, this, std::placeholders::_1),
        std::bind(&TraceBasedCPUForHeterogeneousMemory::WriteCallBack_PIMMem, this, std::placeholders::_1)),
    memory_system_Mem(config_file_DIMM, output_dir,
            std::bind(&TraceBasedCPUForHeterogeneousMemory::ReadCallBack_Mem, this, std::placeholders::_1),
            std::bind(&TraceBasedCPUForHeterogeneousMemory::WriteCallBack_Mem, this, std::placeholders::_1)),
    clk_PIM(0),
    clk_Mem(0),
    PIMMem_complete_addr(-1),
    Mem_complete_addr(-1),
    num_rds(1),
    num_ca_in_cycle(1)
{
    PIMMem_config = memory_system_PIM.config_copy;
    Mem_config = memory_system_Mem.config_copy;

    channels = PIMMem_config->channels;
    bankgroups = PIMMem_config->bankgroups;
    is_using_HEAM = PIMMem_config->PIM_enabled;
    is_using_LUT = PIMMem_config->LUT_enabled;
    batch_size = PIMMem_config->batch_size;
    addrmapping = PIMMem_config->address_mapping;
    if(!is_using_HEAM)
        batch_size=1;

    for(int i=0; i < channels*bankgroups; i++)
    {
        loads_per_bg.push_back(0);
        loads_per_bg_for_q.push_back(0);
    }

    CA_compression = PIMMem_config->CA_compression;
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
    // int total_cycle = RunTRiM();
    // int total_cycle = RunSPACE();

    std::cout << "- total cycle : " << total_cycle << std::endl;
    std::cout << "-----------------------------" << std::endl;
}


int TraceBasedCPUForHeterogeneousMemory::RunTRiM() {
    NMP TRiM_nmp = NMP(add_cycle);
    int batch_start_index = 0;
    int batch_tag = 0;
    int total_batch = (int)PIMMem_transaction.size()/batch_size;
    std::vector<int> PIMMem_vectors_left;
    std::vector<std::unordered_map<int, uint64_t>> pim_transfer_address;

    for(int i=0; i < total_batch-1; i++)
    {
        batch_start_index = i*batch_size;
        batch_tag = 0;
        pim_transfer_address.clear();
        std::vector<int>* Mem_vectors_left = NULL;
        int total_transfers = UpdateBatchInfo(batch_start_index, PIMMem_vectors_left, pim_transfer_address);
        TRiM_nmp.SetTotalTransfers(total_transfers);
        while(TRiM_nmp.CheckNMPDone())
        {
            ClockTick();
            AddBatchTransactions(batch_start_index, batch_tag, PIMMem_vectors_left, pim_transfer_address);
            bool transaction_processed = TRiM_nmp.RunNMPLogic(complete_transactions);
        }
        std::cout << i << " of " << total_batch << " processed" << std::endl;
    }
    return clk_PIM;
}

int TraceBasedCPUForHeterogeneousMemory::RunSPACE() {
    NMP SPACE_nmp = NMP(add_cycle);
    int batch_start_index = 0;
    int batch_tag = 0;
    int total_batch = (int)PIMMem_transaction.size()/batch_size;
    std::vector<int> PIMMem_vectors_left;
    std::vector<int> Mem_vectors_left;
    std::vector<std::unordered_map<int, uint64_t>> pim_transfer_address;

    for(int i=0; i < total_batch-1; i++)
    {
        batch_start_index = i*batch_size;
        batch_tag = 0;
        PIMMem_vectors_left.clear();
        Mem_vectors_left.clear();
        pim_transfer_address.clear();
        int total_transfers = UpdateBatchInfoForHetero(batch_start_index, PIMMem_vectors_left, Mem_vectors_left, pim_transfer_address);

        SPACE_nmp.SetTotalTransfers(total_transfers);
        while(SPACE_nmp.CheckNMPDone())
        {
            ClockTick();
            AddBatchTransactionsToHetero(batch_start_index, batch_tag, PIMMem_vectors_left, Mem_vectors_left, pim_transfer_address);
            bool transaction_processed = SPACE_nmp.RunNMPLogic(complete_transactions);
            if(transaction_processed)
                complete_transactions = 0;

        }
        std::cout << i << " of " << total_batch << " processed" << std::endl;
    }

    return clk_PIM;
}

int TraceBasedCPUForHeterogeneousMemory::RunHEAM() {

    NMP HEAM_nmp = NMP(add_cycle);
    int batch_start_index = 0;
    int batch_tag = 0;
    int total_batch = (int)PIMMem_transaction.size()/batch_size;
    std::vector<int> PIMMem_vectors_left;
    std::vector<int> Mem_vectors_left;
    std::vector<std::unordered_map<int, uint64_t>> pim_transfer_address;

    for(int i=0; i < total_batch-1; i++)
    {
        batch_start_index = i*batch_size;
        batch_tag = 0;
        PIMMem_vectors_left.clear();
        Mem_vectors_left.clear();
        pim_transfer_address.clear();
        int total_transfers = UpdateBatchInfoForHetero(batch_start_index, PIMMem_vectors_left, Mem_vectors_left, pim_transfer_address);
        HEAM_nmp.SetTotalTransfers(total_transfers);
        while(HEAM_nmp.CheckNMPDone())
        {
            ClockTick();
            AddBatchTransactionsToHetero(batch_start_index, batch_tag, PIMMem_vectors_left, Mem_vectors_left, pim_transfer_address);
            bool transaction_processed = HEAM_nmp.RunNMPLogic(complete_transactions);
            if(transaction_processed)
                complete_transactions = 0;
        }
        std::cout << i << " of " << total_batch << " processed" << std::endl;
    }
    return clk_PIM;
}

int TraceBasedCPUForHeterogeneousMemory::UpdateBatchInfoForHetero(int batch_start_idx, std::vector<int>& PIMMem_vectors_left, std::vector<int>& Mem_vectors_left, std::vector<std::unordered_map<int, uint64_t>>& pim_transfer_address){
    int total_memory_transfers = 0;
    int total_transfers = 0;

    for(int i=0; i < batch_size; i++)
    {
        int emb_pool_idx = batch_start_idx + i;
        int PIMMem_vectors = PIMMem_transaction[emb_pool_idx].size();     
        int Mem_vectors = Mem_transaction[emb_pool_idx].size();
        PIMMem_vectors_left.push_back(PIMMem_vectors);
        Mem_vectors_left.push_back(Mem_vectors);

        if(is_using_HEAM)
        {
            std::unordered_map<int, uint64_t> transfer_vectors_per_batch;
            ProfileVectorToTransfer(transfer_vectors_per_batch, batch_start_idx, i);
            pim_transfer_address.push_back(transfer_vectors_per_batch);
            int total_pim_transfers = 0;
            for(int i=0; i < channels*bankgroups; i++)
            {
                if(transfer_vectors_per_batch[i] != 0)
                    total_pim_transfers++;
            }
            total_transfers = total_pim_transfers;
        }
        else
        {
            std::unordered_map<int, uint64_t> transfer_vectors_per_batch;
            pim_transfer_address.push_back(transfer_vectors_per_batch);
            total_transfers = PIMMem_vectors;
        }

        total_memory_transfers += (total_transfers + Mem_vectors);
    }
    total_memory_transfers;

    return total_memory_transfers;
}

int TraceBasedCPUForHeterogeneousMemory::UpdateBatchInfo(int batch_start_idx, std::vector<int>& PIMMem_vectors_left, std::vector<std::unordered_map<int, uint64_t>>& pim_transfer_address){
    int total_memory_transfers = 0;
    int total_transfers = 0;

    for(int i=0; i < batch_size; i++)
    {
        int emb_pool_idx = batch_start_idx + i;
        int PIMMem_vectors = PIMMem_transaction[emb_pool_idx].size();     
        PIMMem_vectors_left.push_back(PIMMem_vectors);
        std::unordered_map<int, uint64_t> transfer_vectors_per_batch;
        ProfileVectorToTransfer(transfer_vectors_per_batch, batch_start_idx, i);
        pim_transfer_address.push_back(transfer_vectors_per_batch);

        int total_pim_transfers = 0;
        for(int i=0; i < channels*bankgroups; i++)
        {
            if(transfer_vectors_per_batch[i] != 0)
                total_pim_transfers++;
        }

        total_memory_transfers += total_pim_transfers;
    }

    return total_memory_transfers;
}

void TraceBasedCPUForHeterogeneousMemory::ProfileVectorToTransfer(std::unordered_map<int, uint64_t>& transfer_vectors, int batch_start_idx, int batch_idx) {
    int emb_pool_idx = batch_start_idx + batch_idx;
    for (int i = 0; i < PIMMem_transaction[emb_pool_idx].size(); i++) {
        uint64_t addr = std::get<0>(PIMMem_transaction[emb_pool_idx][i]);
        int bg_idx = PIMMemGetChannel(addr)*bankgroups + PIMMemGetBankGroup(addr);
        if(is_using_LUT) 
        {
            if(std::get<1>(PIMMem_transaction[emb_pool_idx][i]) != 'r') // check only q vectors
                transfer_vectors[bg_idx] = addr; // Storing the last address for each bank group
        }
        else
            transfer_vectors[bg_idx] = addr;
    }
}

void TraceBasedCPUForHeterogeneousMemory::AddBatchTransactionsToHetero(int batch_index, int& batch_tag, std::vector<int>& PIMMem_vectors_left, std::vector<int>& Mem_vectors_left, 
                                                                std::vector<std::unordered_map<int, uint64_t>> pim_transfer_address)
{
    if(batch_tag >= batch_size)
        return;

    AddTransactionsToPIMMem(batch_index, batch_tag, PIMMem_vectors_left[batch_tag], pim_transfer_address[batch_tag]);
    AddTransactionsToMemory(batch_index, batch_tag, Mem_vectors_left[batch_tag]);
    if((PIMMem_vectors_left[batch_tag] == 0) && (Mem_vectors_left[batch_tag]==0))
        batch_tag++;
}

void TraceBasedCPUForHeterogeneousMemory::AddBatchTransactions(int batch_index, int& batch_tag, std::vector<int>& PIMMem_vectors_left, std::vector<std::unordered_map<int, uint64_t>> pim_transfer_address)
{
    if(batch_tag >= batch_size)
        return;

    AddTransactionsToPIMMem(batch_index, batch_tag, PIMMem_vectors_left[batch_tag], pim_transfer_address[batch_tag]);
    if(PIMMem_vectors_left[batch_tag] == 0)
        batch_tag++;
}

void TraceBasedCPUForHeterogeneousMemory::AddTransactionsToPIMMem(int batch_idx, int batch_tag, int& PIM_vectors_left, std::unordered_map<int, uint64_t> pim_transfer_address)
{
    if(PIM_vectors_left <= 0)
        return;

    int emb_pool_idx = batch_idx+batch_tag;
    int total_trans = PIMMem_transaction[emb_pool_idx].size();
    // ReorderHBMTransaction(emb_pool_idx);
    bool insert_to_all_channel = false;
    int last_channel = -1;
    std::vector<int> trans_per_channel;
    for(int i=0; i<PIMMem_config->channels; i++)
    {
        trans_per_channel.push_back(0);
    }
    // int count = 0;
    while(true)
    {
        int target_vec_idx = total_trans-PIM_vectors_left;
        if(target_vec_idx >= total_trans)
            return;

        // count++;
        // if(count > num_ca_in_cycle)
        //     break;
        uint64_t target_addr = std::get<0>(PIMMem_transaction[emb_pool_idx][target_vec_idx]);
        char vec_class = std::get<1>(PIMMem_transaction[emb_pool_idx][target_vec_idx]);
        int subvec_idx = std::get<2>(PIMMem_transaction[emb_pool_idx][target_vec_idx]);
        bool PIM_mem_get_next_ = memory_system_PIM.WillAcceptTransaction(target_addr, false);

        int curr_channel = PIMMemGetChannel(target_addr);
        if(PIM_mem_get_next_)
            trans_per_channel[curr_channel] += 1;

        for(auto it = trans_per_channel.begin(); it != trans_per_channel.end(); it++)
        {
            if(*it > num_ca_in_cycle)
            {
                insert_to_all_channel = true;
                break;
            }                 
        }

        if(insert_to_all_channel)
            break;

        if (PIM_mem_get_next_) {
            int bg_idx = PIMMemGetChannel(target_addr)*bankgroups + PIMMemGetBankGroup(target_addr);
            bool is_r_vec = is_using_LUT && vec_class == 'r';
            bool is_transfer_vec = (is_using_HEAM || is_using_TRiM) && !is_r_vec && (pim_transfer_address.at(bg_idx) == target_addr);
            bool is_last_subvec = (subvec_idx == (num_rds-1));
            uint64_t start_addr = target_addr + 64*((num_rds-1) - subvec_idx);

            PimValues pim_values(0, is_transfer_vec, is_r_vec, false, batch_tag, num_rds, is_last_subvec, start_addr);

            if(is_using_HEAM)
            {
                if(is_transfer_vec)
                    PIMMem_address_in_processing.push_back(target_addr);
            }
            else if(is_using_TRiM)
            {
                 if(vec_class == 'h')
                    target_addr = HotEntryReplication(target_addr);   
            }
            else
                PIMMem_address_in_processing.push_back(target_addr);

            memory_system_PIM.AddTransaction(target_addr, false, pim_values);
            loads_per_bg[bg_idx] += 1;
            if(!is_r_vec)
                loads_per_bg_for_q[bg_idx] += 1;

            PIM_vectors_left--;
        }
    }
}

void TraceBasedCPUForHeterogeneousMemory::AddTransactionsToMemory(int batch_idx, int batch_tag, int& Mem_vectors_left)
{
    if(Mem_vectors_left <= 0)
        return;

    bool insert_to_all_channel = false;
    int last_channel = -1;
    std::vector<int> trans_per_channel;
    for(int i=0; i<Mem_config->channels; i++)
    {
        trans_per_channel.push_back(0);
    }

    // while(true)
    // {
        // if(Mem_vectors_left <= 0)
        //     return;

        uint64_t target_addr = std::get<0>(Mem_transaction[batch_idx+batch_tag][Mem_vectors_left-1]);
        bool Mem_get_next_ = memory_system_Mem.WillAcceptTransaction(target_addr, false);
        int curr_channel = MemGetChannel(target_addr);

        // if(Mem_get_next_)
        //     trans_per_channel[curr_channel] += 1;

        // for(auto it = trans_per_channel.begin(); it != trans_per_channel.end(); it++)
        // {
        //     if(*it > 1)
        //     {
        //         insert_to_all_channel = true;
        //         break;
        //     }                 
        // }

        // if(insert_to_all_channel)
        //     break;

        if (Mem_get_next_) {
            PimValues pim_values;
            memory_system_Mem.AddTransaction(target_addr, false, pim_values);
            Mem_address_in_processing.push_back(target_addr);
            Mem_vectors_left--;
            // std::cout << "mem vectors left : " << Mem_vectors_left << std::endl;
        }
    // }
}

void TraceBasedCPUForHeterogeneousMemory::ReadCallBack_PIMMem(uint64_t addr)
{
    bool transaction_complete = UpdateInProcessTransactionList(addr, PIMMem_address_in_processing, true);
    if(transaction_complete)
        complete_transactions++;
    PIMMem_complete_addr = addr;
}

void TraceBasedCPUForHeterogeneousMemory::ReadCallBack_Mem(uint64_t addr)
{
    bool transaction_complete = UpdateInProcessTransactionList(addr, Mem_address_in_processing, false);
    if(transaction_complete)
        complete_transactions++;
    Mem_complete_addr = addr;
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

    // this logic considers duplicate input address case
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

uint64_t TraceBasedCPUForHeterogeneousMemory::HotEntryReplication(uint64_t target_addr)
{
    int minimal_load = loads_per_bg[0];
    int minimal_load_bg_idx = 0;
    for(int i=1; i<bankgroups; i++)
    {
        if(loads_per_bg[i] < minimal_load)
            minimal_load = loads_per_bg[i];
        minimal_load_bg_idx = i;
    }
    int ch_idx = minimal_load_bg_idx / channels;
    int bg_idx = minimal_load_bg_idx % channels;
    
    Address addr = PIMMem_config->AddressMapping(target_addr);
    uint64_t redirected_addr = PIMMem_config->GenerateAddress(ch_idx, addr.rank, bg_idx, addr.bank, addr.row, addr.column);
    return redirected_addr;
}

void TraceBasedCPUForHeterogeneousMemory::ReorderHBMTransaction(int emb_pool_idx)
{
    std::vector<std::tuple<uint64_t, char, int>> reorderedTransaction;
    std::vector<std::tuple<uint64_t, char, int>> workCopy = PIMMem_transaction[emb_pool_idx];
    
    while (!workCopy.empty()) {
        std::unordered_set<int> uniqueElements;
        std::vector<std::tuple<uint64_t, char, int>> currentBatch;
        
        auto it = std::stable_partition(workCopy.begin(), workCopy.end(),
            [this, &uniqueElements](const std::tuple<uint64_t, char, int>& trans) {
                return uniqueElements.insert(PIMMemGetChannel(std::get<0>(trans))).second;
            });

        currentBatch.insert(currentBatch.end(), std::make_move_iterator(workCopy.begin()), std::make_move_iterator(it));

        std::sort(currentBatch.begin(), currentBatch.end(),
            [this](const std::tuple<uint64_t, char, int>& a, const std::tuple<uint64_t, char, int>& b) {
                return PIMMemGetChannel(std::get<0>(a)) < PIMMemGetChannel(std::get<0>(b));
            });


        workCopy.erase(workCopy.begin(), it);
        reorderedTransaction.insert(reorderedTransaction.end(), std::make_move_iterator(currentBatch.begin()), std::make_move_iterator(currentBatch.end()));
    }
    
    PIMMem_transaction[emb_pool_idx] = std::move(reorderedTransaction);
}

void TraceBasedCPUForHeterogeneousMemory::ClockTick(){
    memory_system_PIM.ClockTick();
    // calculated considering clock frequency difference
    if(clk_PIM %4 == 0)
    {
        memory_system_Mem.ClockTick();
        memory_system_Mem.ClockTick();
        memory_system_Mem.ClockTick();
        clk_Mem = clk_Mem + 3;
    }    
    clk_PIM++;
}

int TraceBasedCPUForHeterogeneousMemory::PIMMemGetBankGroup(uint64_t address) {
    Address addr = PIMMem_config->AddressMapping(address);
    return addr.bankgroup;
}

int TraceBasedCPUForHeterogeneousMemory::PIMMemGetChannel(uint64_t address) {
    Address addr = PIMMem_config->AddressMapping(address);
    return addr.channel;
}

int TraceBasedCPUForHeterogeneousMemory::MemGetBankGroup(uint64_t address) {
    Address addr = Mem_config->AddressMapping(address);
    return addr.bankgroup;
}

int TraceBasedCPUForHeterogeneousMemory::MemGetChannel(uint64_t address) {
    Address addr = Mem_config->AddressMapping(address);
    return addr.channel;
}

void TraceBasedCPUForHeterogeneousMemory::LoadTrace(string filename)
{
    std::string line, str_tmp;
    std::ifstream file(filename);
    std::stringstream ss;
    int count = 0;

    std::vector<std::tuple<uint64_t, char, int>> HBM_pooling;
    std::vector<std::tuple<uint64_t, char, int>> DIMM_pooling;
    PIMMem_transaction.push_back(HBM_pooling);
    Mem_transaction.push_back(DIMM_pooling);
    PIMMem_transaction[count].clear();
    Mem_transaction[count].clear();

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
                PIMMem_transaction.push_back(HBM_pooling);
                Mem_transaction.push_back(DIMM_pooling);
                PIMMem_transaction[count].clear();
                Mem_transaction[count].clear();
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
                        PIMMem_transaction[count].push_back(std::make_tuple(addr, vec_class, subvec_idx));
                }
                else
                    PIMMem_transaction[count].push_back(std::make_tuple(addr, vec_class, subvec_idx));
            }
            else
                Mem_transaction[count].push_back(std::make_tuple(addr, vec_class, subvec_idx));

            ss.clear();
        }
    }

    std::cout << "Done loading trace file" << std::endl;

}

}  // namespace dramsim3
