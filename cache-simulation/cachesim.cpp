#include "cachesim.hpp"
#include <vector>
#include <cstdint>
#include <algorithm>
#include <climits>
#include <cstdio>

/* Internal data structures */
struct CacheBlock {
    bool valid;
    bool dirty;
    bool prefetched;            // true if block was brought in by prefetch and not yet "demand-hit"
    uint64_t tag;
    int64_t lru_time;          // higher = more recently used

    CacheBlock(bool _valid = false, bool _dirty = false, bool _prefetched = false, uint64_t _tag = 0, uint64_t _lru_time = 0)
        : valid(_valid), dirty(_dirty), prefetched(_prefetched), tag(_tag), lru_time(_lru_time) {}
};

struct Cache {
    uint64_t C, B, S;           // logs of (cache size, block size, # of ways)
    uint64_t num_sets;          // 2^(C-B-S)
    uint64_t num_ways;          // 2^S
    uint64_t block_size;        // 2^B
    uint64_t offset_bits;       // B
    uint64_t index_bits;        // C-B-S
    uint64_t tag_shift;         // B + (C-B-S)
    std::vector<std::vector<CacheBlock>> sets; // [set_index][way]
    uint64_t timer;             // global LRU counter for this level
    
    Cache() {}
    Cache(uint64_t c, uint64_t b, uint64_t s): C(c), B(b), S(s) {
        num_sets = 1ULL << (C - B - S);
        num_ways = 1ULL << S;
        block_size = 1ULL << B;
        offset_bits = B;
        index_bits = C - B - S;
        tag_shift = offset_bits + index_bits;
        sets.assign(num_sets, std::vector<CacheBlock>(num_ways, CacheBlock()));
        timer = 0;
    }
} L1, L2;

uint32_t prefetch_K;
uint64_t last_miss_block_addr;
int64_t pending_stride;
bool first_l2_miss;

static inline uint64_t get_index(const Cache& cache, uint64_t addr) {
    return (addr >> cache.offset_bits) & (cache.num_sets - 1);
}
 
static inline uint64_t get_tag(const Cache& cache, uint64_t addr) {
    return addr >> cache.tag_shift;
}
 
static inline uint64_t block_addr(const Cache& cache, uint64_t addr) {
    return addr & ~(cache.block_size - 1); // zero out offset bits
}

/**
 * Subroutine for initializing the cache. You many add and initialize any global or heap
 * variables as needed.
 *
 * @c1 Total size of L1 in bytes is 2^C1
 * @b1 Size of each block in L1 in bytes is 2^B1
 * @s1 Number of blocks per set in L1 is 2^S1
 * @c2 Total size of L2 in bytes is 2^C2
 * @b2 Size of each block in L2 in bytes is 2^B2
 * @s2 Number of blocks per set in L2 is 2^S2
 * @k Prefetch K subsequent blocks
 */
void setup_cache(uint64_t c1, uint64_t b1, uint64_t s1, uint64_t c2, uint64_t b2, uint64_t s2, uint32_t k) {
    L1 = Cache(c1, b1, s1);
    L2 = Cache(c2, b2, s2);

    prefetch_K = k;

    first_l2_miss = true;
    last_miss_block_addr = 0;
    pending_stride = 0;
}

// Returns way index if hit, -1 if miss.
// If hit and it's a prefetched block that hasn't been demand-accessed, sets
// *was_prefetch_hit = true so the caller can count a successful prefetch.
int64_t find_in_set(Cache& cache, uint64_t set_idx, uint64_t tag, bool *was_prefetch_hit = nullptr) {
    for(uint64_t i = 0; i < cache.num_ways; ++i) {
        CacheBlock& block = cache.sets[set_idx][i];
        if(block.valid && block.tag == tag) {
            if(block.prefetched && was_prefetch_hit)
                *was_prefetch_hit = true;
            return i;
        }
    }
    return -1;
}

void touch(Cache& cache, uint64_t set_idx, uint64_t way_idx) {
    cache.sets[set_idx][way_idx].lru_time = ++cache.timer;
    // demand access -> clear prefetched flag
    cache.sets[set_idx][way_idx].prefetched = false;
}

/* find the oldest (LRU) way */
uint64_t find_lru_way(Cache& cache, uint64_t set_idx) {
    uint64_t lru_idx = 0;
    int64_t oldest = INT64_MAX;
    for(uint64_t i = 0; i < cache.num_ways; ++i) {
        CacheBlock& b = cache.sets[set_idx][i];
        if(!b.valid) return i; // preferred empty slot
        if(b.lru_time < oldest) {
            oldest = b.lru_time;
            lru_idx = i;
        }
    }
    return lru_idx;
}

/* return true if write back is needed */
bool evict_l1_cache(uint64_t set_idx, uint64_t way_idx) {
    CacheBlock& block = L1.sets[set_idx][way_idx];
    if(!block.valid || !block.dirty) {
        block.valid = false;
        return false;
    }

    block.valid = false;

    // tag bits + idx bits + offset bits
    // could fill offset with 0 as B2 >= B1
    uint64_t addr = (block.tag << L1.tag_shift) | (set_idx << L1.offset_bits);
    addr = block_addr(L2, addr);
    uint64_t l2_tag = get_tag(L2, addr);
    uint64_t l2_set_idx = get_index(L2, addr);
    int64_t l2_way_idx = find_in_set(L2, l2_set_idx, l2_tag);

    if(l2_way_idx != -1) {
        // delegate write-back to L2 cache, just mark as dirty
        L2.sets[l2_set_idx][l2_way_idx].dirty = true;
        return false;
    }

    // L2 misses, write-back to memory
    return true;
}

