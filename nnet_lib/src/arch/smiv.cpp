#include <assert.h>
#include <math.h>
#include <string.h>

#include "gem5/m5ops.h"

#include "nnet_fwd.h"
#include "core/ref/activation_functions.h"
#include "core/ref/batch_norm.h"
#include "core/ref/convolution.h"
#include "core/ref/flatten.h"
#include "core/ref/matrix_multiply.h"
#include "core/ref/pooling.h"
#include "core/ref/zeropad.h"
#include "core/smiv/smiv.h"
#include "utility/utility.h"
#include "utility/profiling.h"
#include "utility/mkl/utility.h"
#include "arch/common.h"
#include "arch/interface.h"

#ifdef __cplusplus
#include "mkldnn.hpp"
#include "arch/nnet_mkl.h"
#include "core/mkl/activation_functions.h"
#endif

#ifdef DMA_MODE
#include "gem5_harness.h"
#endif

#if ARCHITECTURE == SMIV

// Convenience macro for profiling a kernel invocation.
//
// This macro assumes that the current scope contains:
//   - layer_t* layers: A pointer to the base layers array.
//   - int lnum: The current layer number.
#define INVOKE_KERNEL_PROF(req_code, kernel_ptr, args...)                      \
    do {                                                                       \
        begin_profiling(STRING(kernel_ptr), lnum);                             \
        INVOKE_KERNEL(req_code, kernel_ptr, args);                             \
        end_profiling();                                                       \
    } while (0)

// These are GLOBAL arrays which cannot be referenced directly by a HW
// function. Instead, pass them to the top level functions as function
// arguments, and use a boolean flag to indicate which one contains the data
// needed.
static float* g_umem;
static float* g_spad0;
static float* g_spad1;

// Each SMIV block has two scratchpads of 64KB each, but the real accelerator
// operates on 16-bit data, whereas we are using 32-bit data. To make sure we
// can fit the same size inputs, we double the per-scratchpad size.
#define SPAD_SIZE (131072)

// The UMEM on the NIC is 3 blocks of 1MB each.
#define UMEM_SIZE (3*1048576)

typedef struct _conv_cfg_t {
    // An array of dim_t objects. Specify the rows, cols, channels, and padding
    // for each iteration.
    dims_t* iteration;
    // Size of the array.
    unsigned num_iterations;
} conv_cfg_t;

// Use the same accelerator id for both the convolutional and FC blocks. This
// means we will simulate only ONE datapath instead of two, which means that
// the two blocks can share the scratchpads (without any more infrastructure
// changes). The key is that we still trace the functions at the _hw level, so
// that Aladdin will exit after simulating each block, and we can return
// control to the CPU at the right places.  In contrast, if we used two
// different ids, we would have two different datapaths that could not share
// data directly.
unsigned kConvolutionHw = 0x0003;
unsigned kInnerProductHw = 0x0003;
unsigned kReductionHw = 0x0003;
unsigned kBatchNormHw = 0x0003;

result_buf flatten_input(float* activations,
                         layer_t* layers,
                         int lnum,
                         float* result) {
    return flatten_input_rowmajor(activations, layers, lnum, result);
}


bool is_supported_activation_func(activation_type func) {
  switch (func) {
    case NO_ACTIVATION:
    case RELU:
    case RELU_THRESHOLD:
      return true;
    default:
      return false;
  }
}

void inner_product_layer_hw_impl(float* host_activations,
                                 float* host_weights,
                                 float* local_weights,
                                 float* local_inputs,
                                 float* results,
                                 layer_t* all_layers,
                                 int lnum) {
    activation_type act_func = all_layers[lnum].activation;
    bool run_activation = act_func == RELU || act_func == RELU_THRESHOLD;
    int weights_size = get_num_weights_layer(all_layers, lnum) * sizeof(float);
    setReadyBits(local_weights, UMEM_SIZE, 0);
    dmaLoad(local_weights, host_weights, weights_size);

    if (all_layers[lnum].input_req == IO_DMA) {
        grab_input_activations_dma(
                host_activations, local_inputs, &all_layers[lnum]);
    }

    matrix_multiply_with_bias_smiv(
            local_inputs,
            local_weights,
            NUM_TEST_CASES,
            all_layers[lnum].weights.rows,
            all_layers[lnum].weights.cols + all_layers[lnum].weights.align_pad,
            all_layers[lnum].inputs.align_pad,
            run_activation,
            results);
}

