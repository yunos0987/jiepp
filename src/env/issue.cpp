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
    message_ = PlainTextMessage::instance();
}

void Issue::set_output(std::ostream& stream) {
    stream_ = &stream;
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
        message_.message(*stream_, severity, code, context, filepath(), lineno(), 0, loc);
        *stream_ << '\n';
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

Issue::LineGuard::LineGuard(int ln, std::optional<std::string> fp) {
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
