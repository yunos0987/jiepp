#pragma once

#include <cstddef>
#include <cstdint>

// Stack-exhaustion guard (PP63), modeled on clang's noteBottomOfStack() /
// isStackNearlyExhausted() (clang/lib/Basic/Stack.cpp). State is per thread;
// on a thread that never called note_stack_base() (library use, tests) every
// query reports "not exhausted".
namespace Util {

// Stack that must remain free at each check (clang's SufficientStack).
inline constexpr std::size_t STACK_HEADROOM = std::size_t{256} << 10;  // 256 KiB

// Approximate current stack pointer.
std::uintptr_t stack_pointer() noexcept;

// Records the caller's frame as the base of this thread's stack, with
// `budget` bytes usable from there. Call once, as close as possible to the
// start of the thread (main() or a worker-thread entry). budget == 0
// disables the guard on this thread.
void note_stack_base(std::size_t budget) noexcept;

// Forgets the recorded base on this thread (the guard becomes a no-op).
void reset_stack_base() noexcept;

// Budget recorded on this thread; 0 when none.
std::size_t stack_budget() noexcept;

// True when at most STACK_HEADROOM bytes of the budget remain.
bool stack_nearly_exhausted() noexcept;

} // namespace Util