// HW accelerated inner product, using DMA for data movement.
//
// All arguments prefixed with host_ are host memory pointers and can only be
// deferenced from the host, except when performing a DMA operation.
void inner_product_layer_hw(float* host_activations,
                            float* host_weights,
                            float* umem,
                            float* spad0,
                            float* spad1,
                            layer_t* all_layers,
                            int lnum,
                            bool input_in_spad0,
                            float* host_result) {
    bool output_dma_req = (all_layers[lnum].output_req == IO_DMA);
    if (input_in_spad0) {
        inner_product_layer_hw_impl(host_activations,
                                    host_weights,
                                    umem,
                                    spad0,
                                    spad1,
                                    all_layers,
                                    lnum);
        if (output_dma_req)
            store_output_activations_dma(host_result, spad1, &all_layers[lnum]);
    } else {
        inner_product_layer_hw_impl(host_activations,
                                    host_weights,
                                    umem,
                                    spad1,
                                    spad0,
                                    all_layers,
                                    lnum);
        if (output_dma_req)
            store_output_activations_dma(host_result, spad0, &all_layers[lnum]);
    }
}

// HW accelerated inner product, using ACP for output data movement.
//
// All arguments prefixed with host_ are host memory pointers. acp_result is
// the host result pointer to be accessed over ACP.
void inner_product_layer_acp_hw(float* host_activations,
                                float* host_weights,
                                float* acp_result,
                                float* umem,
                                float* spad0,
                                float* spad1,
                                layer_t* all_layers,
                                int lnum,
                                bool input_in_spad0) {
    if (input_in_spad0) {
        inner_product_layer_hw_impl(host_activations,
                                    host_weights,
                                    umem,
                                    spad0,
                                    acp_result,
                                    all_layers,
                                    lnum);
    } else {
        inner_product_layer_hw_impl(host_activations,
                                    host_weights,
                                    umem,
                                    spad1,
                                    acp_result,
                                    all_layers,
                                    lnum);
    }
}

result_buf inner_product_layer(float* host_activations,
                               float* host_weights,
                               layer_t* layers,
                               int lnum,
                               float* host_result,
                               device_t* device) {
    static float* current_result_loc = NULL;
    if (current_result_loc == NULL) {
        current_result_loc = g_spad1;
    } else if (current_result_loc == g_spad0) {
        current_result_loc = g_spad1;
    } else if (current_result_loc == g_spad1) {
        current_result_loc = g_spad0;
    }
    float* host_weights_layer =
            host_weights + get_weights_loc_for_layer(layers, lnum);
    PRINT_MSG_V("Weights:\n");
    PRINT_DEBUG_V(host_weights_layer, layers[lnum].weights.rows,
                  layers[lnum].weights.cols,
                  layers[lnum].weights.cols + layers[lnum].weights.align_pad);

    size_t weight_bytes = WEIGHT_BYTES(layers, lnum);
    assert(weight_bytes <= UMEM_SIZE && "Weights must fit on the UMEM!");

    MAP_ARRAY(kInnerProductHw, host_activations, INPUT_BYTES(layers, lnum));
    MAP_ARRAY_TO_ACCEL(kInnerProductHw, "host_weights", host_weights_layer,
                       WEIGHT_BYTES(layers, lnum));

    // If the result is to be in g_spad1, then the input is in g_spad0.
    bool input_in_spad0 = (current_result_loc == g_spad1);
    bool use_acp_offload = (device->cpu_activation_func_offload == IO_ACP);
    if (use_acp_offload) {
        MAP_ARRAY_TO_ACCEL(kInnerProductHw, "acp_result", host_result,
                           OUTPUT_BYTES(layers, lnum));
        INVOKE_KERNEL_PROF(kInnerProductHw, inner_product_layer_acp_hw,
            host_activations, host_weights_layer, host_result, g_umem, g_spad0,
            g_spad1, layers, lnum, input_in_spad0);
    } else {
        MAP_ARRAY(kInnerProductHw, host_result, OUTPUT_BYTES(layers, lnum));
        INVOKE_KERNEL_PROF(kInnerProductHw, inner_product_layer_hw,
            host_activations, host_weights_layer, g_umem, g_spad0, g_spad1,
            layers, lnum, input_in_spad0, host_result);
    }

    return host_result;
}

