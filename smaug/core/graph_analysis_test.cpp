#include "catch.hpp"
#include "smaug/core/backend.h"
#include "smaug/core/tensor.h"
#include "smaug/core/graph_test.h"

using namespace smaug;

TEST_CASE_METHOD(GraphTest, "Graph tests", "[graph]") {
    std::string modelPath = "experiments/models/";

    /*
    SECTION("Schedule Queue Comparison") {
        std::cout << "==========================================================" << std::endl;
        std::cout << "===========Schedule Queue Comparison=======================" << std::endl;
        std::cout << "==========================================================" << std::endl;
        auto analyzer = buildAnalyzer(modelPath + "minerva/minerva_smv_topo.pbtxt",
                                   modelPath + "minerva/minerva_smv_params.pb");
        std::cout << "buildAnalyzer Success\n" << std::endl;
        analyzer.compare_schedule_list();
    }

    SECTION("Create Tensor Liveness Maps") {
        std::cout << "==========================================================" << std::endl;
        std::cout << "===========Create Tensor Liveness Maps====================" << std::endl;
        std::cout << "==========================================================" << std::endl;
        auto analyzer = buildAnalyzer(modelPath + "minerva/minerva_smv_topo.pbtxt",
                                   modelPath + "minerva/minerva_smv_params.pb");
        analyzer.dry_run_network();
        analyzer.print_tensor_map();
    }

    SECTION("Create tensor Pin Map") {
        std::cout << "==========================================================" << std::endl;
        std::cout << "===========Create Tensor Pin Maps====================" << std::endl;
        std::cout << "==========================================================" << std::endl;
        auto analyzer = buildAnalyzer(modelPath + "minerva/minerva_smv_topo.pbtxt",
                                   modelPath + "minerva/minerva_smv_params.pb");
        analyzer.dry_run_network();
        analyzer.print_pin_map();
    }
    */

    SECTION("Minerva network. 4 layers of FCs.") {
        std::cout << "==========================================================" << std::endl;
        std::cout << "===========Build and Run Minerva==========================" << std::endl;
        std::cout << "==========================================================" << std::endl;
        Tensor* output =
            buildAndRunNetwork(modelPath + "minerva/minerva_smv_topo.pbtxt",
                                   modelPath + "minerva/minerva_smv_params.pb");

        // The same network with the reference backend. This is used for
        // producing expected outputs.
        Tensor* refOutput =
                buildAndRunNetwork(modelPath + "minerva/minerva_ref_topo.pbtxt",
                                   modelPath + "minerva/minerva_ref_params.pb");

        // SMV outputs need to be converted into float32 before validations.
        verifyOutputs<float>(
                convertFp16ToFp32Tensor(output, workspace()), refOutput);
    }

}
