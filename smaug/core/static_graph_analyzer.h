#ifndef _CORE_ANALYSIS_H_
#define _CORE_ANALYSIS_H_

#include <map>
#include <cstdint>

#include "smaug/core/scheduler.h"
#include "smaug/core/tensor.h"
#include "smaug/core/liveness_data.h"

namespace smaug {

/**
 * The graph analyzer will analyze the network graph to calculate the tensor
 * usage counts and time to live and label their FOMDs.
*/
class GraphAnalyzer : public Scheduler {
    public:
        std::map<Tensor, bool> findTensorsToPin();
        GraphAnalyzer(Network* _network, Workspace* _workspace) : Scheduler(_network, _workspace) {}

        void compare_schedule_list();
        void print_tensor_map();
        void dry_run_network();

    protected:
        void maybeRunOperator(Operator* op);
        void print_ops(std::list<Operator*> queue);
        void get_liveness_data();
        void update_liveness_map(TensorBase* tensor, uint32_t op_cycle);
        void remove_duplicate_cycles();

        std::map<Tensor, bool> tensorPinMap;
        std::map<TensorBase*, LivenessData> livenessMap;
        std::list<Operator*> network_build_queue;
};
} //smaug
#endif