void reduction_hw_impl(float* spad0,
                       float* spad1,
                       float* local_result,
                       bool input_in_spad0,
                       bool needs_input_load,
                       layer_t partial_layer,
                       size_t result_size,
                       float* host_result) {
    if (needs_input_load) {
        size_t input_bytes =
                result_size * partial_layer.inputs.height * sizeof(float);
        if (input_in_spad0) {
            setReadyBits(spad0, input_bytes, 0);
            dmaLoad(spad0, host_result, input_bytes);
        } else {
            setReadyBits(spad1, input_bytes, 0);
            dmaLoad(spad1, host_result, input_bytes);
        }
    }

    if (input_in_spad0)
        reduction_smiv(spad0, partial_layer, local_result);
    else
        reduction_smiv(spad1, partial_layer, local_result);
}

// Default Reduction module.
//
// Call this when we want to use DMA exclusively for moving data.
void reduction_hw(float* spad0,
                  float* spad1,
                  float* umem,
                  bool input_in_spad0,
                  bool needs_input_load,
                  layer_t partial_layer,
                  size_t result_size,
                  float* host_result) {
    reduction_hw_impl(spad0, spad1, umem, input_in_spad0, needs_input_load,
                      partial_layer, result_size, host_result);

    if (partial_layer.output_req == IO_DMA) {
        dmaStore(host_result, umem, result_size * sizeof(float));
    }
}

// ACP reduction module.
//
// The two scratchpads must remain named spad0 and spad1, but we use a
// different name for the result, called acp_result (instead of umem), to
// distinguish that the Aladdin config file should mark this array with acp.
//
// Importantly, acp_result is a pointer that corresponds to the host, unlike
// spad0/spad1/umem, which are accelerator-local memory.
void reduction_acp_hw(float* spad0,
                      float* spad1,
                      float* acp_result,
                      bool input_in_spad0,
                      bool needs_input_load,
                      layer_t partial_layer,
                      size_t result_size) {
    reduction_hw_impl(spad0, spad1, acp_result, input_in_spad0,
                      needs_input_load, partial_layer, result_size,
                      acp_result);
}

void convolution_layer_hw(float* host_activations,
                          float* host_weights,
                          float* umem,
                          float* spad0,
                          float* spad1,
                          bool input_in_spad0,
                          layer_t* all_layers,
                          layer_t partial_layer,
                          int layer_num,
                          int img,
                          int kern,
                          int start_chan) {
    layer_t curr_layer = all_layers[layer_num];
    const int input_height = curr_layer.inputs.height;
    const int input_rows= curr_layer.inputs.rows;
    const int input_cols = curr_layer.inputs.cols;
    const int input_pad = curr_layer.inputs.align_pad;
    const int k_width = curr_layer.weights.cols;
    const int k_pad = curr_layer.weights.align_pad;

    ARRAY_4D(float, _a, host_activations, input_height, input_rows,
             input_cols + input_pad);
    ARRAY_4D(float, _kernels, host_weights, input_height, k_width,
             k_width + k_pad);

    // We should only DMA part of the weights.
    size_t num_weights =
            partial_layer.weights.rows * partial_layer.weights.height *
            (partial_layer.weights.cols + partial_layer.weights.align_pad);
    if (input_in_spad0) {
        setReadyBits(spad0, num_weights * sizeof(float), 0);
        dmaLoad(spad0, &_kernels[kern][start_chan][0][0],
                num_weights * sizeof(float));
    } else {
        setReadyBits(spad1, num_weights * sizeof(float), 0);
        dmaLoad(spad1, &_kernels[kern][start_chan][0][0],
                num_weights * sizeof(float));
    }
    if (partial_layer.input_req == IO_DMA) {
        size_t num_input_pixels =
                partial_layer.inputs.rows * partial_layer.inputs.height *
                (partial_layer.inputs.cols + partial_layer.inputs.align_pad);
        setReadyBits(umem, num_input_pixels * sizeof(float), 0);
        dmaLoad(umem, &_a[img][start_chan][0][0],
                num_input_pixels * sizeof(float));
    }

    if (input_in_spad0)
        convolution3d_smiv(umem, spad0, partial_layer, spad1);
    else
        convolution3d_smiv(umem, spad1, partial_layer, spad0);
}

