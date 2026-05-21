#include "procsim.hpp"
#include <vector>
#include <map>
#include <deque>
#include <algorithm>
#include <iostream>

std::vector<proc_inst_t*> instructions;
std::deque<rob_entry_t> reorder_buffer;
std::deque<sched_queue_entry_t*> sched_queue[FU_TYPES];
std::deque<proc_inst_t*> dispatch_queue;
std::vector<reg_file_entry_t> reg_file;
std::vector<common_data_bus_t> result_buses;
std::deque<sched_queue_entry_t*> fu_slots[FU_TYPES][FU_TYPES];
int32_t current_cycle, max_num_fu[FU_TYPES], max_rob_size, max_disp_size, fetch_width, max_sched_size[FU_TYPES];
bool fetch_done;

struct cycle_snapshot_t {
    int32_t reorder_buffer_size;
    int32_t sched_queue_size[FU_TYPES];
    int32_t dispatch_queue_size;
    int32_t num_idle_fu[FU_TYPES];
} cycle_snapshot;

/**
 * Prints the pipeline trace, showing the cycle at which each instruction reached every pipeline stage.
 */
void print_pipeline_trace() {
    printf("INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE\tRETIRE\n");
    for(auto& instr: instructions) {
        printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
            instr->id,
            instr->fetch_cycle,
            instr->disp_cycle,
            instr->sched_cycle,
            instr->exec_cycle,
            instr->state_cycle,
            instr->retire_cycle);
    }
    printf("\n");
}

/**
 * Subroutine for initializing the processor.
 *
 * @r ROB size
 * @k0 Number of k0 FUs
 * @k1 Number of k1 FUs
 * @k2 Number of k2 FUs
 * @f Number of instructions to fetch
 * @m Schedule queue multiplier
 */
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f, uint64_t m) {
    fetch_width = f;
    current_cycle = 0;
    max_num_fu[0] = k0;
    max_num_fu[1] = k1;
    max_num_fu[2] = k2;
    for(int i = 0; i < FU_TYPES; ++i) {
        for(int j = 0; j <= i; ++j)
            fu_slots[i][j].clear();
    }
    max_rob_size = max_disp_size = r;
    max_sched_size[0] = k0 * m;
    max_sched_size[1] = k1 * m;
    max_sched_size[2] = k2 * m;
    fetch_done = false;

    instructions.clear();
    reorder_buffer.clear();
    dispatch_queue.clear();
    for(int i = 0; i < FU_TYPES; ++i)
        sched_queue[i].clear();
    result_buses.clear();

    reg_file.clear();
    reg_file.resize(NUM_REG);
    for(int i = 0; i < NUM_REG; ++i) {
        reg_file[i].reg_id = i;
        reg_file[i].valid = true;
    }
}

void broadcast_all_cdb() {
    sort(result_buses.begin(), result_buses.end(),
        [&](const common_data_bus_t& l, const common_data_bus_t& r) {
            return l.tag < r.tag;
        });
    for(auto& bus: result_buses) {
        if(bus.dst_reg_id == -1) continue;
        // mark as valid/ready to compute in RS
        for(int i = 0; i < FU_TYPES; ++i) {
            for(auto& entry: sched_queue[i]) {
                for(int j = 0; j < 2; ++j) {
                    if(!entry->src_ready[j] && entry->src_tag[j] == bus.tag) {
                        entry->src_ready[j] = true;
                    }
                }
            }
        }
        // update reg file
        if(!reg_file[bus.dst_reg_id].valid && reg_file[bus.dst_reg_id].rob_tag == bus.tag) {
            reg_file[bus.dst_reg_id].valid = true;
        }
    }
    result_buses.clear();
}

void state_update_stage() {
    for(int i = 0; i < FU_TYPES; ++i) {
        for(auto& entry: sched_queue[i]) {
            if(entry->completed) {
                for(auto& rob_entry: reorder_buffer) {
                    if(rob_entry.tag == entry->instr->id) {
                        rob_entry.completed_cycle = current_cycle;
                        // std::cout << current_cycle << " STATE UPDATE " << rob_entry.tag << "\n";
                    }
                }
            }
        }
        auto& sq = sched_queue[i];
        sq.erase(
            std::remove_if(sq.begin(), sq.end(), [](const sched_queue_entry_t* e) {
                return e->completed;
            }),
            sq.end()
        );
    }
}

void exec_stage() {
    for(int i = 0; i < FU_TYPES; ++i) {
        while(!fu_slots[i][i].empty()) {
            sched_queue_entry_t *entry = fu_slots[i][i].front();
            entry->instr->state_cycle = current_cycle + 1;
            entry->completed = true;
            result_buses.push_back({entry->instr->id, entry->instr->dst_reg});
            fu_slots[i][i].pop_front();
        }
        for(int j = i; j >= 1; --j) {
            fu_slots[i][j] = fu_slots[i][j - 1];
        }
        fu_slots[i][0].clear();
    }
}

void sched_stage() {
    // cycle_snapshot_t update_snapshot = cycle_snapshot;
    for(int i = 0; i < FU_TYPES; ++i) {
        if((int32_t)fu_slots[i][0].size() == max_num_fu[i]) continue;
        for(auto& entry: sched_queue[i]) {
            if(!entry->fired && entry->src_ready[0] && entry->src_ready[1]) {
                entry->fired = true;
                entry->instr->exec_cycle = current_cycle + 1;
                // std::cout << current_cycle << " SCHED " << entry->instr->id << " TYPE " << i << "\n";
                fu_slots[i][0].push_back(entry);
                if((int32_t)fu_slots[i][0].size() == max_num_fu[i]) break;
            }
        }
    }
}

