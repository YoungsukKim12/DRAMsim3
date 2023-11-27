#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include <map>

class CacheBlock {
public:
    int tag;
    int lru_counter;

    CacheBlock(int tag);
};

class CacheSet {
private:
    std::vector<CacheBlock*> blocks;

public:
    CacheSet(int associativity);
    ~CacheSet();
    bool access_block(int tag);
    void load_block(int tag);
};

class Cache {
private:
    std::vector<CacheSet> sets;
    int block_size;
    int hits;
    int misses;

public:
    Cache(int size, int block_size, int associativity);
    bool access(int address);
    double hit_rate() const;
};

#endif // CACHE_H
