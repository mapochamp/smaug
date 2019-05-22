#include "operators/common.h"
#include "operators/ref/ref_activation_fun_op.h"

#ifdef __cplusplus
extern "C" {
#endif

// Top level function entry for activation functions.
void ref_activation_fun_nc(float* inputs,
                           float* results,
                           int inputs_size,
                           activation_type function,
                           activation_param_t params) {
    dmaLoad(inputs, inputs, inputs_size * sizeof(float));
    activation_fun(inputs, results, inputs_size, function, params);
    dmaStore(results, results, inputs_size * sizeof(float));
}

#ifdef __cplusplus
}  // extern "C"
#endif
