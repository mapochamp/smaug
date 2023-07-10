#include "smaug/core/graph_test.h"

namespace smaug {

Tensor* GraphTest::buildAndRunNetwork(const std::string& modelTopo,
                                      const std::string& modelParams,
                                      std::string map_path) {
    buildNetwork(modelTopo, modelParams);
    GraphAnalyzer analyzer(GraphTest::network_, GraphTest::workspace_);
    analyzer.dry_run_network();
    analyzer.create_ilp_map(map_path);
    //analyzer.populate_pin_map();
    return analyzer.runNetwork();
}

GraphAnalyzer GraphTest::buildAnalyzer(const std::string& modelTopo,
                                      const std::string& modelParams) {
    buildNetwork(modelTopo, modelParams);
    GraphAnalyzer analyzer(GraphTest::network_, GraphTest::workspace_);
    return analyzer;
}

}  // namespace smaug
