#pragma once

#include <dsp/audio_block.hpp>
#include <dsp/process_spec.hpp>

namespace dsp {

class Processor {
public:
    virtual ~Processor() = default;
    virtual bool prepare(const ProcessSpec& spec) = 0;
    virtual void reset() noexcept = 0;
    virtual void process(AudioBlock block) noexcept = 0;
};

} // namespace dsp