// Find a good way to pack the convolution into the accelerator.
conv_cfg_t convolution_divide_work(layer_t* layers, int lnum) {
    conv_cfg_t conv_cfgs;
    unsigned total_input_bytes = INPUT_BYTES(layers, lnum) / NUM_TEST_CASES;
    // This is the unreduced output for a single output channel.
    unsigned total_output_bytes =
            layers[lnum].outputs.rows *
            (layers[lnum].outputs.cols + layers[lnum].outputs.align_pad) *
            layers[lnum].inputs.height * sizeof(float);
    if (total_input_bytes > UMEM_SIZE) {
        printf("A single input image exceeds the capacity of the UMEM, which "
               "is not supported!\n");
        assert(false);
    }
    if (total_output_bytes <= SPAD_SIZE) {
        PRINT_MSG_V("Entire input problem fits into the local memory.\n");
        conv_cfgs.iteration = (dims_t*)malloc(sizeof(dims_t));
        conv_cfgs.iteration[0].rows = layers[lnum].inputs.rows;
        conv_cfgs.iteration[0].cols = layers[lnum].inputs.cols;
        conv_cfgs.iteration[0].height = layers[lnum].inputs.height;
        conv_cfgs.iteration[0].align_pad =
                calc_padding(conv_cfgs.iteration[0].cols, DATA_ALIGNMENT);
        conv_cfgs.num_iterations = 1;
        return conv_cfgs;
    }

    // Divide the problem up per input channel.

    unsigned output_channel_size =
            layers[lnum].outputs.rows *
            (layers[lnum].outputs.cols + layers[lnum].outputs.align_pad) *
            sizeof(float);
    unsigned input_channels = layers[lnum].inputs.height;

    unsigned max_channels_per_iter = SPAD_SIZE / output_channel_size;
    if (max_channels_per_iter >= 2) {
        PRINT_MSG_V("We can fit at least 2 unreduced input channels at once.\n");
        conv_cfgs.num_iterations =
                ceil((float)input_channels / max_channels_per_iter);
        conv_cfgs.iteration =
                (dims_t*)malloc(conv_cfgs.num_iterations * sizeof(dims_t));
        unsigned total_channels = input_channels;
        for (unsigned i = 0; i < conv_cfgs.num_iterations; i++) {
            conv_cfgs.iteration[i].rows = layers[lnum].inputs.rows;
            conv_cfgs.iteration[i].cols = layers[lnum].inputs.cols;
            conv_cfgs.iteration[i].height =
                    min2(total_channels, max_channels_per_iter);
            conv_cfgs.iteration[i].align_pad =
                    calc_padding(conv_cfgs.iteration[i].cols, DATA_ALIGNMENT);
            total_channels -= max_channels_per_iter;
        }
        return conv_cfgs;
    }

    // We can't fit more than a single channel onto the accelerator, which
    // means we won't be able to reduce in the accelerator. So now we have to
    // start chopping up the image into blocks.

    assert(false && "Tiled input handling is not yet supported!\n");
    return conv_cfgs;
}

