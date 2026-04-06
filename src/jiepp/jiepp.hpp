#pragma once
#include "option.hpp"
#include <iostream>

/**
 * @brief Execute jiepp preprocessing.
 *
 * @note This function uses global error state (Issue class) and is NOT thread-safe.
 *       Do not call from multiple threads concurrently.
 *
 * @param opts  Preprocessing options (input files, macros, limits, etc.).
 * @return Exit code: 0 on success, 1 on error.
 */
int jiepp_command(const JieppOptions& opts);
