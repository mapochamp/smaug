#include "catch.hpp"
#include "smaug/core/backend.h"
#include "smaug/core/tensor.h"
#include "smaug/core/graph_test.h"
#include "smaug/core/smaug_test.h"

using namespace smaug;

TEST_CASE_METHOD(GraphTest, "Graph tests", "[graph]") {
    std::string modelPath = "experiments/models/";

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
        //analyzer.print_tensor_map();
    }

    /*
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
            GraphTest::buildAndRunNetwork(modelPath + "minerva/minerva_smv_topo.pbtxt",
                                   modelPath + "minerva/minerva_smv_params.pb",
                                   "spm_map/minerva/");

        // The same network with the reference backend. This is used for
        // producing expected outputs.
        Tensor* refOutput =
                SmaugTest::buildAndRunNetwork(modelPath + "minerva/minerva_ref_topo.pbtxt",
                                   modelPath + "minerva/minerva_ref_params.pb");

        // SMV outputs need to be converted into float32 before validations.
        verifyOutputs<float>(
                convertFp16ToFp32Tensor(output, workspace()), refOutput);
    }

    SECTION("CNN network") {
        std::cout << "==========================================================" << std::endl;
        std::cout << "===========Build and Run CNN==============================" << std::endl;
        std::cout << "==========================================================" << std::endl;
        Tensor* output =
            GraphTest::buildAndRunNetwork(modelPath + "cnn/cnn_smv_topo.pbtxt",
                                   modelPath + "cnn/cnn_smv_params.pb",
                                   "spm_map/cnn/");

        // The same network with the reference backend. This is used for
        // producing expected outputs.
        Tensor* refOutput =
                SmaugTest::buildAndRunNetwork(modelPath + "cnn/cnn_ref_topo.pbtxt",
                                   modelPath + "cnn/cnn_ref_params.pb");

        // SMV outputs need to be converted into float32 before validations.
        verifyOutputs<float>(
                convertFp16ToFp32Tensor(output, workspace()), refOutput);
    }

    SECTION("LSTM network") {
        std::cout << "==========================================================" << std::endl;
        std::cout << "===========Build and Run LSTM==============================" << std::endl;
        std::cout << "==========================================================" << std::endl;
        Tensor* output =
            GraphTest::buildAndRunNetwork(modelPath + "lstm/lstm_smv_topo.pbtxt",
                                   modelPath + "lstm/lstm_smv_params.pb",
                                   "spm_map/lstm/");

        // The same network with the reference backend. This is used for
        // producing expected outputs.
        Tensor* refOutput =
                SmaugTest::buildAndRunNetwork(modelPath + "lstm/lstm_ref_topo.pbtxt",
                                   modelPath + "lstm/lstm_ref_params.pb");

        // SMV outputs need to be converted into float32 before validations.
        verifyOutputs<float>(
                convertFp16ToFp32Tensor(output, workspace()), refOutput);
    }

    SECTION("elu network") {
        std::cout << "==========================================================" << std::endl;
        std::cout << "===========Build and Run elu==============================" << std::endl;
        std::cout << "==========================================================" << std::endl;
        Tensor* output =
            GraphTest::buildAndRunNetwork(modelPath + "elu/elu_smv_topo.pbtxt",
                                   modelPath + "elu/elu_smv_params.pb",
                                   "spm_map/elu/");

        // The same network with the reference backend. This is used for
        // producing expected outputs.
        Tensor* refOutput =
                SmaugTest::buildAndRunNetwork(modelPath + "elu/elu_ref_topo.pbtxt",
                                   modelPath + "elu/elu_ref_params.pb");

        // SMV outputs need to be converted into float32 before validations.
        verifyOutputs<float>(
                convertFp16ToFp32Tensor(output, workspace()), refOutput);
    }

    SECTION("large elu network") {
        std::cout << "==========================================================" << std::endl;
        std::cout << "===========Build and Run Larget Elu==============================" << std::endl;
        std::cout << "==========================================================" << std::endl;
        Tensor* output =
            GraphTest::buildAndRunNetwork(modelPath + "large_elu/large_elu_smv_topo.pbtxt",
                                   modelPath + "large_elu/large_elu_smv_params.pb",
                                   "spm_map/large_elu/");

        // The same network with the reference backend. This is used for
        // producing expected outputs.
        //Tensor* refOutput =
        //        SmaugTest::buildAndRunNetwork(modelPath + "large_elu/large_elu_ref_topo.pbtxt",
        //                           modelPath + "large_elu/large_elu_ref_params.pb");

        //// SMV outputs need to be converted into float32 before validations.
        //verifyOutputs<float>(
        //        convertFp16ToFp32Tensor(output, workspace()), refOutput);
    }

    SECTION("lenet5 network") {
        std::cout << "==========================================================" << std::endl;
        std::cout << "===========Build and Run Lenet5==============================" << std::endl;
        std::cout << "==========================================================" << std::endl;
        Tensor* output =
            GraphTest::buildAndRunNetwork(modelPath + "lenet5/lenet5_smv_topo.pbtxt",
                                   modelPath + "lenet5/lenet5_smv_params.pb",
                                   "spm_map/lenet5/");

        // The same network with the reference backend. This is used for
        // producing expected outputs.
        Tensor* refOutput =
                SmaugTest::buildAndRunNetwork(modelPath + "lenet5/lenet5_ref_topo.pbtxt",
                                   modelPath + "lenet5/lenet5_ref_params.pb");

        // SMV outputs need to be converted into float32 before validations.
        verifyOutputs<float>(
                convertFp16ToFp32Tensor(output, workspace()), refOutput);
    }


    SECTION("resnet network") {
        std::cout << "==========================================================" << std::endl;
        std::cout << "===========Build and Run Resnet==============================" << std::endl;
        std::cout << "==========================================================" << std::endl;
        Tensor* output =
            GraphTest::buildAndRunNetwork(modelPath + "resnet/resnet_smv_topo.pbtxt",
                                   modelPath + "resnet/resnet_smv_params.pb",
                                   "spm_map/resnet/");

        // The same network with the reference backend. This is used for
        // producing expected outputs.
        //Tensor* refOutput =
        //        SmaugTest::buildAndRunNetwork(modelPath + "resnet/resnet_ref_topo.pbtxt",
        //                           modelPath + "resnet/resnet_ref_params.pb");

        //// SMV outputs need to be converted into float32 before validations.
        //verifyOutputs<float>(
        //        convertFp16ToFp32Tensor(output, workspace()), refOutput);
    }

    SECTION("vgg network") {
        std::cout << "==========================================================" << std::endl;
        std::cout << "===========Build and Run VGG==============================" << std::endl;
        std::cout << "==========================================================" << std::endl;
        Tensor* output =
            GraphTest::buildAndRunNetwork(modelPath + "vgg/vgg_smv_topo.pbtxt",
                                   modelPath + "vgg/vgg_smv_params.pb",
                                   "spm_map/vgg/");

        // The same network with the reference backend. This is used for
        // producing expected outputs.
        Tensor* refOutput =
                SmaugTest::buildAndRunNetwork(modelPath + "vgg/vgg_ref_topo.pbtxt",
                                   modelPath + "vgg/vgg_ref_params.pb");

        // SMV outputs need to be converted into float32 before validations.
        verifyOutputs<float>(
                convertFp16ToFp32Tensor(output, workspace()), refOutput);
    }
}
