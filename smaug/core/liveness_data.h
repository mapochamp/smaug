#ifndef _CORE_LIVENESS_H_
#define _CORE_LIVENESS_H_
#include <cstdint>
#include <list>
#include <string>

#include "smaug/core/tensor.h"

namespace smaug {

class LivenessData {
    public:
        LivenessData(std::string _name) : \
            name(_name) {}

        void update_liveness(uint32_t cycle);
        std::string get_access_time_string();

        std::list<uint32_t> get_access_times() const;
        uint32_t get_ttl() const;
        uint32_t get_ul() const;
        TensorShape get_shape();

    protected:
        std::string name;
        std::list<uint32_t> use_counts;
        TensorShape shape;

};

} //smaug
#endif
