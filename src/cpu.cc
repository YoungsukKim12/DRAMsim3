#include "cpu.h"
#include <sstream>

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
        memory_system_.AddTransaction(last_addr_, last_write_, false, false);
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

    if (!inserted_a_ &&
        memory_system_.WillAcceptTransaction(addr_a_ + offset_, false)) {
        memory_system_.AddTransaction(addr_a_ + offset_, false, false, false);
        inserted_a_ = true;
    }
    if (!inserted_b_ &&
        memory_system_.WillAcceptTransaction(addr_b_ + offset_, false)) {
        memory_system_.AddTransaction(addr_b_ + offset_, false, false, false);
        inserted_b_ = true;
    }
    if (!inserted_c_ &&
        memory_system_.WillAcceptTransaction(addr_c_ + offset_, true)) {
        memory_system_.AddTransaction(addr_c_ + offset_, true, false, false);
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
                memory_system_.AddTransaction(trans_.addr, trans_.is_write, false, false);
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
            std::bind(&TraceBasedCPUForHeterogeneousMemory::WriteCallBack_DIMM, this, std::placeholders::_1))
{
    clk_HBM = 0;
    clk_DIMM = 0;
    HBM_complete_addr = -1;
    DIMM_complete_addr = -1;

    LoadTrace(trace_file);

    is_using_HEAM = true;

    if(is_using_HEAM)
        RunHEAM();
    else
        RunNMP();
    std::cout << clk_HBM << std::endl;
}

int TraceBasedCPUForHeterogeneousMemory::GetBankGroup(uint64_t address) {
  return (address >> 4) & 0x3; // Extracting the 4th and 5th bits
}

int TraceBasedCPUForHeterogeneousMemory::GetChannel(uint64_t address) {
    return (address >> 8) & 0x3; // Extracting the 8th and 9th bits
}

void TraceBasedCPUForHeterogeneousMemory::HeterogeneousMemoryClockTick(){
    // clock tick
    memory_system_HBM.ClockTick();
    if(clk_HBM %2 == 0)
    {
        memory_system_DIMM.ClockTick();
        clk_DIMM++;
    }    
    clk_HBM++;
}

void TraceBasedCPUForHeterogeneousMemory::RunNMP() {

    // main function of ClockTick() : run NMP

    for(int i=0; i< (int)HBM_transaction.size() - 1; i++)
    {

        std::cout << i << "/" << HBM_transaction.size() << " processed" << std::endl;

        int pooling_count = HBM_transaction[i].size() + DIMM_transaction[i].size();
        int HBM_vectors_left = HBM_transaction[i].size();
        int DIMM_vectors_left = DIMM_transaction[i].size();

        // std::cout << "total transactions : " << pooling_count << std::endl;
        bool nmp_is_calculating = false;
        int nmp_cycle_left = add_cycle;
        int nmp_buffer_queue = 0;
        // hbm_complete_count = 0;
        // dimm_complete_count = 0;

        // dummy
        std::unordered_map<int, uint64_t> vector_transfer_address = ProfileAddresses(HBM_transaction[i]);
        AddTransactionsToMemory(HBM_transaction[i], DIMM_transaction[i], HBM_vectors_left, DIMM_vectors_left, vector_transfer_address);

        while(pooling_count > 0 || nmp_buffer_queue > 0 || nmp_cycle_left > 0)
        {
            // processing nmp operation
            if(nmp_is_calculating)
            {
                nmp_cycle_left--;
                if(nmp_cycle_left == 0)
                {
                    nmp_is_calculating = false;
                    nmp_cycle_left = 0;
                }
                // std::cout << nmp_cycle_left << std::endl;
            }
            // start nmp add operation
            if(nmp_buffer_queue > 0 && !nmp_is_calculating)
            {
                nmp_buffer_queue--;
                nmp_is_calculating = true;
                nmp_cycle_left = add_cycle;
            }

            HeterogeneousMemoryClockTick();

            // ReadCallback() increments complete_transactions when rd is complete
            if(complete_transactions > 0)
            {
                // put data to nmp buffer queue
                if(nmp_is_calculating)
                    nmp_buffer_queue += complete_transactions;
                else
                {
                    nmp_is_calculating = true;
                    nmp_cycle_left = add_cycle;
                    if(complete_transactions > 1)
                        nmp_buffer_queue += (complete_transactions-1);
                }
                pooling_count -= complete_transactions;
                complete_transactions = 0;
            }                
            
            // std::cout << pooling_count << " " << nmp_buffer_queue << " " << nmp_cycle_left << std::endl;

        }
                // std::cout << i << "/" << HBM_transaction.size() << " process finish" << std::endl;

    }
    std::cout << clk_HBM << std::endl;

        //TODO : Debug HBM transaction vanishing

    return;
}