void convolution_runner(float* host_activations,
                        float* host_weights,
                        layer_t* layers,
                        int lnum,
                        float* host_result,
                        device_t* device) {

    layer_t curr_layer = layers[lnum];
    const int result_height = curr_layer.outputs.rows;
    const int result_width = curr_layer.outputs.cols;
    const int result_pad = curr_layer.outputs.align_pad;
    const int num_kerns = curr_layer.outputs.height;
    const int result_2d_size = result_height * (result_width + result_pad);
    ARRAY_4D(float, _result, host_result, num_kerns, result_height,
             result_width + result_pad);

#if DEBUG_LEVEL >= 1
    const int input_height = curr_layer.inputs.height;
    const int k_width = curr_layer.weights.cols;
    const int k_pad = curr_layer.weights.align_pad;
    ARRAY_4D(float, _kernels, host_weights, input_height, k_width,
             k_width + k_pad);
#endif

    conv_cfg_t conv_cfgs = convolution_divide_work(layers, lnum);
    PRINT_MSG_V("Number of iterations: %d\n", conv_cfgs.num_iterations);
    // temp_result stores the partially reduced results of each iteration.
    size_t temp_result_size =
            result_2d_size * conv_cfgs.num_iterations * sizeof(float);
    float* temp_result = (float*)malloc_aligned(temp_result_size);

    bool do_hw_activation = device->use_hw_activation_func &&
                            is_supported_activation_func(curr_layer.activation);
    bool use_acp_offload = (device->cpu_activation_func_offload == IO_ACP);
    for (int img = 0; img < NUM_TEST_CASES; img++) {
        for (int kern = 0; kern < num_kerns; kern++) {
            PRINT_MSG("Kernel %d\n", kern);
            PRINT_DEBUG4D(&_kernels[kern][0][0][0],
                          k_width,
                          k_width + k_pad,
                          input_height);
            unsigned start_chan = 0;
            float* result_loc = temp_result;
            for (unsigned iter = 0; iter < conv_cfgs.num_iterations; iter++) {
                PRINT_MSG("Iteration %d\n", iter);
                dims_t iter_cfg = conv_cfgs.iteration[iter];

                // Create a new layer description for this iteration.
                layer_t partial_layer = curr_layer;
                partial_layer.inputs.height = iter_cfg.height;
                partial_layer.outputs.height = iter_cfg.height;
                partial_layer.weights.height = iter_cfg.height;
                partial_layer.activation =
                        conv_cfgs.num_iterations > 1 || !do_hw_activation
                                ? NO_ACTIVATION
                                : curr_layer.activation;
                INVOKE_KERNEL_PROF(kConvolutionHw, convolution_layer_hw,
                                   host_activations, host_weights, g_umem,
                                   g_spad0, g_spad1, true, layers,
                                   partial_layer, lnum, img, kern, start_chan);

                // Reduce the results.
                //
                // If the activation function is supported in hardware, then run
                // the standard reduction function with DMA. If the act func is
                // not supported, then use the ACP reduction impl, except if
                // the user specified to use DMA anyways.
                if (do_hw_activation || !use_acp_offload) {
                    MAP_ARRAY_TO_ACCEL(kReductionHw, "host_result", result_loc,
                                       temp_result_size);
                    INVOKE_KERNEL_PROF(kReductionHw, reduction_hw, g_spad0,
                                       g_spad1, g_umem, false, false,
                                       partial_layer, result_2d_size,
                                       result_loc);
                } else {
                    MAP_ARRAY_TO_ACCEL(kReductionHw, "acp_result", result_loc,
                                       temp_result_size);
                    INVOKE_KERNEL_PROF(kReductionHw, reduction_acp_hw, g_spad0,
                                       g_spad1, result_loc, false, false,
                                       partial_layer, result_2d_size);
                }

                result_loc += result_2d_size;
                start_chan += iter_cfg.height;
            }

            // Finish off the reduction here.
            if (conv_cfgs.num_iterations > 1) {
                result_loc = temp_result;

                int result_iter =
                        ceil(result_2d_size * conv_cfgs.num_iterations /
                             (float)SPAD_SIZE);
                assert(result_iter <= 1 &&
                       "Only support 1 last iteration of reduction!");
                int num_result_chans = min2(
                        conv_cfgs.num_iterations, SPAD_SIZE / result_2d_size);

                // Create a new layer description for this iteration.
                layer_t partial_layer = curr_layer;
                partial_layer.inputs.height = num_result_chans;
                partial_layer.outputs.height = 1;
                for (int iter = 0; iter < result_iter; iter++) {
                    PRINT_MSG("Final reduction round %d\n", iter);
                    if (do_hw_activation || !use_acp_offload) {
                        MAP_ARRAY_TO_ACCEL(kReductionHw, "host_result",
                                           result_loc, temp_result_size);
                        INVOKE_KERNEL_PROF(kReductionHw, reduction_hw, g_spad0,
                                           g_spad1, g_umem, false, true,
                                           partial_layer, result_2d_size,
                                           result_loc);
                    } else {
                        MAP_ARRAY_TO_ACCEL(kReductionHw, "acp_result",
                                           result_loc, result_2d_size);
                        INVOKE_KERNEL_PROF(kReductionHw, reduction_acp_hw,
                                           g_spad0, g_spad1, result_loc, false,
                                           true, partial_layer, result_2d_size);
                    }
                    result_loc += result_2d_size;
                }
            }

            // If the HW doesn't support the activation function, don't run the
            // activation function yet - we'll run it all at once when we're
            // done with all the kernels.

            memcpy(&_result[img][kern][0][0], temp_result,
                   result_2d_size * sizeof(float));
        }
    }
    free(conv_cfgs.iteration);
    free(temp_result);
}

