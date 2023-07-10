#include <google/protobuf/stubs/hash.h>
#include <iostream>
#include <fstream>

#include "backend.h"
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

// basically we're just getting all the occurances and calculating frequency of
// each op so that we know whether its worth pinning or not if theres multiple
// pinning candidates in the same cycle
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
        auto &pin_vec_string = opToTensorMap[op->getName()];
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
        pin_vec_string = pin_vec_copy;
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

void GraphAnalyzer::validate_pin_v2()
{
    int spad_count = SMV_SPAD_COUNT;
    // The backend here is hardcoded. This not ideal and should
    // be changed to be passed in as a param.
    int spad_size = SmvBackend::SpadSize();
    // we do spad_count - 1 since we don't want to write to the output spad
    int total_spad_size = spad_size * (spad_count -1);
    // list of tensors that are not in the input list for an op
    std::vector<TensorBase*> non_input_vector{};

    // TODO: add the correct pins to the appropriate spm and offset
    /*
     *
     * For each op in the ready queue:
     *   check if the op is a data op or not
     *      if(data op || reorder):
     *          kill any pins
     * 
     *   get pin list for the vector
     *   if the pin cand is an input
     *      if any of its inputs are the first time getting loaded; remove the pin from this particular instance
     *      if input doesn't fit on an entire spm for the entire duration of
     *      its lifetime; remove the pin from all occurances
     *  
     *   if the pin cand is NOT an input
     *      check whether it fits after the inputs are placed
     *          if it fits then check if it fits for its entire lifetime
     *              if it fits the entire lifetime (ie it fits with all the
     *              inputs together too) then keep it
     *
     *
     * 
     *
     */
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
         * NOTE, this only applies if we aren't enforcing onsram rules.
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

void GraphAnalyzer::create_pin_map_v2()
{
    // TODO: figure out whether we need to keep track of what "current input
    // spms" we have or not and if we do, where to implement and how to
    // integrate go backwards and create a list of things we want to pin
    //
    // is having an <inputName : queue<spm#, offset#, pin/notpin/toPin> enough
    // to infer where things can go? like when we finally decide to pin
    // something later on, we're gonna have to change all the subsequent things. jfc
    // 
    // ^^ ok we do need, so we need to make a function that basically will run
    // to configure all the spmIDs and lifetimes for each tensor on each spm

    // then while( going forwards )
    //
    //  assign your input and outputs to an spm and offset (should be all in
    //  spm::0,1 2 order and all offset 0
    //
    //
    // Sort the pin list in FOMd order
    //
    // Loop through the sorted pin list:
    //      Loop through the queue schedule:
    //          if the op is a reorder or data op, ignore
    //          if the output is the pin cand:
    //              check if pin cand fits on spm entire duration
    //                  start reassigning all the spmIDs and offsets to
    //                  accomodate for the pin

    std::vector<TensorBase*> pinCandidates;

    // we are going backwards through the schedule. For each node, its inputs
    // are an output of a previous node.  This means that the output of the
    // prev node can be pinned so that it can save the dma cost when using it
    // as an input for the current node. so add all inputs that are not reorder
    // or data ops. We don't do data or reorder ops becuase they don't actually
    // move anything to the spm. they are purely cpu based funcs.
    std::list<Operator*>::reverse_iterator prev_op = readyQueue.rbegin();;
    for(auto op = readyQueue.rbegin(); op != readyQueue.rend(); op++) {
        const std::string& name = (*op)->getName();
        // if its not a reorder or data op then look for pin candidates
        if((name.find("reorder") != std::string::npos) ||
                (name.find("data") != std::string::npos))
        {
            auto inputs = (*op)->getInputs();

            for(auto input : inputs) {
                pinCandidates.push_back(input);
            }
        }
    }

    // sort the pin list in FoM order (FoM order) just means order of
    // importance -> how often its used, the size of the tensor, etc
    auto sortedPinMap = this->sort(pinCandidates);

    // initalize all the inputs and outputs as 0 offset and full size available
    // for each spad since we assume everything is going to get overriden and
    // there is no pinning at this point
    int i = 0;
    for(auto op : readyQueue) {
        auto inputs = op->getInputs();
        auto outputs = op->getOutputs();
        auto name = op->getName();

        // we create a map with a seperate SpmStatus per node becuase we want
        // to create a timeline of spm size changes throughout the scheudle
        for(auto input : inputs) {
            auto input_size = input->getShape().storageSize() *
                input->getDataTypeSize();
            // TODO: make it so that spm status size left is 0 if the input
            // size is greater than the spm size.
            if(i == 0) {
                spm0Status[name] = SpmStatus(input_size);
            } else {
                spm1Status[name] = SpmStatus(input_size);
            }
            i++; 
            // we increment i becuase in this build version, i will only ever
            // increment up to 1 since there are only ever 2 inputs per op and
            // one output.
        }

        // TODO: make it so that spm status size left is 0 if the output
        // size is greater than the spm size.
        for(auto output : outputs) {
            auto output_size = output->getShape().storageSize() *
                output->getDataTypeSize();
            spm2Status[name] = SpmStatus(output_size, true);
        }
    }

    // Loop through the sorted pin list:
    //      Loop through the queue schedule:
    //          if the op is a reorder or data op, ignore
    //          if the output is the pin cand:
    //              check if pin cand fits on spm entire duration
    //                  start reassigning all the spmIDs and offsets to
    //                  accomodate for the pin
#if 0
    for(const auto& [tensor, fom] : sortedPinMap) {
        bool found_op = false;
        uint32_t cycle = 0;
        uint32_t tensor_end_time = livenessMap[tensor].get_end_time();
        for(auto op : readyQueue) {
            // 1. skip all the ops until we find the first op where our tensor
            // is the output
            if(!found_op) {
                auto outputs = op->getOutputs();
                auto res = std::find(outputs.begin(), outputs.end(), tensor);
                if(res != std::end(outputs)) {
                    found_op = true;
                    // check if it fits


                    // check if pin cand fits on spm entire duation

                    // start reassigning all the spmIDs and offsets to
                    // accomodate for the pin
                }
            } else if(found_op && cycle <= tensor_end_time){ 
                // all the ops after we found the node where the pinCandidate
                // was born
            } else {
                break;
            }
            cycle++;
        }
    }
#endif


}

void GraphAnalyzer::create_pin_map()
{
    // iterate backwards through the list
    std::list<Operator*>::reverse_iterator prev_op = readyQueue.rbegin();
    for(auto op = readyQueue.rbegin(); op != readyQueue.rend(); op++) {
        auto inputs = (*op)->getInputs(); auto outputs = (*op)->getOutputs();
        const std::string& name = (*op)->getName();
        std::cout << "Tensor pin map: updating op: " << (*op)->getName() << std::endl;
        for(const auto tensor : inputs) {
            tensorPinMap[*op].push_back(tensor);
            opToTensorMap[name].push_back(tensor);
            std::cout << "ADDING : " << tensorPinMap[*op].back()->getName() << std::endl;
            std::cout << "ADDING : " << opToTensorMap[name].back()->getName() << std::endl;
        }
        if(prev_op != op) { // if we are the last in the schedule, ignore
            const auto prev_pin_vec = tensorPinMap[*prev_op];
            auto &curr_pin_vec = tensorPinMap[*op];
            auto &curr_pin_vec_string = opToTensorMap[name];
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
                    curr_pin_vec_string.push_back(pinCand);
                    std::cout << "ADDING : " << tensorPinMap[*op].back()->getName() << std::endl;
                    std::cout << "ADDING : " << opToTensorMap[name].back()->getName() << std::endl;
                }
            }
        }
        prev_op = op;
    }
}

