#include "procsim.hpp"
#include <deque>
#include <list>
#include <vector>
#include <algorithm>

/**
 * Prints the pipeline trace, showing the cycle at which each instruction reached every pipeline stage.
 * XXX: You're responsible for completing this function
 */

uint64_t cycle;
uint64_t inst_id;

uint64_t R, K0, K1, K2, F, M;
uint64_t sq0_lim, sq1_lim, sq2_lim;

struct proc_inst_t* print_inst;
bool print_first;

struct RF_entry{
    bool valid;
    uint32_t tag;
};

std::vector<struct RF_entry> RF;

std::vector<std::vector<std::vector<proc_inst_t*>>> fu_latches;

std::list<proc_inst_t*> disp_queue;
std::list<proc_inst_t*> pending_queue;

std::vector<std::list<proc_inst_t*>> sched_queues;
std::deque<proc_inst_t*> rob;
std::vector<uint64_t> sq_lims;

bool fetch(FILE* p_file){
    for(uint64_t i = 0; i < F; i++){
        if(disp_queue.size() < R){
            proc_inst_t* inst = new proc_inst_t();
            uint32_t inst_addr;
            int32_t op_code, dest_reg, src1_reg, src2_reg;
            if(fscanf(p_file, "%x %d %d %d %d", &inst_addr, &op_code, &dest_reg, &src1_reg, &src2_reg) == 5){
                inst_id++;
                inst->id = inst_id;
                inst->inst_addr = inst_addr;
                inst->op_code = op_code;
                inst->dst_reg = dest_reg;
                inst->src_regs[0] = src1_reg;
                inst->src_regs[1] = src2_reg;
                inst->fetch_time = cycle;
                inst->disp_time = cycle+1;
                inst->sched_time = 0;
                inst->exec_time = 0;
                inst->update_time = 0;
                inst->retire_time = 0;
                inst->src_tag[0] = 0;
                inst->src_tag[1] = 0;
                inst->src_pending[0] = false;
                inst->src_pending[1] = false;
                inst->src_valid[0] = false;
                inst->src_valid[1] = false;
                inst->rob_updated = false;
                inst->rob_completed = false;
            }
            else{
                delete inst;
                return false;
            }
            disp_queue.push_back(inst);
        }
        else{
            break;
        }
    }
    return true;
}

void dispatch(){
    uint64_t cur_sched_count[3] = {sched_queues[0].size(), sched_queues[1].size(), sched_queues[2].size()};
    uint64_t tot_count = rob.size();
    while(!disp_queue.empty()){
        proc_inst_t* inst = disp_queue.front();
        int32_t opcode = inst->op_code;
        if(opcode == -1) opcode = 0;
        cur_sched_count[opcode]++;
        tot_count++;
        if(cur_sched_count[opcode] > sq_lims[opcode] || tot_count > R){
            break;
        }
        disp_queue.pop_front();
        pending_queue.push_back(inst);
    }
}

void schedule(){
    while(!pending_queue.empty()){
        proc_inst_t* inst = pending_queue.front();
        int32_t opcode = inst->op_code;
        if(opcode == -1) opcode = 0;
        inst->sched_time = cycle;
        pending_queue.pop_front();
        rob.push_back(inst);
        sched_queues[opcode].push_back(inst);
        int32_t target_reg = 0;
        for(int i = 0; i < 2; i++){
            target_reg = inst->src_regs[i];
            if(target_reg == -1){
                inst->src_valid[i] = true;
                inst->src_tag[i] = 0;
                continue;
            }
            if(RF[target_reg].valid){
                inst->src_valid[i] = true;
                inst->src_tag[i] = 0;
            }
            else{
                inst->src_valid[i] = false;
                inst->src_tag[i] = RF[target_reg].tag;
            }
        }
        target_reg = inst->dst_reg;
        if(target_reg != -1){
            RF[target_reg].valid = false;
            RF[target_reg].tag = inst->id;
        }
    }
}

void execute(){
    for(int32_t i = 0; i < 3; i++){
        for(auto it = sched_queues[i].begin(); it != sched_queues[i].end();){
            proc_inst_t* inst = *it;
            if(inst->exec_time > 0){
                it++;
                continue;
            }
            if(inst->src_valid[0] && inst->src_valid[1]){
                bool executed = false;
                for(uint64_t j = 0; j < fu_latches[i].size(); j++){
                    if(fu_latches[i][j][0] == NULL){
                        fu_latches[i][j][0] = inst;
                        inst->exec_time = cycle;
                        executed = true;
                        break;
                    }
                }
                if(!executed){
                    break;
                }
                it++;
            }
            else{
                it++;
            }
        }
    }

}