result_buf convolution_layer(float* activations,
                             float* weights,
                             layer_t* layers,
                             int lnum,
                             float* result,
                             device_t* device) {

    float* current_layer_weights =
            weights + get_weights_loc_for_layer(layers, lnum);
    MAP_ARRAY_TO_ACCEL(kConvolutionHw, "host_weights", current_layer_weights,
                       get_num_weights_layer(layers, lnum) * sizeof(float));
    layer_t curr_layer = layers[lnum];
    if (curr_layer.c_padding > 0) {
        // TODO: Replace this with a memcpy implementation.
        copy_zeropad(activations, layers, lnum, result);
        PRINT_MSG("After zeropadding:\n");
        PRINT_DEBUG4D(result,
                      curr_layer.inputs.rows,
                      curr_layer.inputs.cols + curr_layer.inputs.align_pad,
                      curr_layer.inputs.height);
        PRINT_DEBUG4D_V(weights, curr_layer.weights.rows,
                        curr_layer.weights.cols + curr_layer.weights.align_pad,
                        curr_layer.weights.height);
        convolution_runner(result, current_layer_weights, layers, lnum,
                           activations, device);

        return activations;
    }
    convolution_runner(
            activations, current_layer_weights, layers, lnum, result, device);
    return result;
}

// Software implementation. SMIV doesn't accelerate pooling.
result_buf pooling_layer(float* activations,
                         layer_t* layers,
                         int lnum,
                         float* result,
                         device_t* device) {
    layer_t curr_layer = layers[lnum];
    if (curr_layer.pool == MAX) {
        max_pooling(activations, result, layers[lnum]);
    } else {
        assert(false && "Unsupported pooling layer type!");
    }
    return result;
}

