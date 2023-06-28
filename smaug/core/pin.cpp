#include "smaug/core/pin.h"

namespace smaug {

opPinMap tensorPinMap{};
opTensorMap opToTensorMap{};
std::map<TensorBase*, spmOffset> tensorOffsetMap{};
std::map<TensorBase*, spmId> tensorSPMap{};

SPManager::SPManager()
{
    for(const auto&[opPointer, pinVec] : tensorPinMap) {
        opToTensorMap[opPointer->getName()] = pinVec;
    }
}

// only checks against the inputs of the op so we can
// determine how to invoke our kernel
std::vector<TensorBase*> SPManager::getPinnedTensors(std::vector<TensorBase*>
        inputs, std::string opName)
{
    auto& pinnedTensors = opToTensorMap[opName];
    std::vector<TensorBase*> v_intersection;
    // return tensors that match
    std::set_intersection(inputs.begin(), inputs.end(),
            pinnedTensors.begin(), pinnedTensors.end(),
            std::back_inserter(v_intersection));

    return v_intersection;
}

bool SPManager::saveOutput()
{
}

spmId SPManager::getSpmId(TensorBase*)
{
}

spmOffset SPManager::getSpmOffset(TensorBase*)
{
}

}  // namespace smaug
