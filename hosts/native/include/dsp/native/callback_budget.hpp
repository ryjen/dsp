#pragma once

#include <dsp/processor.hpp>

#include <cstddef>

namespace dsp::native {

enum class BudgetError {
    none,
    invalid_spec,
    invalid_frame_count,
    invalid_callback_count,
    prepare_failed,
};

struct CallbackBudgetReport {
    std::size_t callback_count{};
    double elapsed_seconds{};
    double average_seconds{};
    double deadline_seconds{};
    double average_deadline_fraction{};
};

struct CallbackBudgetResult {
    BudgetError error{BudgetError::none};
    CallbackBudgetReport report{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == BudgetError::none;
    }
};

[[nodiscard]] CallbackBudgetResult measure_callback_budget(
    Processor& processor,
    const ProcessSpec& spec,
    std::size_t frame_count,
    std::size_t callback_count);

} // namespace dsp::native
