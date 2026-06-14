#pragma once

#include <cstdio>
#include <cstdint>

constexpr uint64_t DEFAULT_K0 = 3;
constexpr uint64_t DEFAULT_K1 = 1;
constexpr uint64_t DEFAULT_K2 = 1;
constexpr uint64_t DEFAULT_R = 8;
constexpr uint64_t DEFAULT_M = 2;
constexpr uint64_t DEFAULT_F = 2;

/**
 * You don't need to customize this structure.
 * Please refer to 'void print_statistics(proc_stats_t* p_stats)' in procsim_driver.cpp.
 */
struct proc_stats_t {
    float avg_inst_retired;
    unsigned long retired_instruction;
    unsigned long cycle_count;
};

/**
 * The structures below are provided as examples, with one of them filled in
 * with sample members for reference. You are free to define your own C/C++
 * structures for your simulator implementation.
 */
struct proc_inst_t {
    uint32_t id;
    uint32_t inst_addr;
    int32_t op_code;
    int32_t src_regs[2];
    int32_t dst_reg;

    uint32_t fetch_time;
    uint32_t disp_time;
    uint32_t sched_time;
    uint32_t exec_time;
    uint32_t update_time;
    uint32_t retire_time;
    uint32_t src_tag[2];

    bool src_pending[2];
    bool src_valid[2];
    bool rob_updated;
    bool rob_completed;
};

struct cycle_log_t {};

struct rob_entry_t {};

struct reg_file_entry_t {};

struct sched_queue_entry_t {};

/**
 * You are required to implement the following functions.
 * In addition, you may design additional classes or functions to support your simulator implementation.
 */
void print_pipeline_trace();
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f, uint64_t m);
void run_proc(proc_stats_t* p_stats, FILE* p_file);
void complete_proc(proc_stats_t* p_stats);
