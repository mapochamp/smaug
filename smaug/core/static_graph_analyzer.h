#ifndef _CORE_ANALYSIS_H_
#define _CORE_ANALYSIS_H_
#define SMV_SPAD_COUNT 3

#include <map>
#include <cstdint>

#include "smaug/core/scheduler.h"
#include "smaug/core/pin.h"
#include "smaug/core/tensor.h"
#include "smaug/core/liveness_data.h"
#include "smaug/core/backend.h"
#include "smaug/core/spm_status.h"

namespace smaug {

/**
 * The graph analyzer will analyze the network graph to calculate the tensor
 * usage counts and time to live and label their FOMDs.
*/

class GraphAnalyzer : public Scheduler {
    public:
        std::map<Tensor, bool> findTensorsToPin();
        GraphAnalyzer(Network* _network, Workspace* _workspace) : Scheduler(_network, _workspace) { spad_count = SMV_SPAD_COUNT;}
        GraphAnalyzer(Network* _network, Workspace* _workspace, int _spad_count) : Scheduler(_network, _workspace), spad_count{_spad_count} {}

        void compare_schedule_list();
        void create_ilp_map();
        void populate_pin_map();
        // creates a schedule of all the ops in the network wihtout executing
        // any of them so that we have a reference to the execution schedule
        // independant of the one that will actually run. (we don't change the
        // schedule at all)
        void dry_run_network();
        std::map<Operator*, std::vector<TensorBase*>>* get_pin_map();

    protected:
        // override from Scheduler class to not actually run any kernels so we
        // can dry run the network.
        void maybeRunOperator(Operator* op);

        // print all the operators in the schedule queue
        void print_ops(std::list<Operator*> queue);
        
        // get liveness data for each tensor
        void get_liveness_data();
        
        // update the livenessmap with a new entry or cycle count
        void update_liveness_map(TensorBase* tensor, uint32_t op_cycle);

        // remove duplicate cycles in the liveness map this is an artifact of
        // how we update the use_counts in LivenessData
        void remove_duplicate_cycles();

        // tensor to bool map. This will get updated by the runtime by
        // referencing the pin schedule
        void create_pin_map();
        void create_pin_map_v2();

        // Remove overlaps between competing pin cadidates. Do not remove
        // overlaps if competing tensors can all fit.
        void clean_ttl();

        // Validate whether the current list of pinned tensors will fit across
        // its entire lifetime.
        void validate_pin();
        void validate_pin_v2();


        // Clean up pinned tensors to enforce onsram rules where the pins must
        // fit on the spm their entire lifetime
        void enforce_onsram_lifetime(TensorBase* tensor);

        // get the current tensors that are pinned
        void get_current_pins();

        std::map<TensorBase*, double> sort(std::vector<TensorBase*> tensor_vec);

        std::map<TensorBase*, LivenessData*> livenessMap;
        std::list<Operator*> network_build_queue;
        int spad_count;
        std::map<std::string, SpmStatus> spm0Status;
        std::map<std::string, SpmStatus> spm1Status;
        std::map<std::string, SpmStatus> spm2Status;

        std::vector<std::vector<int>> spm0Map;
        std::vector<std::vector<int>> spm1Map;
        std::vector<std::vector<int>> spm2Map;
        std::vector<std::vector<std::vector<int>>> allSpmMatrix;
        std::map<uint32_t, std::vector<TensorBase*>> op_cycle_to_input_output_mapping;
        std::map<TensorBase*, uint32_t> uniqueTensorNumberMap;
};
} //smaug
#endif
