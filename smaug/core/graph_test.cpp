#include "smaug/core/graph_test.h"
#include "static_graph_analyzer.h"

namespace smaug { 

Tensor* GraphTest::buildAndRunNetwork(const std::string& modelTopo,
                                      const std::string& modelParams) {
    buildNetwork(modelTopo, modelParams);
    //GraphAnalyzer analyzer(network_, workspace_);
    GraphAnalyzer analyzer(GraphTest::network_, GraphTest::workspace_);
    analyzer.compare_schedule_list();
}

}  // namespace smaug