void batch_norm_layer_hw(float* host_activations,
                         float* host_weights,
                         float* host_result,
                         float* umem,
                         float* spad0,
                         float* spad1,
                         layer_t* all_layers,
                         int lnum) {
    // DMA in the weights (to UMEM)
    setReadyBits(umem, UMEM_SIZE, 0);
    dmaLoad(umem, host_weights, WEIGHT_BYTES(all_layers, lnum));

    // DMA in the inputs (to SPAD0)
    if (all_layers[lnum].input_req == IO_DMA) {
        grab_input_activations_dma(host_activations, spad0, &all_layers[lnum]);
    }

    // The main kernel
    batch_norm_fxp(spad0, umem, &all_layers[lnum], NUM_TEST_CASES, spad1);

    // DMA out the result (from SPAD1)
    store_output_activations_dma(host_result, spad1, &all_layers[lnum]);
}

result_buf batch_norm_layer(float* activations,
                            float* weights,
                            layer_t* layers,
                            int lnum,
                            float* result,
                            device_t* device) {
    float* current_layer_weights =
            weights + get_weights_loc_for_layer(layers, lnum);

    MAP_ARRAY_TO_ACCEL(kBatchNormHw, "host_activations", activations,
                       INPUT_BYTES(layers, lnum));
    MAP_ARRAY_TO_ACCEL(kBatchNormHw, "host_weights", current_layer_weights,
                       WEIGHT_BYTES(layers, lnum));
    MAP_ARRAY_TO_ACCEL(kBatchNormHw, "host_result", result,
                       OUTPUT_BYTES(layers, lnum));
    INVOKE_KERNEL_PROF(kBatchNormHw, batch_norm_layer_hw, activations,
                       current_layer_weights, result, g_umem, g_spad0, g_spad1,
                       layers, lnum);

    return result;
}

result_buf run_layer(float* activations,
                     float* weights,
                     layer_t* layers,
                     int layer_num,
                     float* result,
                     device_t* device) {
    begin_profiling("run_layer", layer_num);

    begin_profiling("run_layer_skip_activation_func", layer_num);
    result_buf result_loc = run_layer_skip_activation_func(
            activations, weights, layers, layer_num, result, device);
    end_profiling();

    activation_type act_func = layers[layer_num].activation;
    bool do_activation = act_func != NO_ACTIVATION;
    bool do_hw_activation = device->use_hw_activation_func &&
                            is_supported_activation_func(act_func);
    if (do_activation && !do_hw_activation) {
        int output_size = get_output_activations_size(&layers[layer_num]) /
                          NUM_TEST_CASES;
#ifdef __cplusplus
        if (result_loc == activations) {
            nnet_mkl::activation_fun(activations, NUM_TEST_CASES, output_size,
                                     act_func, result, device);
            result_loc = result;
        } else {
            nnet_mkl::activation_fun(result, NUM_TEST_CASES, output_size,
                                     act_func, activations, device);
            result_loc = activations;
        }
        nnet_mkl::MklSession* session = nnet_mkl::get_session(device);
        session->run();
        session->clear();
#else
        begin_profiling(ACTIVATION_TYPE_STR(act_func), layer_num);
        activation_fun(result_loc, output_size, act_func, sigmoid_table);
        end_profiling();
#endif
        PRINT_MSG("\nactivation function\n");
        PRINT_DEBUG4D(result_loc, layers[layer_num].outputs.rows,
                      layers[layer_num].outputs.cols +
                              layers[layer_num].outputs.align_pad,
                      layers[layer_num].outputs.height);
    }
    end_profiling();
    return result_loc;
}