void GraphAnalyzer::create_ilp_map(std::string map_path)
{
    uint32_t op_cycle{};
    uint32_t tensorNum{};
    // create a dict of all the inputs and outputs per op for a schedule dict
    // will have the op_cycle_idx as key and value of input0, input1, output0
    // there is no op_name mapping for the idx. we just assume that the
    // schedule is determinisitic and static so we can retreive it in another
    // function if we want. also create mapping of tensor # to tensor. so like
    // some unique tensor name should be mapped to a unique tensor #.
    for(auto op : readyQueue) {
        std::vector<TensorBase*> inputs = op->getInputs();
        std::vector<TensorBase*> outputs = op->getOutputs();
        // create a temp vector for the inputs and outputs
        std::vector<TensorBase*> inputs_and_output{};
        // concat the inputs and outputs into the temp vec
        inputs_and_output.insert(inputs_and_output.end(),
                inputs.begin(), inputs.end());
        inputs_and_output.insert(inputs_and_output.end(),
                outputs.begin(), outputs.end());

        // add the concatted inputs and outputs vec to the map
        op_cycle_to_input_output_mapping.insert_or_assign(
                op_cycle, inputs_and_output);

        std::cout << "      operator: " << op->getName() << "...\n";
        for(auto tensor : inputs_and_output) {
            if(!uniqueTensorNumberMap.count(tensor)) {
                tensorSizeMap.insert_or_assign(tensorNum,
                        tensor->getShape().storageSize());
                uniqueTensorNumberMap.insert_or_assign(tensor, tensorNum);
                std::cout << "      tensor: " << tensor->getName() 
                    << " tensorNumber = " << tensorNum << "\n";
                tensorNum++;
            }
        }
        op_cycle++;
    }

    // now that we have a dict of intputs and outputs to a cycle map, we create
    // our spm tensor mappings
    
    // get # of tensors that exist to create tensor mapping matrix
    int num_tensors = uniqueTensorNumberMap.size();

    // op_cycle - 1 since the last time we incremet op_cycle at end of loop
    // when creating the op to tensor mapping
    std::cout << "======================================================\n";
    std::cout << "      Starting SPM mapping...\n";
    std::cout << "======================================================\n";
    // initialize spm_n_map to a vector of op_size so we don't segfault
    std::vector<int> tempTensors(num_tensors, 0);
    std::vector<std::vector<int>> tempSpm(op_cycle, tempTensors);
    spm0Map = tempSpm;
    spm1Map = tempSpm;
    spm2Map = tempSpm;
    for(int i = 0; i < op_cycle; i++) {
        std::vector<int> allTensors0(num_tensors, 0); // input0 spm
        std::vector<int> allTensors1(num_tensors, 0); // input0 spm
        std::vector<int> allTensors2(num_tensors, 0); //output spm
        if(op_cycle_to_input_output_mapping[i].size() < 3) {
            // reorder op or something where we don't populate all 3 spms
            // There might be a problem with the ILP solver where an input is
            // also the output and so theres some kind of interference thing 
            // there where it thinks it goes back to main mem and back to spm
            std::vector<std::vector<int>> temp_spm_map{allTensors0, allTensors2};
            for(int j = 0; j < 2; j++) {
                TensorBase* tensor = op_cycle_to_input_output_mapping[i][j];
                int tensorIdx = uniqueTensorNumberMap[tensor];
                temp_spm_map[j][tensorIdx] = 1;
            }
            spm0Map[i] = temp_spm_map[0];
            spm1Map[i] = allTensors1;
            spm2Map[i] = temp_spm_map[1];
        } else { // there are the proper amount of inputs and outputs
            std::vector<std::vector<int>> temp_spm_map{allTensors0, 
                                                       allTensors1,
                                                       allTensors2};
            for(int j = 0; j < 3; j++) {
                TensorBase* tensor = op_cycle_to_input_output_mapping[i][j];
                int tensorIdx = uniqueTensorNumberMap[tensor];
                temp_spm_map[j][tensorIdx] = 1;
            }
            spm0Map[i] = temp_spm_map[0];
            spm1Map[i] = temp_spm_map[1];
            spm2Map[i] = temp_spm_map[2];
        }
    }
    auto print_array = [](auto &array0, auto &array1, auto &array2) {
        for(int k = 0; k < array0.size(); k++) {
            //std::cout << "array[" << k << "] = " << array[k] << " ";
            std::cout << array0[k] << " ";
        }

        std:: cout << "     ";
        for(int k = 0; k < array1.size(); k++) {
            std::cout << array1[k] << " ";
        }

        std:: cout << "     ";
        for(int k = 0; k < array2.size(); k++) {
            std::cout << array2[k] << " ";
        }

        std::cout << "\n" << std::endl;
    };
    for(int i = 0; i < spm0Map.size(); i++){
        print_array(spm0Map[i], spm1Map[i], spm2Map[i]);
    }


    // TODO: create file of all tensor sizes
    // TODO: make sure if the tensor size is greater than the spm size then we make it the size of the spm
    // TODO: create file of spm sizes
    
    std::ofstream tensorSizeFile;
    std::string fileName = map_path + "sizeFile.txt";
    tensorSizeFile.open(fileName);
    for(int i = 0; i < num_tensors; i++) {
        uint32_t size = tensorSizeMap[i];
        if(size > smv::kSpadSize) {
            size = smv::kSpadSize;
        }
        tensorSizeFile << size << " ";
    }
    tensorSizeFile.close();

    std::vector<std::vector<std::vector<int>>> spmMaps{spm0Map, spm1Map, spm2Map};
    for(int i = 0; i < 3; i++) {
        std::ofstream matrixFile;
        std::string fileName = map_path + "matrixFile" + std::to_string(i) + ".txt";
        matrixFile.open(fileName);
        auto& array = spmMaps[i];
        for(auto &op_cycle : array) {
            for(auto &tensor : op_cycle) {
                matrixFile << tensor;
            matrixFile << " ";
            }
            matrixFile << "\n";
        }
        matrixFile.close();
    }
}

void GraphAnalyzer::populate_pin_map()
{
    std::cout << "======================================================\n";
    std::cout << "      Getting Liveness Data...\n";
    std::cout << "======================================================\n";
    this->get_liveness_data();
    std::cout << "======================================================\n";
    std::cout << "      Creating Pin Map ...\n";
    std::cout << "======================================================\n";
    this->create_pin_map();
    this->create_pin_map_v2();
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
    this->validate_pin_v2();

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
