#ifndef _CORE_ANALYSIS_H_
#define _CORE_ANALYSIS_H_

#include <map>

#include "smaug/core/scheduler.h"
#include "smaug/core/tensor.h"

namespace smaug {

class LivenessData {
    LivenessData() {}
};
/**
 * The graph analyzer will analyze the network graph to calculate the tensor
 * usage counts and time to live and label their FOMDs.
*/
class GraphAnalyzer : public Scheduler {
    public:
        std::map<Tensor, bool> findTensorsToPin();
        GraphAnalyzer(Network* _network, Workspace* _workspace) : Scheduler(_network, _workspace) {}

        void compare_schedule_list();

    protected:
        void maybeRunOperator(Operator* op);
        void print_ops(std::list<Operator*> queue);
        void get_liveness_data();

        std::map<Tensor, bool> tensorPinMap;
        //std::map<TensorBase, LivenessData> livenessMap;
        std::list<Operator*> network_build_queue;
};
}
#endif
