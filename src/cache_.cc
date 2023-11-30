#include "cache_.h"
#include <iostream>

// Constructor for CacheBlock
CacheBlock::CacheBlock(int tag) : tag(tag), lru_counter(0) {}

// Constructor for CacheSet
CacheSet::CacheSet(int associativity) : blocks(associativity, nullptr) {}

// Destructor for CacheSet
CacheSet::~CacheSet() {
    for (auto& block : blocks) {
        delete block;
    }
}

void CacheSet::update_lru_counters(CacheBlock* accessed_block) {
    for (auto& block : blocks) {
        if (block && block != accessed_block && block->lru_counter < accessed_block->lru_counter) {
            block->lru_counter++;
        }
    }
    accessed_block->lru_counter = 0;
}

// Access a block in the cache set
bool CacheSet::access_block(int tag) {
    auto it = block_map.find(tag);
    if (it != block_map.end()) {
        update_lru_counters(it->second);
        return true; // Cache hit
    }

    // Cache miss, load the block
    load_block(tag);
    return false;
}

// Load a block into the cache set
void CacheSet::load_block(int tag) {
    CacheBlock* lru_block = nullptr;
    int max_lru = -1;
    for (auto& block : blocks) {
        if (!block) {
            block = new CacheBlock(tag);
            block_map[tag] = block;
            return;
        } else if (block->lru_counter > max_lru) {
            lru_block = block;
            max_lru = block->lru_counter;
        }
    }

    // Replace the LRU block
    block_map.erase(lru_block->tag);
    lru_block->tag = tag;
    block_map[tag] = lru_block;
    update_lru_counters(lru_block);
}

// Constructor for Cache
Cache::Cache(int size, int block_size, int associativity) 
    : block_size(block_size), hits(0), misses(0) {
    int num_sets = size / (block_size * associativity);
    for (int i = 0; i < num_sets; ++i) {
        sets.emplace_back(associativity);
    }
}

// Access a block in the cache
bool Cache::access(int address) {
    int tag = address / (block_size * sets.size());
    int set_index = (address / block_size) % sets.size();

    if (sets[set_index].access_block(tag)) {
        // hits++;
        return true;
    } else {
        // misses++;
        return false;
    }
}

// // Calculate the hit rate of the cache
// double Cache::hit_rate() const {
//     return hits + misses > 0 ? static_cast<double>(hits) / (hits + misses) : 0;
// }
