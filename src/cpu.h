#ifndef __CPU_H
#define __CPU_H

#include <fstream>
#include <functional>
#include <random>
#include <string>
#include <list>
#include "memory_system.h"
#include "common.h"
#include "pim.h"
#include <tuple>
#include "cache_.h"

namespace dramsim3 {

class CPU {
   public:
    CPU(const std::string& config_file, const std::string& output_dir)
        : memory_system_(
              config_file, output_dir,
              std::bind(&CPU::ReadCallBack, this, std::placeholders::_1),
              std::bind(&CPU::WriteCallBack, this, std::placeholders::_1)),
          clk_(0) {}
    virtual void ClockTick() = 0;
    void ReadCallBack(uint64_t addr) { return; }
    void WriteCallBack(uint64_t addr) { return; }
    void PrintStats() { memory_system_.PrintStats(); }

   protected:
    MemorySystem memory_system_;
    uint64_t clk_;
};

class RandomCPU : public CPU {
   public:
    using CPU::CPU;
    void ClockTick() override;

   private:
    uint64_t last_addr_;
    bool last_write_ = false;
    std::mt19937_64 gen;
    bool get_next_ = true;
};

class StreamCPU : public CPU {
   public:
    using CPU::CPU;
    void ClockTick() override;

   private:
    uint64_t addr_a_, addr_b_, addr_c_, offset_ = 0;
    std::mt19937_64 gen;
    bool inserted_a_ = false;
    bool inserted_b_ = false;
    bool inserted_c_ = false;
    const uint64_t array_size_ = 2 << 20;  // elements in array
    const int stride_ = 64;                // stride in bytes
};

class TraceBasedCPU : public CPU {
   public:
    TraceBasedCPU(const std::string& config_file, const std::string& output_dir,
                  const std::string& trace_file);
    ~TraceBasedCPU() { trace_file_.close(); }
    void ClockTick() override;

   private:
    std::ifstream trace_file_;
    Transaction trans_;
    bool get_next_ = true;
};


class CPUForHetergeneousMemory {
   public:
    CPUForHetergeneousMemory(const std::string& config_file_HBM, const std::string& config_file_DIMM, const std::string& output_dir)
        : memory_system_HBM(
              config_file_HBM, output_dir,
              std::bind(&CPUForHetergeneousMemory::ReadCallBack_HBM, this, std::placeholders::_1),
              std::bind(&CPUForHetergeneousMemory::WriteCallBack_HBM, this, std::placeholders::_1)),
        memory_system_DIMM(
            config_file_DIMM, output_dir,
              std::bind(&CPUForHetergeneousMemory::ReadCallBack_HBM, this, std::placeholders::_1),
              std::bind(&CPUForHetergeneousMemory::WriteCallBack_DIMM, this, std::placeholders::_1)),
          clk_HBM(0), clk_DIMM(0) {}
    virtual void ClockTick() = 0;
    void ReadCallBack_HBM(uint64_t addr) { return; }
    void ReadCallBack_DIMM(uint64_t addr) { return; }
    void WriteCallBack_HBM(uint64_t addr) { return; }
    void WriteCallBack_DIMM(uint64_t addr) { return; }
    void PrintStats() { memory_system_HBM.PrintStats(); }

   protected:
    MemorySystem memory_system_HBM;
    MemorySystem memory_system_DIMM;
    uint64_t clk_HBM;
    uint64_t clk_DIMM;
};


class TraceBasedCPUForHeterogeneousMemory : public CPU {
   public:
    TraceBasedCPUForHeterogeneousMemory(const std::string& config_file_HBM, const std::string& config_file_DIMM, const std::string& output_dir, const std::string& trace_file);
    ~TraceBasedCPUForHeterogeneousMemory() {}

    int PIMMemGetBankGroup(uint64_t address);
    int PIMMemGetChannel(uint64_t address);
    int PIMMemGetRank(uint64_t address);
    int MemGetBankGroup(uint64_t address);
    int MemGetChannel(uint64_t address);
    void LoadTrace(string filename);
    void PrintStats() { memory_system_PIM.PrintStats(); }
    void PrintStats_DIMM() { memory_system_Mem.PrintStats(); }

