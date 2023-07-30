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
        memory_system_.AddTransaction(last_addr_, last_write_);
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
        memory_system_.AddTransaction(addr_a_ + offset_, false);
        inserted_a_ = true;
    }
    if (!inserted_b_ &&
        memory_system_.WillAcceptTransaction(addr_b_ + offset_, false)) {
        memory_system_.AddTransaction(addr_b_ + offset_, false);
        inserted_b_ = true;
    }
    if (!inserted_c_ &&
        memory_system_.WillAcceptTransaction(addr_c_ + offset_, true)) {
        memory_system_.AddTransaction(addr_c_ + offset_, true);
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
                memory_system_.AddTransaction(trans_.addr, trans_.is_write);
            }
        }
    }
    clk_++;
    return;
}

TraceBasedCPUForHeterogeneousMemory::TraceBasedCPUForHeterogeneousMemory(const std::string& config_file,
                             const std::string& output_dir,
                             const std::string& trace_file)
    : CPU(config_file, output_dir) {

    std::cout << "Using following trace file : " << std::endl;
    HBM_complete_addr = -1;
    DIMM_complete_addr = -1;
    GetTrace(trace_file);
}

void TraceBasedCPUForHeterogeneousMemory::ClockTick() {
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
                memory_system_.AddTransaction(trans_.addr, trans_.is_write);
            }
        }
    }
    clk_++;
    return;
}

void TraceBasedCPUForHeterogeneousMemory::ReadCallBack(uint64_t addr)
{
    if(HBM_complete_addr != addr)
        HBM_complete_addr = addr;

    if(DIMM_complete_addr != addr)
        DIMM_complete_addr = addr;
}

void TraceBasedCPUForHeterogeneousMemory::GetTrace(string filename)
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

    if(file.is_open())
    {
        while (std::getline(file, line, '\n'))
        {
            if(count==3)
                exit(0);
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

            std::cout << addr << std::endl;

            if(mode == "HBM")
                HBM_transaction[count].push_back(addr);
            else
                DIMM_transaction[count].push_back(addr);

            ss.clear();
        }
    }

    std::cout << "Done loading trace file" << std::endl;

}

}  // namespace dramsim3
