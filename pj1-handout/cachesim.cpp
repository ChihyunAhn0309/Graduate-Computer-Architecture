#include "cachesim.hpp"
#include <cassert>
#include <cstdint>
#include <vector>

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


struct Cache_Block{
    bool dirty;
    bool valid;
    bool prefetched;
    uint64_t tag;
    uint64_t last_access_time;
};

std::vector<std::vector<Cache_Block>> L1;
std::vector<std::vector<Cache_Block>> L2;

uint64_t cycle = 0;

uint64_t C1 = 0, B1 = 0, S1 = 0, C2 = 0, B2 = 0, S2 = 0;
uint32_t K = 0;

uint64_t Last_Miss_Block_addr = ~0ULL;
long long Pending_Stride = 0;
long long d = 0;

uint64_t get_tag1(uint64_t address){
    return address >> (C1 - S1);
}

uint64_t get_tag2(uint64_t address){
    return address >> (C2 - S2);
}

uint64_t get_index1(uint64_t address){
    return (address & ((1ULL << (C1 - S1)) - 1)) >> B1;
}

uint64_t get_index2(uint64_t address){
    return (address & ((1ULL << (C2 - S2)) - 1)) >> B2;
}

int find_LRU1(std::vector<Cache_Block> &target_L1){
    int min_index = 0;
    uint64_t min_time = target_L1[0].last_access_time;
    for(uint64_t i = 1; i < (1ULL << S1); i++){
        if(target_L1[i].last_access_time < min_time){
            min_time = target_L1[i].last_access_time;
            min_index = i;
        }
    }
    return min_index;
}

int find_LRU2(std::vector<Cache_Block> &target_L2){
    int min_index = 0;
    uint64_t min_time = target_L2[0].last_access_time;
    for(uint64_t i = 1; i < (1ULL << S2); i++){
        if(target_L2[i].last_access_time < min_time){
            min_time = target_L2[i].last_access_time;
            min_index = i;
        }
    }
    return min_index;
}

uint64_t find_LRU2_time(std::vector<Cache_Block> &target_L2){
    uint64_t min_time = 0;
    for(uint64_t i = 0; i < (1ULL << S2); i++){
        if(target_L2[i].last_access_time != 0){
            min_time = target_L2[i].last_access_time;
            break;
        }   
    }
    for(uint64_t i = 0; i < (1ULL << S2); i++){
        if(target_L2[i].last_access_time < min_time && target_L2[i].last_access_time != 0){
            min_time = target_L2[i].last_access_time;
        }
    }
    if (min_time == 0) return 1;
    else return min_time;
}

void WB_to_L2(uint64_t block_address, struct cache_stats_t* p_stats){
    uint64_t index2 = get_index2(block_address);
    uint64_t tag2 = get_tag2(block_address);
    std::vector<Cache_Block>& target_L2 = L2[index2];
    for (uint64_t i = 0; i < (1ULL<<S2); i++){
        if(target_L2[i].valid && target_L2[i].tag == tag2){
            target_L2[i].dirty = true;
            return;
        }   
    }
    p_stats->write_backs++;
}

int evict1(std::vector<Cache_Block> &target_L1, struct cache_stats_t* p_stats, uint64_t tag1, uint64_t index1, char rw){
    int evict_set = find_LRU1(target_L1);
    if(target_L1[evict_set].dirty){
        //Write-Back or L2 block 존재 확인후 L2 값 update & dirty bit setting
        uint64_t block_address = (target_L1[evict_set].tag << (C1 - S1)) + (index1 << B1);
        WB_to_L2(block_address, p_stats);
    }
    if(tag1 != 0){
        target_L1[evict_set].valid = true;
        target_L1[evict_set].tag = tag1;
    }
    else{
        target_L1[evict_set].valid = false;
        target_L1[evict_set].tag = 0;
    }
    if(rw == 'r') target_L1[evict_set].dirty = false;
    else target_L1[evict_set].dirty = true;
    // 여기 살짝 이상할수도..?
    target_L1[evict_set].prefetched = false;
    target_L1[evict_set].last_access_time = cycle;
    return evict_set;
}