void update(){
    for(int i = 0; i < 3; i++){
        if(sched_queues[i].empty()) continue;
        for(auto it = sched_queues[i].begin(); it != sched_queues[i].end(); ){
            proc_inst_t* inst = *it;
            if(inst->update_time > 0 && inst->update_time < cycle){
                it = sched_queues[i].erase(it);
            }
            else{
                for(int h = 0; h < 2; h++){
                    if(inst->src_pending[h]){
                        inst->src_pending[h] = false;
                        inst->src_valid[h] = true;
                    }
                }
                it++;
            }
        }
    }

    std::vector<proc_inst_t*> sorting_list;

    for(int i = 0; i < 3; i++){
        for(uint64_t j = 0; j < fu_latches[i].size(); j++){
            uint64_t last_index = fu_latches[i][j].size()-1;
            if(fu_latches[i][j][last_index] != NULL){
                proc_inst_t* inst = fu_latches[i][j][last_index];
                sorting_list.push_back(inst);
            }
        }
    }

    std::sort(sorting_list.begin(), sorting_list.end(), [](proc_inst_t* a, proc_inst_t* b){
        return a->id < b->id;
    });

    for(proc_inst_t* inst : sorting_list){
        inst->update_time = cycle;
        inst->rob_updated = true;
        uint32_t target_tag = inst->id;
        int32_t target_reg = inst->dst_reg;
        if(target_reg != -1 && RF[target_reg].tag == inst->id){
            RF[target_reg].valid = true;
            RF[target_reg].tag = 0;
        }
        for(int k = 0; k < 3; k++){
            for(auto it = sched_queues[k].begin(); it != sched_queues[k].end(); ){
                proc_inst_t* iter = *it;
                for(int h = 0; h < 2; h++){
                    if(iter->src_tag[h] == target_tag){
                        iter->src_pending[h] = true;
                    }
                }
                it++;
            }
        }
    }
    for(int i = 0; i < 3; i++){
        for(uint64_t j = 0; j < fu_latches[i].size(); j++){
            if(i == 0){
                fu_latches[i][j][0] = NULL;
                continue;
            }
            uint64_t last_index = fu_latches[i][j].size()-1;
            for(int k = last_index - 1; k >= 0; k--){
                proc_inst_t* inst = fu_latches[i][j][k];
                fu_latches[i][j][k+1] = inst;
            }
            fu_latches[i][j][0] = NULL;
        }
    }
}

void retire(proc_stats_t* p_stats){
    uint64_t retire_num = 0;
    for(auto it = rob.begin(); it != rob.end(); ){
        if(retire_num >= F) break;
        proc_inst_t* temp = *it;
        if(temp->rob_completed){
            temp->retire_time = cycle;
            print_inst = temp;
            print_pipeline_trace();
            delete temp;
            it = rob.erase(it);
            retire_num++;
        }
        else break;
    }
    p_stats->retired_instruction += retire_num;
    for(auto it = rob.begin(); it != rob.end(); it++){
        proc_inst_t* temp = *it;
        if(temp->rob_updated){
            temp->rob_completed = true;
            temp->rob_updated = false;
        }
    }
}

void print_pipeline_trace() {
    if (cycle == 0){
        printf("\n");
        return;
    }
    if (print_first){
        printf("INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE\tRETIRE\n");
        print_first = false;
    }
    printf("%u\t%u\t%u\t%u\t%u\t%u\t%u\n", print_inst->id, print_inst->fetch_time, print_inst->disp_time, print_inst->sched_time, print_inst->exec_time, print_inst->update_time, print_inst->retire_time);
}

/**
 * Subroutine for initializing the processor. You may add and initialize any global or heap
 * variables as needed.
 * XXX: You're responsible for completing this routine
 *
 * @r ROB size
 * @k0 Number of k0 FUs
 * @k1 Number of k1 FUs
 * @k2 Number of k2 FUs
 * @f Number of instructions to fetch
 * @m Schedule queue multiplier
 */
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f, uint64_t m) {
    cycle = 0;
    inst_id = 0;
    R = r; K0 = k0; K1 = k1; K2 = k2; F = f; M = m;
    sq0_lim = m*k0;
    sq1_lim = m*k1;
    sq2_lim = m*k2;
    sq_lims = {sq0_lim, sq1_lim, sq2_lim};

    print_inst = NULL;
    print_first = true;

    disp_queue.clear();
    pending_queue.clear();
    rob.clear();

    RF.assign(32, {true, 0});
    sched_queues.assign(3, std::list<proc_inst_t*>());
    fu_latches.resize(3);
    fu_latches[0].assign(k0, std::vector<proc_inst_t*>(1, NULL));
    fu_latches[1].assign(k1, std::vector<proc_inst_t*>(2, NULL));
    fu_latches[2].assign(k2, std::vector<proc_inst_t*>(3, NULL));
}

/**
 * Subroutine that simulates the processor.
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 * @p_file Pointer to the trace file
 */
void run_proc(proc_stats_t* p_stats, FILE* p_file) {
    p_stats->retired_instruction = 0;
    p_stats->avg_inst_retired = 0;
    p_stats->cycle_count = 0;
    bool keep = true;
    while(keep || pending_queue.size() != 0 || disp_queue.size() != 0 || rob.size() != 0){
        cycle++;
        p_stats->cycle_count = cycle;
        update();
        execute();
        schedule();
        dispatch();
        keep = fetch(p_file);
        retire(p_stats);
        if(!keep){
            for(int i = 0; i < 3; i++){
                if(!sched_queues[i].empty()){
                    keep = true;
                    break;
                }
            }
        }
    }
}

/**
 * Subroutine for cleaning up any outstanding instructions and calculating overall statistics
 * such as average IPC or branch prediction percentage
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_proc(proc_stats_t *p_stats) {
    cycle = 0;
    p_stats->avg_inst_retired = (float)p_stats->retired_instruction / p_stats->cycle_count;
}