void disp_stage() {
    cycle_snapshot_t update_snapshot = cycle_snapshot;
    while(dispatch_queue.size()) {
        proc_inst_t *instr = dispatch_queue.front();
        int op_code = instr->op_code;
        
        if(update_snapshot.sched_queue_size[op_code] >= max_sched_size[op_code]
            || update_snapshot.reorder_buffer_size >= max_rob_size) {
            // stall the pipeline
            break;
        }

        ++update_snapshot.sched_queue_size[op_code];
        ++update_snapshot.reorder_buffer_size;

        /* dispatch to schedule */
        dispatch_queue.pop_front();
        sched_queue_entry_t *entry = new sched_queue_entry_t();
        entry->instr = instr;
        entry->fired = false;
        entry->completed = false;

        /* the case when register is marked ready when completes in the same cycle is not possible
           as they are broadcasted in the beginning of the next cycle */
        for(int i = 0; i < 2; ++i) {
            if(instr->src_regs[i] == -1 || reg_file[instr->src_regs[i]].valid) {
                entry->src_ready[i] = true;
            } else {
                entry->src_ready[i] = false;
                entry->src_tag[i] = reg_file[instr->src_regs[i]].rob_tag;
            }
        }
        sched_queue[op_code].push_back(entry);
        reorder_buffer.push_back({instr, instr->id, 0});

        /* rename register, mark as invalid */
        if(instr->dst_reg != -1) {
            reg_file[instr->dst_reg].rob_tag = instr->id;
            reg_file[instr->dst_reg].valid = false;
        }

        // std::cout << current_cycle << " DISPATCHED " << instr->id << " TYPE " << instr->op_code << "\n";
        /* the next cycle after it is dispatch, it reaches the schedule stage */
        instr->sched_cycle = current_cycle + 1;
    }
}

void fetch_stage(FILE* p_file) {
    if(fetch_done) return;

    int32_t fw = std::min(fetch_width, max_disp_size - (int32_t)dispatch_queue.size());
    for(int i = 0; i < fw; ++i) {
        proc_inst_t* instr = new proc_inst_t();

        int result = fscanf(p_file, "%x %d %d %d %d",
                            &instr->inst_addr,
                            &instr->op_code,
                            &instr->dst_reg,
                            &instr->src_regs[0],
                            &instr->src_regs[1]);
        
        if (result != 5) {
            delete instr;
            fetch_done = true;
            break;
        }

        instr->id = (uint32_t)instructions.size() + 1;
        instr->op_code = instr->op_code == -1? 0: instr->op_code;
        instr->fetch_cycle = current_cycle;
        // std::cout << current_cycle << " FETCHED " << instr->id << "\n";
        /* the next cycle after it is fetched, it reaches the dispatch queue */
        instr->disp_cycle = current_cycle + 1;
        instructions.push_back(instr);
        dispatch_queue.push_back(instr);
    }
}

void retire_stage() {
    for(int i = 0; i < fetch_width && reorder_buffer.size(); ++i) {
        if(reorder_buffer.front().completed_cycle != 0 && 
            reorder_buffer.front().completed_cycle < current_cycle) {
            // std::cout << current_cycle << " RETIRED " << reorder_buffer.front().instr->id << "\n";
            reorder_buffer.front().instr->retire_cycle = current_cycle;
            reorder_buffer.pop_front();
        } else break;
    }
}

bool sched_all_empty() {
    for(int i = 0; i < FU_TYPES; ++i) {
        if(sched_queue[i].size())
            return false;
    }
    return true;
}

bool fu_all_idle() {
    for(int i = 0; i < FU_TYPES; ++i) {
        for(int j = 0; j <= i; ++j) {
            if(fu_slots[i][j].size())
                return false;
        }
    }
    return true;
}

/**
 * Subroutine that simulates the processor.
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 *
 * @p_stats Pointer to the statistics structure
 * @p_file Pointer to the trace file
 */
void run_proc(proc_stats_t* p_stats, FILE* p_file) {
    while(true) {
        broadcast_all_cdb();

        ++current_cycle;
        cycle_snapshot.reorder_buffer_size = reorder_buffer.size();
        for(int i = 0; i < FU_TYPES; ++i) {
            cycle_snapshot.sched_queue_size[i] = sched_queue[i].size();
        }
        
        state_update_stage();
        exec_stage();
        sched_stage();
        disp_stage();
        fetch_stage(p_file);
        retire_stage();

        if(fetch_done && reorder_buffer.empty() && dispatch_queue.empty() &&
            sched_all_empty() && fu_all_idle())
            break;
    }
}

/**
 * Subroutine for cleaning up any outstanding instructions and calculating overall statistics
 * such as average IPC or branch prediction percentage
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_proc(proc_stats_t *p_stats) {
    p_stats->cycle_count = instructions.size()? instructions.back()->retire_cycle: 0;
    p_stats->retired_instruction = instructions.size();
    p_stats->avg_inst_retired = p_stats->cycle_count?
        (double)p_stats->retired_instruction / (double)p_stats->cycle_count: 0;
}
