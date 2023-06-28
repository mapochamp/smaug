#ifndef _CORE_SPM_STATUS_H_
#define _CORE_SPM_STATUS_H_
namespace smaug {

struct SpmStatus {
    SpmStatus() {
        sizeLeft = SmvBackend::SpadSize();
        availOffset = 0;
        output = false;
    }

    SpmStatus(uint32_t _sizeLeft) :
        sizeLeft{SmvBackend::SpadSize() - _sizeLeft},
        availOffset{_sizeLeft},
        output{false} {}

    SpmStatus(uint32_t _sizeLeft, bool _output) :
        sizeLeft{SmvBackend::SpadSize() - _sizeLeft},
        availOffset{_sizeLeft},
        output{_output} {}

    uint32_t availOffset;
    uint32_t sizeLeft;
    bool output; // are we an output spm or input spm?
};

} //namespace smaug
#endif