int evict2(std::vector<Cache_Block> &target_L2, struct cache_stats_t* p_stats, uint64_t tag2, bool prefetch){
    int evict_set = find_LRU2(target_L2);
    if(target_L2[evict_set].dirty){
        //WB
        p_stats->write_backs++;
    }
    if(tag2 != 0){
        target_L2[evict_set].valid = true;
        target_L2[evict_set].tag = tag2;
    }
    else{
        target_L2[evict_set].valid = false;
        target_L2[evict_set].tag = 0;
    }
    target_L2[evict_set].dirty = false;
    if (!prefetch){
        target_L2[evict_set].last_access_time = cycle;
        target_L2[evict_set].prefetched = false;
    }
    else{
        uint64_t LRU_time = find_LRU2_time(target_L2);
        target_L2[evict_set].last_access_time = LRU_time - 1;
        target_L2[evict_set].prefetched = true;
    }
    
    return evict_set;
}

void DRAM_to_L2(uint64_t address, std::vector<std::vector<Cache_Block>>& L2, struct cache_stats_t* p_stats){
    uint64_t index = get_index2(address);
    uint64_t tag = get_tag2(address);
    std::vector<Cache_Block>& target_L2 = L2[index];
    long long target = -1;
    long long empty = -1;
    for(uint64_t i = 0; i < (1ULL<< S2); i++){
        if(target_L2[i].valid && target_L2[i].tag == tag){
            target = i;
        }
        else if(target_L2[i].valid == 0){
            empty = i;
        }
    }
    // L2 miss
    if (target == -1){
        p_stats->prefetched_blocks++;
        if (empty == -1){
            evict2(target_L2, p_stats, tag, true);
        }
        else{
            target_L2[empty].valid = true;
            target_L2[empty].dirty = false;
            target_L2[empty].tag = tag;
            target_L2[empty].last_access_time = find_LRU2_time(target_L2) - 1;
            target_L2[empty].prefetched = true;
        }
    }
}

// Keeps track of whether Pending_Stride has been established
bool pending_stride_valid = false;

void prefetch(std::vector<std::vector<Cache_Block>>& L2, struct cache_stats_t* p_stats, uint64_t block_address){
    if (Last_Miss_Block_addr == ~0ULL) {
        Last_Miss_Block_addr = block_address;
    }
    else if(!pending_stride_valid){
        Pending_Stride = (long long)block_address - (long long)Last_Miss_Block_addr;
        Last_Miss_Block_addr = block_address;
        pending_stride_valid = true;
    }
    else{
        d = ((long long)block_address - (long long)Last_Miss_Block_addr);
        if (d == Pending_Stride){
            for (uint32_t i = 1; i <= K; i++){
                DRAM_to_L2(block_address + i * Pending_Stride, L2, p_stats);
            }
        }
        else{
            Pending_Stride = d;
        }
        Last_Miss_Block_addr = block_address;
    }
}

void setup_cache(uint64_t c1, uint64_t b1, uint64_t s1, uint64_t c2, uint64_t b2, uint64_t s2, uint32_t k) {
    C1 = c1; B1 = b1; S1 = s1; C2 = c2; B2 = b2; S2 = s2; K = k;
    uint64_t ilen1 = 1ULL << (C1 - B1 - S1);
    uint64_t ilen2 = 1ULL << (C2 - B2 - S2);
    uint64_t assc1 = 1ULL << S1;
    uint64_t assc2 = 1ULL << S2;
    L1.resize(ilen1, std::vector<Cache_Block>(assc1));
    L2.resize(ilen2, std::vector<Cache_Block>(assc2));
    for (uint64_t i = 0; i < ilen1; i++){
        for (uint64_t j = 0; j < assc1; j ++){
            L1[i][j].dirty = false;
            L1[i][j].valid = false;
            L1[i][j].prefetched = false;
            L1[i][j].tag = 0;
            L1[i][j].last_access_time = 0;
        }
    }
    for (uint64_t i = 0; i < ilen2; i++){
        for (uint64_t j = 0; j < assc2; j ++){
            L2[i][j].dirty = false;
            L2[i][j].valid = false;
            L2[i][j].prefetched = false;
            L2[i][j].tag = 0;
            L2[i][j].last_access_time = 0;
        }
    }
    pending_stride_valid = false;
}

/**
 * Subroutine that simulates the cache one trace event at a time.
 *
 * @rw The type of event. Either READ or WRITE
 * @address  The target memory address
 * @p_stats Pointer to the statistics structure
 */

