#include "smaug/core/static_graph_analyzer.h"
#include "smaug/utility/debug_stream.h"

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

void GraphAnalyzer::get_liveness_data()
{
    for(auto op : readyQueue) {
        // iteratate through outputs
    }
}

} //smaug