void install_l1_block(uint64_t address, bool dirty, cache_stats_t* p_stats) {
    uint64_t l1_set_idx = get_index(L1, address);
    uint64_t l1_way_idx = find_lru_way(L1, l1_set_idx);

    bool wb = evict_l1_cache(l1_set_idx, l1_way_idx);
    if(wb && p_stats) {
        ++p_stats->write_backs;
    }

    // install new block
    CacheBlock& block = L1.sets[l1_set_idx][l1_way_idx];
    block.valid = true;
    block.dirty = dirty;
    block.prefetched = false;
    block.tag = get_tag(L1, address);
    block.lru_time = ++L1.timer;
}

void install_l2_block(uint64_t address, bool dirty, bool prefetched, cache_stats_t* p_stats) {
    uint64_t l2_set_idx = get_index(L2, address);
    int l2_way_idx = find_lru_way(L2, l2_set_idx);
    CacheBlock& block = L2.sets[l2_set_idx][l2_way_idx];

    if(block.valid && block.dirty && p_stats) {
        ++p_stats->write_backs;
    }

    block.valid = true;
    block.dirty = dirty;
    block.prefetched = prefetched;
    // lru_time starts from 1 for demand block
    if(block.prefetched) {
        int64_t min_time = INT64_MAX;
        for (int w = 0; w < (int)L2.num_ways; w++) {
            CacheBlock& other = L2.sets[l2_set_idx][w];
            if (other.valid && &other != &block)
                min_time = std::min(min_time, other.lru_time);
        }
        block.lru_time = min_time - 1;
    } else {
        block.lru_time = ++L2.timer;
    }
    block.tag = get_tag(L2, address);
}

/**
 * Subroutine that simulates the cache one trace event at a time.
 *
 * @rw The type of event. Either READ or WRITE
 * @address  The target memory address
 * @p_stats Pointer to the statistics structure
 */
void cache_access(char rw, uint64_t address, cache_stats_t* p_stats) {
    ++p_stats->accesses;
    ++p_stats->L1_accesses;

    bool is_write = (rw == WRITE);
    if(is_write) ++p_stats->writes;
    else ++p_stats->reads;

    uint64_t l1_set_idx = get_index(L1, address);
    uint64_t l1_tag = get_tag(L1, address);
    int64_t l1_way_idx = find_in_set(L1, l1_set_idx, l1_tag);

    /* L1 hits */
    if(l1_way_idx != -1) {
        if(is_write)
            L1.sets[l1_set_idx][l1_way_idx].dirty = true;
        touch(L1, l1_set_idx, l1_way_idx);
        return;
    }
    
    /* L1 misses */
    if(is_write) ++p_stats->L1_write_misses;
    else ++p_stats->L1_read_misses;

    /* find in L2 */
    ++p_stats->L2_accesses;

    uint64_t l2_set_idx = get_index(L2, address);
    uint64_t l2_tag = get_tag(L2, address);
    bool prefetch_hit = false;
    int64_t l2_way_idx = find_in_set(L2, l2_set_idx, l2_tag, &prefetch_hit);

    /* L2 hits */
    if(l2_way_idx != -1) {
        if(prefetch_hit) {
            ++p_stats->successful_prefetches;
            L2.sets[l2_set_idx][l2_way_idx].prefetched = false;
        }
        touch(L2, l2_set_idx, l2_way_idx);
        install_l1_block(address, is_write, p_stats);
        return;
    }

    /* L2 misses */
    if(is_write) ++p_stats->L2_write_misses;
    else ++p_stats->L2_read_misses;

    /* load blocks */
    install_l2_block(address, false, false, p_stats);
    install_l1_block(address, is_write, p_stats);

    /* prefetch */
    uint64_t cur_block = block_addr(L2, address);

    if(first_l2_miss) {
       first_l2_miss = false;
    } else {
        int64_t d = (int64_t)cur_block - (int64_t)last_miss_block_addr;
        if(prefetch_K > 0 && d == pending_stride && d != 0) {
            for(int64_t i = 1; i <= prefetch_K; ++i) {
                uint64_t prefetched_addr = cur_block + i * pending_stride;
                uint64_t l2_set_idx = get_index(L2, prefetched_addr);
                uint64_t l2_tag = get_tag(L2, prefetched_addr);
                if(find_in_set(L2, l2_set_idx, l2_tag) < 0) {
                    // only prefetch if missed
                    install_l2_block(prefetched_addr, false, true, p_stats);
                    ++p_stats->prefetched_blocks;
                }
            }
        }
        pending_stride = d;
    }

    last_miss_block_addr = cur_block;
}

/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_cache(cache_stats_t *p_stats) {
    uint64_t l1_total_misses = p_stats->L1_read_misses + p_stats->L1_write_misses;
    uint64_t l2_total_misses = p_stats->L2_read_misses + p_stats->L2_write_misses;
 
    double MR1 = (p_stats->L1_accesses > 0)
                 ? (double)l1_total_misses / p_stats->L1_accesses
                 : 0.0;
    double MR2 = (p_stats->L2_accesses > 0)
                 ? (double)l2_total_misses / p_stats->L2_accesses
                 : 0.0;

    double HT1 = 2.0 + 0.2 * (double)L1.S;
    double HT2 = 4.0 + 0.4 * (double)L2.S;
    double MP2 = 500.0;
    double AAT2 = HT2 + MR2 * MP2;  // = MP1
    double AAT1 = HT1 + MR1 * AAT2;

    p_stats->avg_access_time = AAT1;
 
    L1.sets.clear();
    L2.sets.clear();
}
