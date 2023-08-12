#ifndef __CPU_H
#define __CPU_H

#include <fstream>
#include <functional>
#include <random>
#include <string>
#include <list>
#include "memory_system.h"

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

    void ReadCallBack_HBM(uint64_t addr);
    void ReadCallBack_DIMM(uint64_t addr);
    void WriteCallBack_HBM(uint64_t addr){}
    void WriteCallBack_DIMM(uint64_t addr){}
    void PrintStats() { memory_system_HBM.PrintStats(); }
    void ClockTick();
    void RunNMP();
    void RunHEAM();
    void LoadTrace(string filename);
    void AddTransactionsToMemory(std::vector<uint64_t> HBM_transaction, std::vector<uint64_t> DIMM_transaction, int &HBM_vectors_left, int &DIMM_vectors_left, std::unordered_map<int, uint64_t> vector_transfer_address);
    bool UpdateInProcessTransactionList(uint64_t addr, std::list<uint64_t>& transactionlist);
    void HeterogeneousMemoryClockTick();
    int GetBankGroup(uint64_t address);
    int GetChannel(uint64_t address);
    std::unordered_map<int, uint64_t> ProfileAddresses(const std::vector<uint64_t>& addresses);
    bool IsLastAddressInBankGroup(const std::unordered_map<int, uint64_t>& lastAddressInBankGroup, uint64_t address);
    int GetTotalPIMTransfers(std::unordered_map<int, uint64_t> lastAddressInBankGroup);

   private:
    MemorySystem memory_system_HBM;
    MemorySystem memory_system_DIMM;
    uint64_t clk_HBM;
    uint64_t clk_DIMM;

    std::ifstream trace_file_;
    Transaction trans_;
    bool HBM_get_next_ = true;
    bool DIMM_get_next_ = true;
    int complete_transactions = 0;
    int add_cycle = 3;

    std::vector<std::vector<uint64_t>> HBM_transaction;
    std::vector<std::vector<uint64_t>> DIMM_transaction;

    std::list<uint64_t> HBM_address_in_processing;
    std::list<uint64_t> DIMM_address_in_processing;

    uint64_t HBM_complete_addr;
    uint64_t DIMM_complete_addr;

    // for callback debug
    int hbm_complete_count;
    int dimm_complete_count;

    bool is_using_HEAM;
    int channels = 16;
    int bankgroups = 4;
    int vec_transfers = 0;
};


}  // namespace dramsim3
#endif