void cache_access(char rw, uint64_t address, cache_stats_t* p_stats) {
    cycle++;
    uint64_t block_address = (address >> B2) << B2;
    uint64_t tag1 = get_tag1(address);
    uint64_t index1 = get_index1(address);
    uint64_t tag2 = get_tag2(address);
    uint64_t index2 = get_index2(address);

    long long empty1 = -1;
    long long empty2 = -1;
    long long target1 = -1;
    long long target2 = -1;

    p_stats->accesses++;
    if (rw == 'r'){
        p_stats->reads++;
    }
    else if(rw == 'w'){
        p_stats->writes++;
    }
    else{
        assert(0);
    }
    p_stats->L1_accesses++;
    std::vector<Cache_Block>& target_L1 = L1[index1];
    for(uint64_t i = 0; i < (1ULL << S1); i++){
        if(target_L1[i].valid && target_L1[i].tag == tag1){
            // p_stats->L1_read_hits++;
            target1 = i;
        }
        else if(target_L1[i].valid == 0){
            empty1 = i;
        }
    }
    // L1 hit
    if (target1 != -1){
        target_L1[target1].last_access_time = cycle;
        if (rw == 'w'){
            target_L1[target1].dirty = true;
        }
        return;
    }
    // L1 miss
    if (rw == 'r') p_stats->L1_read_misses++;
    else p_stats->L1_write_misses++;
    
    p_stats->L2_accesses++;
    std::vector<Cache_Block>& target_L2 = L2[index2];
    for(uint64_t i = 0; i < (1ULL << S2); i++){
        if(target_L2[i].valid && target_L2[i].tag == tag2){
            // p_stats->L2_read_hits++;
            target2 = i;
        }
        else if(target_L2[i].valid == 0){
            empty2 = i;
        }
    }
    // L2 hit
    if (target2 != -1){
        // L1에 cache block 올려야함
        // 여기에서 찾은 target2가 prefetched된 것인지 확인 후 successful_prefetches 증가
        if(target_L2[target2].prefetched){
            p_stats->successful_prefetches++;
            target_L2[target2].prefetched = false;
        }
        if(empty1 == -1){
            evict1(target_L1, p_stats, tag1, index1, rw);
        }
        else{
            target_L1[empty1].valid = true;
            if (rw == 'r') target_L1[empty1].dirty = false;
            else target_L1[empty1].dirty = true;
            target_L1[empty1].tag = tag1;
            target_L1[empty1].prefetched = false;
            target_L1[empty1].last_access_time = cycle;
        }
        target_L2[target2].last_access_time = cycle;
        return;
    }
    // L1 & L2 both miss
    if (rw == 'r') p_stats->L2_read_misses++;
    else p_stats->L2_write_misses++;
    
    if(empty1 == -1){
        evict1(target_L1, p_stats, tag1, index1, rw);
    }
    else{
        target_L1[empty1].valid = true;
        if (rw == 'r') target_L1[empty1].dirty = false;
        else target_L1[empty1].dirty = true;
        target_L1[empty1].tag = tag1;
        target_L1[empty1].prefetched = false;
        target_L1[empty1].last_access_time = cycle;
    }
    if(empty2 == -1){
        evict2(target_L2, p_stats, tag2, false);
    }
    else{
        target_L2[empty2].valid = true;
        target_L2[empty2].dirty = false;
        target_L2[empty2].tag = tag2;
        target_L2[empty2].prefetched = false;
        target_L2[empty2].last_access_time = cycle;
    }
    //prefetch 해야 한다. => L2 캐시로 block 가져올때 eviction에서 write back가능. 따라서 write back만 변한다.
    prefetch(L2, p_stats, block_address);
}

/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_cache(cache_stats_t *p_stats) {
    double HT1 = 2 + 0.2*(double)S1;
    double HT2 = 4 + 0.4*(double)S2;
    double MP2 = 500;

    double MR1 = 0;
    double MR2 = 0;
    if (p_stats->L1_accesses > 0){
        MR1 = ((double)p_stats->L1_read_misses + (double)p_stats->L1_write_misses)/(double)p_stats->L1_accesses;
    }
    if (p_stats->L2_accesses > 0){
        MR2 = ((double)p_stats->L2_read_misses + (double)p_stats->L2_write_misses)/(double)p_stats->L2_accesses;
    }
    
    double MP1 = HT2 + MR2 * MP2;
    p_stats->avg_access_time = HT1 + MR1*MP1;
}
