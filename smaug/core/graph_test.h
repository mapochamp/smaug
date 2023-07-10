#ifndef _CORE_GRAPH_TEST_H_
#define _CORE_GRAPH_TEST_H_

#include "smaug/core/smaug_test.h"
#include "static_graph_analyzer.h"

namespace smaug {
class GraphTest : public SmaugTest {
    public:
        Tensor* buildAndRunNetwork(const std::string& modelTopo,
                const std::string& modelParams,
                std::string map_path);

        GraphAnalyzer buildAnalyzer(const std::string& modelTopo,
                const std::string& modelParams);
};
}  // namespace smaug
#endif