void TraceBasedCPUForHeterogeneousMemory::RunHEAM() {

    for(int i=0; i < (int)HBM_transaction.size() - 1; i++)
    {
        if(i%100 == 0)
            std::cout << i << "/" << HBM_transaction.size() << " processed" << std::endl;


        int HBM_vectors_left = HBM_transaction[i].size();
        int DIMM_vectors_left = DIMM_transaction[i].size();
        std::unordered_map<int, uint64_t> vector_transfer_address = ProfileAddresses(HBM_transaction[i]);
        int total_pim_transfers = GetTotalPIMTransfers(vector_transfer_address);
        int pooling_count = DIMM_vectors_left + total_pim_transfers;

        // std::cout << "total transactions : " << pooling_count << std::endl;
        bool nmp_is_calculating = false;
        int nmp_cycle_left = add_cycle;
        int nmp_buffer_queue = 0;


        while(pooling_count > 0 || nmp_buffer_queue > 0 || nmp_cycle_left > 0)
        {
            // processing nmp operation
            if(nmp_is_calculating)
            {
                nmp_cycle_left--;
                if(nmp_cycle_left == 0)
                {
                    nmp_is_calculating = false;
                    nmp_cycle_left = 0;
                }
                // std::cout << nmp_cycle_left << std::endl;
            }
            // start nmp add operation
            if(nmp_buffer_queue > 0 && !nmp_is_calculating)
            {
                nmp_buffer_queue--;
                nmp_is_calculating = true;
                nmp_cycle_left = add_cycle;
            }

            HeterogeneousMemoryClockTick();
            AddTransactionsToMemory(HBM_transaction[i], DIMM_transaction[i], HBM_vectors_left, DIMM_vectors_left, vector_transfer_address);

            // ReadCallback() increments complete_transactions when rd is complete
            if(complete_transactions > 0)
            {
                // put data to nmp buffer queue
                if(nmp_is_calculating)
                    nmp_buffer_queue += complete_transactions;
                else
                {
                    nmp_is_calculating = true;
                    nmp_cycle_left = add_cycle;
                    if(complete_transactions > 1)
                        nmp_buffer_queue += (complete_transactions-1);
                }
                pooling_count -= complete_transactions;
                complete_transactions = 0;
            }                
            
            // std::cout << pooling_count << " " << nmp_buffer_queue << " " << nmp_cycle_left << std::endl;

        }
            // std::cout << i << "/" << HBM_transaction.size() << " process finish" << std::endl;

    }
    std::cout << clk_HBM << std::endl;

        //TODO : Debug HBM transaction vanishing

    return;
}

// TODO : split function for NMP / HEAM
void TraceBasedCPUForHeterogeneousMemory::AddTransactionsToMemory(std::vector<uint64_t> HBM_transaction, std::vector<uint64_t> DIMM_transaction, int &HBM_vectors_left, int &DIMM_vectors_left, std::unordered_map<int, uint64_t> vector_transfer_address){
    // add transaction to HBM
    if(!(HBM_vectors_left <= 0))
    {
        int total_tras = HBM_transaction.size();
        int curr_idx = total_tras-HBM_vectors_left;
        HBM_get_next_ = memory_system_HBM.WillAcceptTransaction(HBM_transaction[curr_idx], false);
        if (HBM_get_next_) {
            bool vector_transfer = false;
            bool is_r_vec = false;

            if(IsLastAddressInBankGroup(vector_transfer_address, HBM_transaction[curr_idx]))
               vector_transfer = true;
 
            // check for even number of HBM vector
            if(is_using_HEAM && HBM_vectors_left%2 == 1)
                is_r_vec = true;

            // std::cout << "----- processing on HEAM -----" << std::endl;
            // std::cout << "\t hbm vecs left : " << HBM_vectors_left<< std::endl;
            // std::cout << " \t addr : " << HBM_transaction[curr_idx] << " / r_vec : "  << is_r_vec << " / vector transfer : " <<  vector_transfer <<  std::endl;
            // std::cout << "------------------------------\n" << std::endl;

            memory_system_HBM.AddTransaction(HBM_transaction[curr_idx], false, vector_transfer, is_r_vec);
            if(!is_using_HEAM)
                HBM_address_in_processing.push_back(HBM_transaction[curr_idx]);
            else
            {
                if(vector_transfer)
                {
                    HBM_address_in_processing.push_back(HBM_transaction[curr_idx]);
                    vec_transfers++;
                    // std::cout << "transfer vec addr in emb op : " << HBM_transaction[curr_idx] << std::endl;
                }
            }
                
            HBM_vectors_left--;
            // std:: cout << "HBM vectors left : " << HBM_vectors_left << std::endl;
        }
    }

    // std::cout << "vec transfers : " << vec_transfers << std::endl;

    // add transaction to DIMM
    if(!(DIMM_vectors_left <= 0))
    {
        DIMM_get_next_ = memory_system_DIMM.WillAcceptTransaction(DIMM_transaction[DIMM_vectors_left-1], false);
        if (DIMM_get_next_) {
            memory_system_DIMM.AddTransaction(DIMM_transaction[DIMM_vectors_left-1], false, false, false);
            DIMM_address_in_processing.push_back(DIMM_transaction[DIMM_vectors_left-1]);
            DIMM_vectors_left--;
            //  std:: cout << "DIMM vectors left : " << DIMM_vectors_left << std::endl;
        }
    }
}

