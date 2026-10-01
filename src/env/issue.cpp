#include "issue.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include "issue_message.hpp"
#include "env.hpp"

// ---------------------------------------------------------------------------
// Static state
// ---------------------------------------------------------------------------

// Default blockings: all SEVERE + ERROR codes throw
std::set<Issue::Code> make_default_blockings() {
    std::set<Issue::Code> s;
#define JIEPP_ISSUE_CODE(name, id, severity, message) \
    if (Issue::is_severe(Issue::Code::name) || Issue::is_error(Issue::Code::name)) \
        s.insert(Issue::Code::name);
#include "issue_codes.def"
#undef JIEPP_ISSUE_CODE
    return s;
}

std::ostream* Issue::stream_ = &std::cerr;
std::vector<Issue::LocationEntry> Issue::loc_stack_;
std::set<Issue::Code> Issue::ignorings_;
std::set<Issue::Code> Issue::blockings_ = make_default_blockings();
bool Issue::silent_ = false;
bool Issue::suppress_warnings_ = false;
bool Issue::werror_ = false;
bool Issue::cli_mode_ = false;
bool Issue::continue_mode_ = false;
int Issue::error_count_ = 0;
// Inert while continue_mode_ is false; ContinueMode always forces all
// SEVERE codes in, mirroring make_default_blockings()'s severe-forcing.
std::set<Issue::Code> Issue::continue_abort_codes_;
IssueMessage& Issue::message_ = PlainTextMessage::instance();

// ---------------------------------------------------------------------------
// Severity lookup
// ---------------------------------------------------------------------------

Issue::Severity Issue::severity_of(Code code) {
    switch (code) {
#define JIEPP_ISSUE_CODE(name, id, severity, message) \
    case Code::name: return Severity::severity;
#include "issue_codes.def"
#undef JIEPP_ISSUE_CODE
    }
    return Severity::SEVERE;
}

std::string_view Issue::codename(Code code) {
	switch (code) {
#define JIEPP_ISSUE_CODE(name, id, severity, message) \
    case Issue::Code::name: return #name;
#include "issue_codes.def"
#undef JIEPP_ISSUE_CODE
	}
	return "";
}

// ---------------------------------------------------------------------------
// Error implementation
// ---------------------------------------------------------------------------

void Issue::initialize(std::ostream& stream) {
    stream_ = &stream;
    ignorings_.clear();
    blockings_ = make_default_blockings();
    loc_stack_.clear();
    loc_stack_.push_back({1, "<unknown location>"}); // dummy entry to avoid empty stack checks in filepath()/lineno()
    silent_ = false;
    suppress_warnings_ = false;
    werror_ = false;
    cli_mode_ = false;
    continue_mode_ = false;
    error_count_ = 0;
    continue_abort_codes_.clear();
    message_ = PlainTextMessage::instance();
}

void Issue::set_output(std::ostream& stream) {
    stream_ = &stream;
}

void Issue::ensure_location_stack() {
    // B1: only seed the bottom-of-stack dummy when it is missing; unlike
    // initialize(), this must never reset ignorings_/blockings_/werror_/etc.,
    // since a library caller may call setup() more than once (one Env per
    // call) while relying on Issue state configured in between, and a CLI
    // process has already had initialize() push this same dummy entry from
    // main() before setup() ever runs, so this is a no-op there.
    if (loc_stack_.empty())
        loc_stack_.push_back({1, "<unknown location>"});
}

void Issue::push(LocationEntry loc) {
    loc_stack_.push_back(std::move(loc));
}

Issue::LocationEntry Issue::pop() {
    if (!loc_stack_.empty()) {
        auto entry = loc_stack_.back();
        loc_stack_.pop_back();
        return entry;
    }
    FATAL();
}

Issue::LocationEntry Issue::top() {
    if (!loc_stack_.empty()) {
        return loc_stack_.back();
    }
    FATAL();
}

std::string Issue::base_filepath() {
    if (!loc_stack_.empty()) {
        if (loc_stack_.size() >= 2)
            return loc_stack_[1].second;
        return loc_stack_.front().second;
    }
    FATAL();
}

void Issue::add_ignoring(Code code) {
    ignorings_.insert(code);
}

bool Issue::is_ignored(Code code) {
    return ignorings_.count(code) != 0;
}

void Issue::add_blocking(Code code) {
    blockings_.insert(code);
}

void Issue::remove_blocking(Code code) {
    // SEVERE codes cannot be removed from blockings
    if (is_severe(code))
        return;
    blockings_.erase(code);
}

bool Issue::is_blocked(Code code) {
    return blockings_.count(code) != 0;
}

