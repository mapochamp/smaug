#include "smaug/core/static_graph_analyzer.h"
#include "smaug/utility/debug_stream.h"
#include <google/protobuf/stubs/hash.h>
#include <iostream>

namespace smaug {

void GraphAnalyzer::compare_schedule_list()
{
    std::cout << "======================================================\n";
    std::cout << "      Analyzing network graph...\n";
    std::cout << "======================================================\n";

    for (auto nameOp : network->getOperators()) {
        Operator* op = nameOp.second;
        dout(0) << "Tiling " << op->getName() << " ("
                << OpType_Name(op->getOpType()) << ").\n";
        op->tile();
        network_build_queue.push_back(op);
    }

    for (auto nameOp : network->getOperators()) {
        Operator* op = nameOp.second;
        Vertex vertex = op->getVertex();
        int numPendingInputs = boost::in_degree(vertex, network->getGraph());
        op->setNumPendingInputs(numPendingInputs);
        if (numPendingInputs == 0)
            readyQueue.push_back(op);
    }

    scheduleReady();

    std::cout << "======================================================\n";
    std::cout << "      Dumping network build queue...\n";
    std::cout << "======================================================\n";

    print_ops(network_build_queue);

    std::cout << "======================================================\n";
    std::cout << "      Analyzing network graph...\n";
    std::cout << "======================================================\n";

    print_ops(readyQueue);
}

void GraphAnalyzer::print_ops(std::list<Operator*> queue)
{
    for(auto op : queue) {
        std::cout << "TENSOR NAME: " << op->getName() << std::endl;
        auto tensor_vector = op->getInputs();
        std::cout << "TENSOR INPUTS: " << std::endl;
        for(auto tensor : tensor_vector) {
            std::cout << tensor->getName() << std::endl;
        }

        std::cout << "TENSOR OUTPUTS: " << std::endl;
        tensor_vector = op->getOutputs();
        for(auto tensor : tensor_vector) {
            std::cout << tensor->getName() << std::endl;
        }
    }
}

void GraphAnalyzer::maybeRunOperator(Operator* op) {
    if (op->isDead()) {
        for (auto output : op->getOutputs())
            output->setDead();
    }
}

auto print_node = [](auto &node) {
    std::cout << "[" << node.first << "] = " << node.second.get_access_time_string() << std::endl;
};

void GraphAnalyzer::update_liveness_map(TensorBase* tensor, uint32_t op_cycle)
{
    if(livenessMap.count(tensor)) {
        LivenessData& data = livenessMap.at(tensor);
        data.update_liveness(op_cycle);
    } else {
        livenessMap.insert_or_assign(tensor, LivenessData(tensor->getName()));
        livenessMap.at(tensor).update_liveness(op_cycle);
    }
}

void GraphAnalyzer::get_liveness_data()
{
    uint32_t op_cycle{};
    for(auto op : readyQueue) {
        std::vector<TensorBase*> inputs = op->getInputs();
        for(TensorBase* input : inputs) {
            std::cout << "UPDATING OP MAP " << op->getName() <<  " WITH INPUT : " << input->getName();
            std::cout << " AT CYCLE: " << op_cycle << std::endl;
            update_liveness_map(input, op_cycle);
        }

        // iteratate through outputs and add to liveness map
        std::vector<TensorBase*> outputs = op->getOutputs();
        for(TensorBase* output : outputs) {
            std::cout << "UPDATING OP MAP " << op->getName() << " WITH OUTPUT : " << output->getName();
            std::cout << " AT CYCLE: " << op_cycle << std::endl;;
            update_liveness_map(output, op_cycle);
        }

        op_cycle++;
    }
}

void GraphAnalyzer::print_tensor_map()
{
    std::cout << "======================================================\n";
    std::cout << "      Getting Liveness Data...\n";
    std::cout << "======================================================\n";

    this->get_liveness_data();
    for(auto &node : livenessMap)
    {
        print_node(node);
    }
}

void GraphAnalyzer::dry_run_network()
{
    std::cout << "======================================================\n";
    std::cout << "      Tiling operators of the network...\n";
    std::cout << "======================================================\n";
    for (auto nameOp : network->getOperators()) {
        Operator* op = nameOp.second;
        dout(0) << "Tiling " << op->getName() << " ("
                << OpType_Name(op->getOpType()) << ").\n";
        op->tile();
    }

    std::cout << "======================================================\n";
    std::cout << "      Scheduling operators of the network...\n";
    std::cout << "======================================================\n";
    // Initialize number of pending inputs for every operator and put Data
    // operators into the ready queue.
    for (auto nameOp : network->getOperators()) {
        Operator* op = nameOp.second;
        Vertex vertex = op->getVertex();
        int numPendingInputs = boost::in_degree(vertex, network->getGraph());
        op->setNumPendingInputs(numPendingInputs);
        if (numPendingInputs == 0)
            readyQueue.push_back(op);
    }

    scheduleReady();
}

} //smaug
