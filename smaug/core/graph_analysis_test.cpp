#include "catch.hpp"
#include "smaug/core/backend.h"
#include "smaug/core/tensor.h"
#include "smaug/core/graph_test.h"

using namespace smaug;

TEST_CASE_METHOD(GraphTest, "Graph tests", "[graph]") {
    std::string modelPath = "experiments/models/";

    SECTION("Schedule Queue Comparison") {
        auto analyzer = buildAndRunNetwork(modelPath + "minerva/minerva_smv_topo.pbtxt",
                                   modelPath + "minerva/minerva_smv_params.pb");
        analyzer.compare_schedule_list();
    }

    SECTION("Create Tensor Liveness Maps") {
        auto analyzer = buildAndRunNetwork(modelPath + "minerva/minerva_smv_topo.pbtxt",
                                   modelPath + "minerva/minerva_smv_params.pb");
        analyzer.dry_run_network();
        analyzer.print_tensor_map();
    }
}
