#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <vector>

int recursive_mapping(int operation_index,
                       const std::vector<std::vector<int>>& input_sizes,
                       const std::vector<int>& output_sizes,
                       std::unordered_map<int, std::vector<int>>& output_reuse,
                       std::unordered_map<int, std::pair<int, int>>& input_scratchpad_mapping,
                       std::unordered_map<int, int>& output_scratchpad_mapping,
                       int max_scratchpad_id, int scratchpad_size, int& max_pinned) {
  if (operation_index >= output_reuse.size()) {
    return true;
  }

  const std::vector<int> next_operation_indices = output_reuse[operation_index];
  int local_max_pinned = 0;

  for (int input1_scratchpad_id = 0; input1_scratchpad_id < max_scratchpad_id;
       ++input1_scratchpad_id) {
    for (int input2_scratchpad_id = 0; input2_scratchpad_id < max_scratchpad_id;
         ++input2_scratchpad_id) {
      if (input1_scratchpad_id == input2_scratchpad_id) {
        continue;
      }

      std::unordered_map<int, std::pair<int, int>> original_input_mapping = input_scratchpad_mapping;
      std::unordered_map<int, int> original_output_mapping = output_scratchpad_mapping;

      input_scratchpad_mapping[operation_index].first = input1_scratchpad_id;
      input_scratchpad_mapping[operation_index].second = input2_scratchpad_id;

      for (int next_operation_index : next_operation_indices) {
          // TODO: we need to change this to make it so that we set an
          // input_scratchpad_mapping instead of output_scratchpad_mapping for
          // each next_operation_index since we are reusing the output of the
          // current operation as an input for the next_opertion_index
          // TODO: we need to make it so that outpu_scratchpad_mapping[current_operation_index] and
          // intput_scratcpad_mapping[next_operation_index] are the same.
          // TODO: we need to make sure that output_scratchppad_mapping[next_operation_index] is something
          // other than the input_scratchpad_mapping[next_oepration_index]
        output_scratchpad_mapping[next_operation_index] =
            3 - input1_scratchpad_id - input2_scratchpad_id;


        bool size_constraint_satisfied = true;

        // get the size of our output
        int pinned_output_size = output_sizes[operation_index];
        // check that it fits with all the other outputs that get mapped to the same scratchpad
        for (const auto& item : output_scratchpad_mapping) {
          if (item.first < next_operation_index &&
              item.first > operation_index &&
              item.second == output_scratchpad_mapping[next_operation_index]) {
              if(pinned_output_size + output_sizes[item.first] > scratchpad_size) {
                  size_constraint_satisfied = false;
              }
          }
        }

        for (int i = operation_index;
             i < next_operation_index && size_constraint_satisfied; ++i) {
            int input_scratchpad_id = -1;
            int input_size = 0;
          if (input_scratchpad_mapping[i].first == output_scratchpad_mapping[next_operation_index]) {
                  input_scratchpad_id = input_scratchpad_mapping[i].first;
          } else if (input_scratchpad_mapping[i].second == output_scratchpad_mapping[next_operation_index]) {
                  input_scratchpad_id = input_scratchpad_mapping[i].second;
          }

          if (input_scratchpad_id != -1) {
              int input_size = input_sizes[i][input_scratchpad_id];
          }

          if (pinned_output_size + input_size > scratchpad_size) {
            size_constraint_satisfied = false;
          }
        }

        int pinned_from_recursive_call = 0;
        if (size_constraint_satisfied) {
          pinned_from_recursive_call = recursive_mapping(
              next_operation_index, input_sizes, output_sizes, output_reuse,
              input_scratchpad_mapping, output_scratchpad_mapping,
              max_scratchpad_id, scratchpad_size, max_pinned);
        }

        if(size_constraint_satisfied &&
           pinned_from_recursive_call + 1 > max_pinned) {
            max_pinned = pinned_from_recursive_call + 1;
            local_max_pinned = max_pinned;
        } else {
            input_scratchpad_mapping = original_input_mapping;
            output_scratchpad_mapping = original_output_mapping;
        }
      }
    }
  }

  return false;
}

std::vector<std::vector<int>> get_mapping_vector(
    std::unordered_map<int, int> output_spm_mapping,
    std::unordered_map<int, std::pair<int, int>> input_spm_mapping);

void initialize_mapping(
        std::unordered_map<int, std::pair<int, int>>& input_spm_mapping,
                        std::unordered_map<int, int>& output_spm_mapping,
                        const std::vector<std::vector<int>>& input_sizes) 
{	
    for (int i = 0; i < input_sizes.size(); i++) {
        input_spm_mapping[i] = std::pair<int, int>{0, 1};
    }
    for (int i = 0; i < input_sizes.size(); i++) {
        output_spm_mapping[i] = 2;
    }
}

std::vector<std::vector<int>> find_optimal_mapping(
    const std::vector<std::vector<int>>& input_sizes,
    const std::vector<int>& output_sizes,
    std::unordered_map<int, std::vector<int>>& output_reuse)
{
    std::unordered_map<int, std::pair<int, int>> input_spm_mapping;
    std::unordered_map<int, int> output_spm_mapping;
    initialize_mapping(input_spm_mapping, output_spm_mapping, input_sizes);

    int max_pinned_outputs = 0;
    int max_spm_id = 2;
    int max_pinned = 0;
    int scratchpad_size = SmvBackend::SpadSize();
    std::vector<std::vector<int>> optimal_mapping;

    for (int output_scratchpad_id = 0; output_scratchpad_id < max_spm_id;
         ++output_scratchpad_id) {
        output_spm_mapping[0] = output_scratchpad_id;

        if (recursive_mapping(0, input_sizes, output_sizes, output_reuse, input_spm_mapping,
                              output_spm_mapping, max_spm_id, scratchpad_size, max_pinned)) {
            int pinned_outputs = std::count_if(
                output_spm_mapping.begin(), output_spm_mapping.end(),
                [output_scratchpad_id](const std::pair<int, int>& item) {
                    return item.second == output_scratchpad_id;
                });

            if (pinned_outputs > max_pinned_outputs) {
                max_pinned_outputs = pinned_outputs;
                optimal_mapping =
                    get_mapping_vector(output_spm_mapping, input_spm_mapping);
            }
        }
    }

    return optimal_mapping;
}
