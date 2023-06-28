#ifndef _CORE_PIN_H_
#define _CORE_PIN_H_

#include <map>
#include <vector>
#include "smaug/core/tensor.h"
#include "smaug/core/backend.h"
#include "smaug/core/operator.h"

namespace smaug {

using opPinMap = std::map<Operator*, std::vector<TensorBase*>>;
using opTensorMap = std::map<std::string, std::vector<TensorBase*>>;
using spmId = uint8_t;
using spmOffset = uint32_t;

extern opPinMap tensorPinMap;
extern opTensorMap opToTensorMap;
extern std::map<TensorBase*, spmOffset> tensorOffsetMap;
extern std::map<TensorBase*, spmId> tensorSPMap;

class SPManager {
    public:
        SPManager();

       // check if inputs should be read -> return list of inputs to read
        std::vector<TensorBase*> getPinnedTensors(std::vector<TensorBase*>
                inputs, std::string opName);
       // whether we should pin the output
       bool saveOutput();
       // return which SPM the tensor is pinned on
       spmId getSpmId(TensorBase*);
       // get the offset the tensor starts at
       spmOffset getSpmOffset(TensorBase*);

    protected:
        // livenessMap;
        // spad & offset map -> map<Tensor, pair<spad, offset>>?
        // spad pointer tracker
        // heuristic to pick the input and output spads
};

}
#endif
