#include "smaug/core/graph_test.h"

namespace smaug { 

GraphAnalyzer GraphTest::buildAndRunNetwork(const std::string& modelTopo,
                                      const std::string& modelParams) {
    buildNetwork(modelTopo, modelParams);
    //GraphAnalyzer analyzer(network_, workspace_);
    GraphAnalyzer analyzer(GraphTest::network_, GraphTest::workspace_);
    return analyzer;
    //analyzer.compare_schedule_list();
}

}  // namespace smaug
