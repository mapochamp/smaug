#include "smaug/core/liveness_data.h"
#include <algorithm>

namespace smaug {
void LivenessData::update_liveness(uint32_t cycle)
{
    use_counts.push_back(cycle);
}

std::vector<uint32_t> LivenessData::get_access_times() const
{
    return use_counts;
}

uint32_t LivenessData::get_start_time() {
    std::sort(use_counts.begin(), use_counts.end());
    auto min = std::min_element(use_counts.begin(), use_counts.end());
    return *min;
}

uint32_t LivenessData::get_end_time() {
    std::sort(use_counts.begin(), use_counts.end());
    auto max = std::max_element(use_counts.begin(), use_counts.end());
    return *max;
}

uint32_t LivenessData::get_ttl() const
{
    auto max = std::max_element(use_counts.begin(), use_counts.end());
    auto min = std::min(use_counts.begin(), use_counts.end());
    return *max - *min;
}

uint32_t LivenessData::get_ul() const
{
    return get_unused_liveness();
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
    this->sort();
    auto last = std::unique(use_counts.begin(), use_counts.end());
    use_counts.erase(last, use_counts.end());
}

void LivenessData::sort()
{
    std::sort(use_counts.begin(), use_counts.end());
}

double LivenessData::get_fomd()
{
    return get_unused_liveness() + get_memory_boundness() + get_impact();
}

uint32_t LivenessData::get_unused_liveness() const
{
    auto max = std::max_element(use_counts.begin(), use_counts.end());
    auto min = std::min(use_counts.begin(), use_counts.end());
    uint32_t liveness = use_counts.size();
    return *max - *min - liveness;
}

double LivenessData::get_memory_boundness()
{
    return 0;
}

double LivenessData::get_impact()
{
    return 0;
}

} //smaug
