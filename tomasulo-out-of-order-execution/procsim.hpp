#pragma once

#include <cstdio>
#include <cstdint>

#define FU_TYPES 3
#define NUM_REG 32

constexpr uint64_t DEFAULT_K0 = 3;
constexpr uint64_t DEFAULT_K1 = 1;
constexpr uint64_t DEFAULT_K2 = 1;
constexpr uint64_t DEFAULT_R = 8;
constexpr uint64_t DEFAULT_M = 2;
constexpr uint64_t DEFAULT_F = 2;

struct proc_stats_t {
    float avg_inst_retired;
    unsigned long retired_instruction;
    unsigned long cycle_count;
};

struct proc_inst_t {
    uint32_t id;
    uint32_t inst_addr;
    int32_t op_code;
    int32_t src_regs[2];
    int32_t dst_reg;

    /* logging */
    uint32_t fetch_cycle;
    uint32_t disp_cycle;
    uint32_t sched_cycle;
    uint32_t exec_cycle;
    uint32_t state_cycle;
    uint32_t retire_cycle;
};

struct rob_entry_t {
    proc_inst_t* instr;
    uint32_t tag;
    int32_t completed_cycle;
};

struct reg_file_entry_t {
    uint32_t reg_id;
    uint32_t rob_tag;
    bool valid;
};

struct sched_queue_entry_t {
    proc_inst_t* instr;
    bool fired;
    bool completed;
    bool src_ready[2];
    uint32_t src_tag[2];
};

struct common_data_bus_t {
    uint32_t tag;
    int32_t dst_reg_id;
};

void print_pipeline_trace();
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f, uint64_t m);
void run_proc(proc_stats_t* p_stats, FILE* p_file);
void complete_proc(proc_stats_t* p_stats);