void Issue::happen(Code code, std::string context, std::source_location loc) {
    // ignore list has no effect, but SEVERE always proceeds.
    if (is_ignored(code) && !is_severe(code))
        return;

    Severity severity = severity_of(code);

    // -Werror: promote WARNING to ERROR
    bool promoted = false;
    if (werror_ && is_warning(code))
        promoted = true;
    if (promoted)
        severity = Severity::ERROR;

    // -w: suppress warning output (unless promoted by -Werror)
    bool output_suppressed = suppress_warnings_ && is_warning(code) && !promoted;

    if (!silent_ && !output_suppressed && stream_) {
        // CLI-level code (jiepp_command()/main()/parse_args(), see CliMode)
        // raises diagnostics that are not tied to any source file while
        // loc_stack_ still holds only the initialize()-time dummy entry
        // (size() == 1); render those as CLI_LOCATION instead of the dummy's
        // "<unknown location>:N.0" form, which the string-input
        // preprocess()/preprocess_text() API keeps using unchanged.
        // C5/U2: also render as CLI_LOCATION when a real entry was pushed on
        // top of the dummy but still carries the dummy's own placeholder
        // filename -- e.g. with_fallback_line() (lexer_helpers.hpp), used by
        // a lexer diagnostic (like UNCLOSED_COMMENT) raised while tokenizing
        // a -D/-U operand at the CLI stage, pushes {ln, filepath()} which
        // just copies that placeholder forward. Without this, such a
        // diagnostic rendered as "<unknown location>:N.0: ..." instead of
        // "jiepp: ...", because loc_stack_.size() was already 2 by then.
        // B1 defensive: loc_stack_ should never be empty here (ensure_location_stack()
        // and initialize() both guarantee a bottom dummy entry), but if some future
        // caller manages to hit this with an empty stack, filepath()/lineno() calling
        // back into top() -> FATAL() -> fatal() -> happen() here would recurse
        // without bound (the original B1 stack overflow). Fall back to a literal
        // placeholder instead of calling filepath()/lineno() in that case.
        std::string loc_file;
        LineNo loc_line = 0;
        if (loc_stack_.empty()) {
            loc_file = "<no location>";
        } else {
            loc_file = (cli_mode_ && (loc_stack_.size() == 1 ||
                        filepath() == loc_stack_.front().second)) ? CLI_LOCATION : filepath();
            loc_line = lineno();
        }
        message_.message(*stream_, severity, code, context, loc_file, loc_line, 0, loc);
        *stream_ << '\n';
    }

    if (continue_mode_) {
        // E0: only a code in continue_abort_codes_ (SEVERE is always forced
        // in, see ContinueMode) stops processing. Every other code that
        // would otherwise have thrown -- a plain blocked ERROR, or a
        // -Werror-promoted WARNING judged by its own original code, not by
        // the fact that it was promoted -- is instead counted here and
        // swallowed, so jiepp_command can keep going past it. This check is
        // independent of blockings_/is_blocked(): continue_mode_ replaces
        // that decision entirely rather than layering on top of it (see the
        // ContinueMode doc comment in issue.hpp for why -- -Werror
        // promotion bypasses blockings_ already).
        bool should_throw = is_severe(code) || continue_abort_codes_.count(code) != 0;
        if (!should_throw && severity == Severity::ERROR)
            ++error_count_; // E2: counted regardless of --silent
        if (should_throw)
            throw Exception(code);
        return;
    }

    // SEVERE always throws; promoted warnings throw if ERROR is in blockings;
    // other codes throw if in blockings
    if (is_severe(code) \
        || promoted \
        || is_blocked(code))
        throw Exception(code);
}

void Issue::fatal(std::string context, std::source_location loc) {
    happen(Code::FATAL, std::move(context), loc);
    throw std::logic_error("unreachable");
}

// ---------------------------------------------------------------------------
// LineGuard
// ---------------------------------------------------------------------------

Issue::LineGuard::LineGuard(LineNo ln, std::optional<std::string> fp) {
    std::string filepath_str = fp.has_value() ? std::move(*fp) : Issue::filepath();
    Issue::push({ln, std::move(filepath_str)});
}

Issue::LineGuard::~LineGuard() {
    Issue::pop();
}

// ---------------------------------------------------------------------------
// Blocking / Ignoring RAII guards
// ---------------------------------------------------------------------------

Issue::Blocking::Blocking(std::set<Issue::Code> codes) : original_(blockings_) {
    blockings_ = std::move(codes);
    // Ensure all SEVERE codes remain blocked
#define JIEPP_ISSUE_CODE(name, id, severity, message) \
    if (is_severe(Code::name)) blockings_.insert(Code::name);
#include "issue_codes.def"
#undef JIEPP_ISSUE_CODE
}

Issue::Blocking::~Blocking() {
    blockings_ = std::move(original_);
}

Issue::Ignoring::Ignoring(std::set<Issue::Code> codes) : original_(ignorings_) {
    ignorings_ = std::move(codes);
}

Issue::Ignoring::~Ignoring() {
    ignorings_ = std::move(original_);
}

Issue::CliMode::CliMode() : original_(cli_mode_) {
    cli_mode_ = true;
}

Issue::CliMode::~CliMode() {
    cli_mode_ = original_;
}

Issue::ContinueMode::ContinueMode(std::set<Issue::Code> abort_codes)
    : original_continue_(continue_mode_), original_abort_(continue_abort_codes_) {
    continue_mode_ = true;
    continue_abort_codes_ = std::move(abort_codes);
    // Ensure all SEVERE codes remain in the abort set (mirrors Blocking).
#define JIEPP_ISSUE_CODE(name, id, severity, message) \
    if (is_severe(Code::name)) continue_abort_codes_.insert(Code::name);
#include "issue_codes.def"
#undef JIEPP_ISSUE_CODE
}

Issue::ContinueMode::~ContinueMode() {
    continue_mode_ = original_continue_;
    continue_abort_codes_ = std::move(original_abort_);
}
