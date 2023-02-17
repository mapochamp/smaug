#include <google/protobuf/stubs/hash.h>
#include <iostream>

#include "smaug/core/static_graph_analyzer.h"
#include "smaug/utility/debug_stream.h"

namespace smaug {

auto print_pin = [](auto &node, auto& livenessMap) {
    std::cout << "OPERATOR: [" << node.first->getName() << "] = " << std::endl;
    std::vector<TensorBase*> pin_vec = node.second;
    for(TensorBase* tensor : pin_vec) {
        auto liveness = livenessMap[tensor];
        std::cout << tensor->getName()
            << " begin time: " << liveness->get_start_time()
            << " end time: " << liveness->get_end_time()
            << std::endl;
    }
};

// returns a pointer to the pin map
std::map<Operator*, std::vector<TensorBase*>>* GraphAnalyzer::get_pin_map()
{
    return &tensorPinMap;
}

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
        std::cout << "OPERATOR NAME: " << op->getName() << std::endl;
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
    std::cout << "[" << node.first << "] = " << node.second->get_access_time_string() << std::endl;
};

void GraphAnalyzer::update_liveness_map(TensorBase* tensor, uint32_t op_cycle)
{
    if(livenessMap.count(tensor)) {
        LivenessData* data = livenessMap.at(tensor);
        data->update_liveness(op_cycle);
    } else {
        livenessMap.insert_or_assign(tensor, new LivenessData(tensor->getName()));
        livenessMap.at(tensor)->update_liveness(op_cycle);
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

    this->remove_duplicate_cycles();
}

void GraphAnalyzer::clean_ttl()
{
    int cycle{};

    for(auto op : readyQueue) {
        std::cout << "Cleaning pins for Op: " << op->getName() << std::endl;
        auto &pin_vec = tensorPinMap[op];
        auto pin_vec_copy = pin_vec;

        for(auto tensor : pin_vec) {
            auto min = livenessMap[tensor]->get_start_time();
            auto max = livenessMap[tensor]->get_end_time();
            std::cout << "considering tensor : " << tensor->getName() << std::endl;
            std::cout << "begin: " << min << " end: " << max << std::endl;
            if(min > cycle || max < cycle) {
                std::cout << "removing tensor : " << tensor->getName() << std::endl;
                pin_vec_copy.erase(std::remove(pin_vec_copy.begin(), pin_vec_copy.end(), \
                            tensor), pin_vec_copy.end());
            }
            std::cout << "----------------------\n" << std::endl;
        }
        pin_vec = pin_vec_copy;
        cycle++;
    }
}

// sort all the tensors based on their FoM. std::map is sorted by default since
// its a red black tree
std::map<TensorBase*, double> GraphAnalyzer::sort(std::vector<TensorBase*> tensor_vec)
{
    std::map<TensorBase*, double> fom_map{};

    for(auto tensor : tensor_vec) {
        fom_map[tensor] = livenessMap[tensor]->get_fomd();
    }

    return fom_map;
}

void GraphAnalyzer::validate_pin()
{
    // The backend here is hardcoded. This not ideal and should
    // be changed to be passed in as a param.
    int spad_size = SmvBackend::SpadSize();
    // we do spad_count - 1 since we don't want to write to the output spad
    int total_spad_size = spad_size * (spad_count -1);
    // list of tensors that are not in the input list for an op
    std::vector<TensorBase*> non_input_vector{};

    // for each operator in the pin map, we want to go through and check that
    // each pinCandidate in the pinMap will actually fit and is needed

    for(const auto op : readyQueue) {
        std::cout << "Operator cycle: " << op->getName() << std::endl;
        auto &pin_vec = tensorPinMap[op];
        auto inputs = op->getInputs();
        int input_tensors_size{};
        int remaining_spad_space{};
        auto tensor_size = [](auto tensor_vec) {
            int size{};
            for(auto i : tensor_vec) {
                i->getShape().storageSize() * i->getDataTypeSize();
            }
            return size;
        };
        input_tensors_size = tensor_size(inputs);
        remaining_spad_space = total_spad_size - input_tensors_size;

        // TODO: for each iteration of the readyQueue, keep track of the cycle
        // and reference the livenessMap to see if the tensor in the pinmap
        // is alive before the current op.
        auto pin_vec_copy = pin_vec;
        for(auto tensor : pin_vec) {
            int tensor_size = tensor->getShape().storageSize() * tensor->getDataTypeSize();
            // if the tensor is larger than the spads then don't pin
            if(tensor_size > total_spad_size) {
                enforce_onsram_lifetime(tensor);
                //pin_vec_copy.erase(std::remove(pin_vec_copy.begin(), pin_vec_copy.end(), \
                //            tensor), pin_vec_copy.end());
            }
            // if the tensor is not an input and larger then the remaining
            // available space if the inputs were pinned, don't pin
            if(std::find(inputs.begin(), inputs.end(), tensor) == inputs.end()) {
                non_input_vector.push_back(tensor);
                if(tensor_size > remaining_spad_space) {
                    enforce_onsram_lifetime(tensor);
                    //pin_vec_copy.erase(std::remove(pin_vec_copy.begin(), pin_vec_copy.end(), \
                    //            tensor), pin_vec_copy.end());
                }
            }
        }
        pin_vec = pin_vec_copy;

        // for all the tensors that are left as pin candidates, all of them may not
        // be able to coexist. So sort by FOM and temporal priority and start
        // pruning.
        auto fom_map = this->sort(non_input_vector);
        for(auto [key, val] : fom_map) {
            std::cout << "about to validate: " << key->getName() << std::endl;
            int tensor_size = key->getShape().storageSize() * key->getDataTypeSize();
            std::cout << "remaining_spad_space: " << remaining_spad_space << std::endl;
            std::cout << "tensor: " << key->getName() << " storageSize: "  << \
                key->getShape().storageSize() << " DataTypeSize: " << \
                key->getDataTypeSize() << std::endl;

            if(tensor_size >= remaining_spad_space) {
                auto pos = std::find(pin_vec.begin(), pin_vec.end(), key);
                if(pos != pin_vec.end()) {
                    pin_vec.erase(pos);
                }
            } else {
                remaining_spad_space -= tensor_size;
            }
        }


        //TODO:IMPORTANT !!!!
        /*==================================================================|
         * =================================================================|
         * =================================================================|
         * =================================================================|
         * ===================IMPORTANT=====================================|
         * =================================================================|
         * =================================================================|
         * =================================================================|
         * =================================================================|
         * =================================================================|
         * So basically, right now if we prune stuff, they might be pruned in a
         * wierd order where TTL's aren't crossing but shit has to get written
         * back, they might be "pinned" later so we have to go grab it again. So
         * we can either follow the ONSRAM thing and only keep shit that stays
         * alive and fits its entire liveness duration or we can do it our way
         * where we create some runtime shit to actually figure out how to do the
         * writebacks and repinning. But basically TTL overlap is an issue now.
         *
         * Start with the OnSRAM versio first and come back to do our way. Maybe
         * it'll be better if we figure out some cool algorithm to be less greedy
         * and minimize pin writebacks => so if something is being used alot but
         * gets written back bc its not an input for like 2/7 times its being used,
         * we should still pin it just becuase it's not gonna stay pinned its
         * entire TTL.
         */
    }

}

// Remove all instances of the given tensor from the pin map
void GraphAnalyzer::enforce_onsram_lifetime(TensorBase* tensor)
{
    for(auto &node : tensorPinMap) {
        auto& pin_vec = node.second;
        auto pin_vec_copy = pin_vec;

        if(std::find(pin_vec_copy.begin(), pin_vec_copy.end(), tensor) != pin_vec_copy.end()) {
            pin_vec_copy.erase(std::remove(pin_vec_copy.begin(), pin_vec_copy.end(), \
                        tensor), pin_vec_copy.end());
        }
        pin_vec = pin_vec_copy;
    }
}

void GraphAnalyzer::create_pin_map()
{
    // iterate backwards through the list
    std::list<Operator*>::reverse_iterator prev_op = readyQueue.rbegin();;
    for(auto op = readyQueue.rbegin(); op != readyQueue.rend(); op++) {
        auto inputs = (*op)->getInputs();
        auto outputs = (*op)->getOutputs();
        std::cout << "Tensor pin map: updating op: " << (*op)->getName() << std::endl;
        for(const auto tensor : inputs) {
            tensorPinMap[*op].push_back(tensor);
            std::cout << "ADDING : " << tensorPinMap[*op].back()->getName() << std::endl;
        }
        if(prev_op != op) { // if we are the last in the schedule, ignore
            const auto prev_pin_vec = tensorPinMap[*prev_op];
            auto &curr_pin_vec = tensorPinMap[*op];
            for(const auto pinCand : prev_pin_vec)
            {
                // if there is something that the next op expects to be pinned,
                // we should have it pinned as well if we don't already eg. if
                // mat_mul_4 requires data_1 as input and we are currently
                // mat_mul_3 and data_1 is neither in our inputs or outputs, we
                // should pin data_1 and recursively update the previous ops
                // behind us that create data_1 to pin it
                if(std::find(curr_pin_vec.begin(), curr_pin_vec.end(),
                            pinCand) == curr_pin_vec.end()) {
                    curr_pin_vec.push_back(pinCand);
                    std::cout << "ADDING : " << tensorPinMap[*op].back()->getName() << std::endl;
                }
            }
        }
        prev_op = op;
    }
}

void GraphAnalyzer::print_pin_map()
{
    std::cout << "======================================================\n";
    std::cout << "      Getting Liveness Data...\n";
    std::cout << "======================================================\n";
    this->get_liveness_data();
    std::cout << "======================================================\n";
    std::cout << "      Creating Pin Map ...\n";
    std::cout << "======================================================\n";
    this->create_pin_map();
    for(auto &node : tensorPinMap)
    {
        print_pin(node, livenessMap);
    }
    std::cout << "======================================================\n";
    std::cout << "      Cleaning TTLs ...\n";
    std::cout << "======================================================\n";
    this->clean_ttl();
    std::cout << "======================================================\n";
    std::cout << "      Validating Pin Map ...\n";
    std::cout << "======================================================\n";
    this->validate_pin();

    for(auto op : readyQueue) {
        std::cout << "======================================================\n";
        std::cout << "OPERATOR NAME: " << op->getName() << std::endl;
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

        std::cout << "PINNED TENSORS" << std::endl;
        auto& pin_vec = tensorPinMap[op];
        for(auto tensor : pin_vec) {
            std::cout << tensor->getName() << std::endl;
        }
        std::cout << "======================================================\n\n";
    }
}

void GraphAnalyzer::create_pin_schedule()
{
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

void GraphAnalyzer::remove_duplicate_cycles()
{
    for(auto &node : livenessMap)
    {
        node.second->remove_duplicate_cycles();
    }
}

} //smaug
