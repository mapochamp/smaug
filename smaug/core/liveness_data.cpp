#include "smaug/core/liveness_data.h"

namespace smaug {
void LivenessData::update_liveness(uint32_t cycle)
{
    use_counts.push_back(cycle);
}

std::list<uint32_t> LivenessData::get_access_times() const
{
    return use_counts;
}

uint32_t LivenessData::get_ttl() const
{
}

uint32_t LivenessData::get_ul() const
{
}

std::string LivenessData::get_access_time_string()
{
    std::string access_string;
    for(const auto& count : use_counts) {
        access_string += std::to_string(count) + ", ";
    }

    return access_string;
}

void LivenessData::remove_duplicate_cycles()
{
    use_counts.unique();
}

} //smaug