std::unordered_map<int, uint64_t> TraceBasedCPUForHeterogeneousMemory::ProfileAddresses(const std::vector<uint64_t>& addresses) {
    std::unordered_map<int, uint64_t> lastAddressInBankGroup;

    for(int i=0; i < channels*bankgroups; i++)
    {
        lastAddressInBankGroup[i] = 0;
    }

    int idx = 0;
    for (const uint64_t& address : addresses) {
        int channel = GetChannel(address);
        int bankGroup = GetBankGroup(address);
        if(idx%2 == 0)
            lastAddressInBankGroup[channel*bankgroups + bankGroup] = address; // Storing the last address for each bank group
        idx++;
    }

  return lastAddressInBankGroup;
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

bool TraceBasedCPUForHeterogeneousMemory::IsLastAddressInBankGroup(const std::unordered_map<int, uint64_t>& lastAddressInBankGroup, uint64_t address) {
    int channel = GetChannel(address);
    int bankGroup = GetBankGroup(address);
    return lastAddressInBankGroup.at(channel*bankgroups + bankGroup) == address;
}

void TraceBasedCPUForHeterogeneousMemory::ClockTick(){

}

void TraceBasedCPUForHeterogeneousMemory::ReadCallBack_HBM(uint64_t addr)
{
    bool transaction_complete = UpdateInProcessTransactionList(addr, HBM_address_in_processing, true);
    if(transaction_complete)
        complete_transactions++;
    HBM_complete_addr = addr;
    // std::cout << "hbm complete : " << ++hbm_complete_count << std::endl;
    
}

void TraceBasedCPUForHeterogeneousMemory::ReadCallBack_DIMM(uint64_t addr)
{
    bool transaction_complete = UpdateInProcessTransactionList(addr, DIMM_address_in_processing, false);
    if(transaction_complete)
        complete_transactions++;
    DIMM_complete_addr = addr;
    // std::cout << "dimm complete : " << ++dimm_complete_count << std::endl;
}

bool TraceBasedCPUForHeterogeneousMemory::UpdateInProcessTransactionList(uint64_t addr, std::list<uint64_t>& transactionlist, bool hbm)
{
    bool address_found=false;
    for (std::list<uint64_t>::iterator i=transactionlist.begin(); i!=transactionlist.end(); i++)
    {
       if(*i == addr)
       {
           address_found=true;
        //    if(hbm)
        //     std::cout << "return complete address : " << addr << std::endl;
            break;
       }
    }

    // this logic is for considering duplicate address case
    if(address_found)
    {

        //std :: cout << "HBM " << addr << std::endl;
        int original_length = transactionlist.size();
        transactionlist.remove(addr);
        int changed_length = transactionlist.size();
        int repush = original_length - changed_length - 1;
        // if(repush != 0)
        //     std :: cout << "repushing address : " << addr << " by amount of " << repush << std::endl;
        for(int i=0; i<repush; i++)
            transactionlist.push_back(addr);
    }

    if(address_found)
        return true;
    else
        return false;
}


void TraceBasedCPUForHeterogeneousMemory::LoadTrace(string filename)
{
    std::string line, str_tmp;
    std::ifstream file(filename);
    std::stringstream ss;
    int count = 0;

    std::vector<uint64_t> HBM_pooling;
    std::vector<uint64_t> DIMM_pooling;
    HBM_transaction.push_back(HBM_pooling);
    DIMM_transaction.push_back(DIMM_pooling);
    HBM_transaction[count].clear();
    DIMM_transaction[count].clear();

    std::cout << "Using following trace file : " << filename << std::endl;

    bool last_was_DIMM = false;
    bool is_collision = false;
    if(filename.find("col_"))
        is_collision = true;

    if(file.is_open())
    {
        while (std::getline(file, line, '\n'))
        {
            if(line.empty())
            {
                count++;
                std::vector<uint64_t> HBM_pooling;
                std::vector<uint64_t> DIMM_pooling;
                HBM_transaction.push_back(HBM_pooling);
                DIMM_transaction.push_back(DIMM_pooling);
                HBM_transaction[count].clear();
                DIMM_transaction[count].clear();
                continue;
            }

            ss.str(line);
            string mode = "DIMM";
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
                else
                    addr = std::stoull(str_tmp);

                str_count++;
            }

            // std::cout << addr << std::endl;

            if(mode == "HBM")
            {
                if(is_collision)
                {
                    if(!last_was_DIMM)
                        HBM_transaction[count].push_back(addr);
                }
                else
                    HBM_transaction[count].push_back(addr);
                last_was_DIMM = false;
            }
            else
            {
                DIMM_transaction[count].push_back(addr);
                last_was_DIMM = true;
            }

            ss.clear();
        }
    }

    std::cout << "Done loading trace file" << std::endl;

}

}  // namespace dramsim3
