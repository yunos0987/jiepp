#include "stack_guard.hpp"

namespace Util {

namespace {
thread_local std::uintptr_t stack_base_ = 0;
thread_local std::size_t stack_budget_ = 0;
} // namespace

std::uintptr_t stack_pointer() noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
#else
    volatile char on_stack = 0;
    return reinterpret_cast<std::uintptr_t>(&on_stack);
#endif
}

void note_stack_base(std::size_t budget) noexcept {
    stack_base_ = budget ? stack_pointer() : 0;
    stack_budget_ = budget;
}

void reset_stack_base() noexcept {
    stack_base_ = 0;
    stack_budget_ = 0;
}

std::size_t stack_budget() noexcept {
    return stack_budget_;
}

bool stack_nearly_exhausted() noexcept {
    if (stack_base_ == 0)
        return false;  // base never recorded: library use, do not guess
    const std::uintptr_t sp = stack_pointer();
    const std::size_t used = sp < stack_base_ ? stack_base_ - sp : sp - stack_base_;
    // A distance beyond the whole budget means a stack layout we do not
    // understand (e.g. a different stack); do not guess (as clang does).
    if (used > stack_budget_)
        return false;
    return used + STACK_HEADROOM >= stack_budget_;
}

} // namespace Util
