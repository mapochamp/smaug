#include "catch.hpp"
#include "smaug/core/backend.h"
#include "smaug/core/tensor.h"
#include "smaug/core/graph_test.h"

using namespace smaug;

TEST_CASE_METHOD(GraphTest, "Network tests", "[network]") {
    std::string modelPath = "experiments/models/";

    SECTION("Schedule Queue Comparison") {
        //graph_test.buildAndRunNetwork(modelPath + "minerva/minerva_smv_topo.pbtxt",
        //                           modelPath + "minerva/minerva_smv_params.pb");
        //GraphTest graph_test;
        buildAndRunNetwork(modelPath + "minerva/minerva_smv_topo.pbtxt",
                                   modelPath + "minerva/minerva_smv_params.pb");
    }
}
