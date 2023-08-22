#ifndef __COMMON_H
#define __COMMON_H

#include <stdint.h>
#include <iostream>
#include <vector>

namespace dramsim3 {

struct Address {
    Address()
        : channel(-1), rank(-1), bankgroup(-1), bank(-1), row(-1), column(-1) {}
    Address(int channel, int rank, int bankgroup, int bank, int row, int column)
        : channel(channel),
          rank(rank),
          bankgroup(bankgroup),
          bank(bank),
          row(row),
          column(column) {}
    Address(const Address& addr)
        : channel(addr.channel),
          rank(addr.rank),
          bankgroup(addr.bankgroup),
          bank(addr.bank),
          row(addr.row),
          column(addr.column) {}
    int channel;
    int rank;
    int bankgroup;
    int bank;
    int row;
    int column;
};

inline uint32_t ModuloWidth(uint64_t addr, uint32_t bit_width, uint32_t pos) {
    addr >>= pos;
    auto store = addr;
    addr >>= bit_width;
    addr <<= bit_width;
    return static_cast<uint32_t>(store ^ addr);
}

// extern std::function<Address(uint64_t)> AddressMapping;
int GetBitInPos(uint64_t bits, int pos);
// it's 2017 and c++ std::string still lacks a split function, oh well
std::vector<std::string> StringSplit(const std::string& s, char delim);
template <typename Out>
void StringSplit(const std::string& s, char delim, Out result);

int LogBase2(int power_of_two);
void AbruptExit(const std::string& file, int line);
bool DirExist(std::string dir);

enum class CommandType {
    READ,
    READ_PRECHARGE,
    WRITE,
    WRITE_PRECHARGE,
    ACTIVATE,
    PRECHARGE,
    REFRESH_BANK,
    REFRESH,
    SREF_ENTER,
    SREF_EXIT,
    SIZE
};

struct Command {
    Command() : cmd_type(CommandType::SIZE), hex_addr(0) {}
    Command(CommandType cmd_type, const Address& addr, uint64_t hex_addr)
        : cmd_type(cmd_type), addr(addr), hex_addr(hex_addr) {}
    // Command(const Command& cmd) {}

    bool IsValid() const { return cmd_type != CommandType::SIZE; }
    bool IsRefresh() const {
        return cmd_type == CommandType::REFRESH ||
               cmd_type == CommandType::REFRESH_BANK;
    }
    bool IsRead() const {
        return cmd_type == CommandType::READ ||
               cmd_type == CommandType ::READ_PRECHARGE;
    }
    bool IsWrite() const {
        return cmd_type == CommandType ::WRITE ||
               cmd_type == CommandType ::WRITE_PRECHARGE;
    }
    bool IsAct() const{
        return cmd_type == CommandType::ACTIVATE;
    }
    bool IsReadWrite() const { return IsRead() || IsWrite(); }
    bool IsRankCMD() const {
        return cmd_type == CommandType::REFRESH ||
               cmd_type == CommandType::SREF_ENTER ||
               cmd_type == CommandType::SREF_EXIT;
    }
    CommandType cmd_type;
    Address addr;
    uint64_t hex_addr;

    int Channel() const { return addr.channel; }
    int Rank() const { return addr.rank; }
    int Bankgroup() const { return addr.bankgroup; }
    int Bank() const { return addr.bank; }
    int Row() const { return addr.row; }
    int Column() const { return addr.column; }

    friend std::ostream& operator<<(std::ostream& os, const Command& cmd);
};

struct PimValues {
    PimValues()
        : skewed_cycle(0), 
          vector_transfer(false), 
          is_r_vec(false), 
          is_locality_bit(false),
          batch_tag(false),
          decode_cycle(0) {} // Default constructor
    
    PimValues(uint64_t skewedCycle, bool vectorTransfer, bool isRVec, bool isLocalityBit, int batchTag)
        : skewed_cycle(skewedCycle), 
          vector_transfer(vectorTransfer), 
          is_r_vec(isRVec), 
          is_locality_bit(isLocalityBit),
          batch_tag(batchTag),
          decode_cycle(0) {} // Parameterized constructor

    PimValues(const PimValues& pim_values)
        : skewed_cycle(pim_values.skewed_cycle), 
          vector_transfer(pim_values.vector_transfer), 
          is_r_vec(pim_values.is_r_vec), 
          is_locality_bit(pim_values.is_locality_bit),
          batch_tag(pim_values.batch_tag),
          decode_cycle(pim_values.decode_cycle) {} // Parameterized constructor

    uint64_t skewed_cycle;
    bool vector_transfer;
    bool is_r_vec;
    bool is_locality_bit;
    int batch_tag;
    uint64_t decode_cycle;

};

struct Transaction {
    Transaction() {}
    Transaction(uint64_t addr, bool is_write)
        : addr(addr),
          added_cycle(0),
          complete_cycle(0),
          is_write(is_write),
          pim_values() {}
    Transaction(const Transaction& tran)
        : addr(tran.addr),
          added_cycle(tran.added_cycle),
          complete_cycle(tran.complete_cycle),
          is_write(tran.is_write),
          pim_values(tran.pim_values) {}
    Transaction(uint64_t addr, bool is_write, PimValues pim_values_)
        : addr(addr),
          added_cycle(0),
          complete_cycle(0),
          is_write(is_write),
          pim_values(pim_values_) {}

    uint64_t addr;
    uint64_t added_cycle;
    uint64_t complete_cycle;
    bool is_write;

    // PIM parameters
    PimValues pim_values;

    friend std::ostream& operator<<(std::ostream& os, const Transaction& trans);
    friend std::istream& operator>>(std::istream& is, Transaction& trans);
};



}  // namespace dramsim3
#endif