// Set the dmaLoad/dmaStore required flags for each layer.
//
// Since SMIV can share scratchpads between the conv/fc blocks, we only need
// DMA if we need to send data back to the CPU.
void set_dma_requirements(network_t* network, device_t* device) {
    for (int layer_num = 0; layer_num < network->depth; layer_num++) {
        // The input layer is easy.
        if (layer_num == 0) {
            network->layers[layer_num].input_req = IO_NONE;
            network->layers[layer_num].output_req = IO_DMA;
            continue;
        }
#if DEBUG_LEVEL > 0
        // When debugging, if we don't DMA the results back, we won't be able
        // to see what's happening.
        network->layers[layer_num].output_req = IO_DMA;
#else
        // First, determine if we need to dma store the output.
        if (layer_num == network->depth - 1 ||
            // All these activation functions are unsupported.
            network->layers[layer_num].activation == LRELU ||
            network->layers[layer_num].activation == ELU ||
            network->layers[layer_num].activation == SELU ||
            network->layers[layer_num].activation == TANH ||
            network->layers[layer_num].activation == SIGMOID ||
            network->layers[layer_num].activation == SOFTMAX ||
            // If we disabled HW activation functions but an activation
            // function is necessary, we need to DMA.
            (!device->use_hw_activation_func &&
             network->layers[layer_num].activation != NO_ACTIVATION) ||
            network->layers[layer_num].type == POOLING ||
            // For now, conv layers also do not support local caching.
            network->layers[layer_num].type == CONV ||
            network->layers[layer_num + 1].type == POOLING ||
            network->layers[layer_num + 1].type == BATCH_NORM) {
            network->layers[layer_num].output_req = IO_DMA;
        } else {
            network->layers[layer_num].output_req = IO_NONE;
        }
        if(network->layers[layer_num].input_preprocessing == FLATTEN)
            network->layers[layer_num - 1].output_req = IO_DMA;
#endif
        // Whether we need to load the input on this layer is just whether we
        // had to store the outputs in the previous layer.
        network->layers[layer_num].input_req =
                network->layers[layer_num - 1].output_req;
    }

    for (int layer_num = 0; layer_num < network->depth; layer_num++) {
        printf("Layer %d: dmaLoad = %d, dmaStore = %d\n", layer_num,
               network->layers[layer_num].input_req,
               network->layers[layer_num].output_req);
    }

}

// Runs the forward pass of a neural network.
//
// This version loads weights on a per layer basis, and activations are
// ping-ponged between two buffers, activations and result.
void nnet_fwd(farray_t activations,
              farray_t weights,
              farray_t result,
              network_t network,
              device_t* device) {

    int l;
    layer_t curr_layer;

    g_umem = (float*)malloc_aligned(UMEM_SIZE);
    g_spad0 = (float*)malloc_aligned(SPAD_SIZE);
    g_spad1 = (float*)malloc_aligned(SPAD_SIZE);

#ifdef __cplusplus
    nnet_mkl::MklSession* session = new nnet_mkl::MklSession();
    device->session = (void*)session;
#endif

    // Alternate between reading from/writing to activations and result so we
    // can avoid copying matrices. The initial activations is obviously in
    // "activations", so that's where we start.
    result_buf result_loc = activations.d;

    if (PRINT_DATA_AND_WEIGHTS) {
        print_data_and_weights(activations.d, weights.d, network.layers[0]);
    }

    // FORMAT HERE IS H TIMES W, NOT W TIMES H!!!!!
    // SO EACH DATA POINT IS A ***ROW****

    l = 0;

    set_dma_requirements(&network, device);

    MAP_ARRAY_TO_ACCEL(kConvolutionHw, "host_activations", activations.d,
                       activations.size);

    //******************//
    //   PRIMARY LOOP   //
    //******************//

nnet_fwd_outer:
    for (l = 0; l < network.depth; l++) {
        curr_layer = network.layers[l];

        if (result_loc == result.d) {
            result_loc = run_layer(result.d, weights.d, network.layers, l,
                                   activations.d, device);
        } else {
            result_loc = run_layer(activations.d, weights.d, network.layers, l,
                                   result.d, device);
        }
    }

    network.layers[network.depth - 1].result_in_temp = (result_loc == result.d);

    if (result_loc == result.d)
        dmaStore(result.d, result.d, NUM_TEST_CASES * NUM_CLASSES * sizeof(float));
    else
        dmaStore(activations.d, activations.d,
                 NUM_TEST_CASES * NUM_CLASSES * sizeof(float));
    dmaStore(network.layers, network.layers, network.depth * sizeof(layer_t));

    free(g_umem);
    free(g_spad0);
    free(g_spad1);
}

#endif