    void ClockTick();

    int RunTensorDIMM();
    int RunRecNMP();
    int RunSPACE();
    int RunHEAM();
    int RunTRiM();

    void ReadCallBack_PIMMem(uint64_t addr);
    void ReadCallBack_Mem(uint64_t addr);
    void WriteCallBack_PIMMem(uint64_t addr){}
    void WriteCallBack_Mem(uint64_t addr){}

    int UpdateBatchInfoForHetero(int batch_start_idx, std::vector<int>& PIMMem_vectors_left, std::vector<int>& Mem_vectors_left, std::vector<std::unordered_map<int, uint64_t>>& vector_transfer_address);
    int UpdateBatchInfo(int batch_start_idx, std::vector<int>& PIMMem_vectors_left, std::vector<std::unordered_map<int, uint64_t>>& vector_transfer_address);

    void AddSingleBatchTransactions(int batch_start_index, int& vectors_left);
    void AddBatchTransactionsToHetero(int batch_start_index, int& batch_tag, std::vector<int>& PIMMem_vectors_left, std::vector<int>& Mem_vectors_left, std::vector<std::unordered_map<int, uint64_t>> vector_transfer_address);
    void AddBatchTransactions(int batch_start_index, int& batch_tag, std::vector<int>& PIMMem_vectors_left, std::vector<std::unordered_map<int, uint64_t>> vector_transfer_address);
    // void AddBatchTransactionsForRecNMP(int batch_index, int batch_tag, std::vector<int>& PIMMem_vectors_left, std::vector<std::unordered_map<int, uint64_t>> pim_transfer_address);
    void AddTransactionsToPIMMem(int batch_start_idx, int batch_tag, int& HBM_vectors_left, std::unordered_map<int, uint64_t> vector_transfer_address);
    void AddTransactionsToMemory(int batch_start_idx, int batch_tag, int& DIMM_vectors_left);
    void ProfileVectorToTransfer(std::unordered_map<int, uint64_t>&, int batch_start_idx, int batch_idx);
    bool UpdateInProcessTransactionList(uint64_t addr, std::list<uint64_t>& transactionlist, bool hbm);
    EmbDataInfo PullVectorInformation(int emb_pool_idx, int target_vec_idx, std::unordered_map<int, uint64_t> pim_transfer_address);
    void RescheduleTransactions(int batch_start_idx, int batch_id);
    void ReorderHBMTransaction(int emb_pool_idx);
    uint64_t HotEntryReplication(uint64_t target_addr);
    // void AddTransactionsToMemory(int batch_start_index, int& batch_tag, int &HBM_vectors_left, int &DIMM_vectors_left, std::unordered_map<int, uint64_t> vector_transfer_address);

   private:
    MemorySystem memory_system_PIM;
    MemorySystem memory_system_Mem;
    uint64_t clk_PIM;
    uint64_t clk_Mem;
    const Config* PIMMem_config;
    const Config* Mem_config;

    std::ifstream trace_file_;
    Transaction trans_;
    int complete_transactions = 0;
    int add_cycle = 3;

    std::vector<std::vector<std::tuple<uint64_t, char, int, int, int>>> PIMMem_transaction;
    std::vector<std::vector<std::tuple<uint64_t, char, int>>> Mem_transaction;
    std::list<uint64_t> PIMMem_address_in_processing;
    std::list<uint64_t> Mem_address_in_processing;
    uint64_t PIMMem_complete_addr;
    uint64_t Mem_complete_addr;

    bool CA_compression;
    bool is_using_TensorDIMM;
    bool is_using_RecNMP;
    bool is_using_TRiM;
    bool is_using_SPACE;
    bool is_using_rankNMP;
    bool is_using_PIM;
    bool is_using_LUT;
    bool is_using_HEAM;
    int collision;

    int cache_hit_on_transfer_vec=0;

    int num_rds;
    int channels;
    int ranks;
    int bankgroups;
    int vec_transfers;
    int batch_size;
    int num_ca_in_cycle;
    std::string addrmapping;
    std::vector<int> loads_per_bg;
    std::vector<int> loads_per_bg_for_q;
    Cache recNMPCache;

};


}  // namespace dramsim3
#endif
