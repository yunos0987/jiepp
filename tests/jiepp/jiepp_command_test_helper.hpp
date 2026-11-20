#pragma once
// jiepp_command_test_helper.hpp — Shared runner for the test_*_command.cpp
// files that call jiepp_command() on a small input written to a file.

#include "test_helper.hpp"
#include "jiepp/jiepp.hpp"
#include "jiepp/option.hpp"

// The result of one jiepp_command() run: its return code and the bytes of
// the output file (empty when no output file was written).
struct JieppCommandResult {
    int rc;
    std::string out;
};

// Writes `content` to the file `name` in the system temp directory, runs
// jiepp_command(opts) there with `name` as the only input and `name`.piec as
// the output file, and removes both files afterwards. `name` shows up in the
// line markers of the output, and each caller passes its own so that test
// processes running in parallel do not share a file.
inline JieppCommandResult run_jiepp_command_on(const std::string& content, JieppOptions opts,
                                               const std::string& name) {
    const fs::path dir = fs::temp_directory_path();
    CwdGuard cwd(dir);
    write_file(dir / name, content);
    const fs::path out = dir / (name + ".piec");
    opts.input_filepaths = {name};
    opts.output_filepath = out.generic_string();
    JieppCommandResult r;
    r.rc = jiepp_command(opts);
    r.out = read_file(out);
    std::error_code ec;
    fs::remove(dir / name, ec);
    fs::remove(out, ec);
    return r;
}
