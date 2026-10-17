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
#ifdef JIEPP_SANDBOX
#include "../util/text.hpp"
#endif

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
std::set<Issue::Code> Issue::enabled_;
std::set<Issue::Code> Issue::werror_codes_;
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
    enabled_.clear();
    werror_codes_.clear();
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

void Issue::set_top_lineno(LineNo ln) {
    // B1 defensive: mirror ensure_location_stack()'s dummy entry if somehow
    // called before any entry exists; the normal caller (preprocess(), after
    // setup()'s Issue::ensure_location_stack() call) always has one.
    if (loc_stack_.empty()) {
        loc_stack_.push_back({ln, "<unknown location>"});
        return;
    }
    loc_stack_.back().first = ln;
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

// PP10-PP14 (FILE_ERROR, FILE_NOT_FOUND, MAX_INCLUDE_DEPTH_EXCEEDED,
// INVALID_COMMAND, INCLUDE_TARGET_IS_DIRECTORY) cannot be suppressed by
// {#ignore} / add_ignoring(), same as SEVERE (SPEC 10). Suppressing one
// would not just hide a message: the file silently counts as empty, or a
// failed -o / -MF open exits 0 with no file written. These are also the
// codes jiepp_command() lists in jiepp_continue_abort_codes() (jiepp.cpp)
// as aborting in continue mode, together with PP60/PP61, which stay
// suppressible (SPEC 11 uses {#ignore PP60} to lift the limit).
static bool is_unignorable_file_code(Issue::Code code) {
    switch (code) {
    case Issue::Code::FILE_ERROR:
    case Issue::Code::FILE_NOT_FOUND:
    case Issue::Code::MAX_INCLUDE_DEPTH_EXCEEDED:
    case Issue::Code::INVALID_COMMAND:
    case Issue::Code::INCLUDE_TARGET_IS_DIRECTORY:
        return true;
    default:
        return false;
    }
}

bool Issue::is_ignored(Code code) {
    if (is_severe(code) || is_unignorable_file_code(code))
        return false;
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
    // {#ignore} suppresses the code, except SEVERE and PP10-PP14, which
    // is_ignored() never reports as ignored.
    if (is_ignored(code))
        return;

    // Default-off code that was not enabled: nothing at all happens.
    if (!is_enabled(code))
        return;

    Severity severity = severity_of(code);

    // -Werror (all warnings) or add_werror() (this code): promote WARNING
    // to ERROR
    bool promoted = false;
    if (is_warning(code) && (werror_ || werror_codes_.count(code) != 0))
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
        // filename -- a LineGuard pushed while only the dummy is below it
        // (LineGuard(ln) pushes {ln, filepath()}) just copies that
        // placeholder forward.
        // Without this, such a diagnostic rendered as "<unknown
        // location>:N.0: ..." instead of "jiepp: ...", because
        // loc_stack_.size() was already 2 by then. (A -D/-U operand itself
        // is re-lexed with iec3_relex(), which pushes nothing, so its lexer
        // diagnostics take the size() == 1 branch; this condition stays as
        // a guard for the other paths.)
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
#ifdef JIEPP_SANDBOX
        // Sandbox: strip control characters to prevent log injection (SPEC
        // 14) from both user-derived fields of every diagnostic -- the
        // context text and the file path -- right before they are handed
        // to the formatter. In src/, this call is where message_ and
        // stream_ are used to print a diagnostic (main.cpp's last-resort
        // PP01 lines write to std::cerr directly but print no user text in
        // sandbox builds); the code, severity and the C++ source location
        // are not user text, and Issue::Exception carries only the code. So
        // the printed line and the IssueMessage formatter both receive the
        // stripped text. The fixed placeholders ("jiepp", "<unknown location>",
        // "<no location>") contain no control characters and are unchanged.
        // The strip keeps '\n' and '\r'; after it, each LF and each CR is
        // replaced with one space (CR LF becomes two spaces) in the file
        // path of every diagnostic and in the context of every diagnostic
        // except the message passthrough codes PP90-PP93, whose context is
        // the user's own message ({#error}/{#warning}/{#info}/{#severe},
        // __error/__assert), where a $n/$r line break is intended. Elsewhere
        // a line break would let quoted operand text or a path forge a whole
        // extra diagnostic line. Replacing a byte with a space in place
        // removes nothing and a space is neither C2 nor 0x80-0x9F, so the
        // strip's "no forbidden byte or pair" result still holds.
        // The UTF-8 C1 rule (C2 80 - C2 9F) assumes UTF-8 text. On Windows a
        // file path arrives in the ANSI code page (argv; CP932 C2 82 A0 is
        // U+FF82 U+3042), so the path keeps those pairs there and only loses
        // C0 controls and DEL. That covers the location path and the context
        // of the file codes PP10/PP11/PP12/PP14, which is the path itself:
        // in sandbox builds {#include}/{#sinclude}/__has_include are PP62 and
        // {#line}'s file name is ignored, so both come only from the command
        // line (input file, --disppath, -include, -o, -MF) or from a library
        // caller. Other contexts are input text (UTF-8, SPEC 1) and keep the
        // full rule on every platform.
#ifdef _WIN32
        constexpr Util::C1Pairs path_c1 = Util::C1Pairs::Keep;
#else
        constexpr Util::C1Pairs path_c1 = Util::C1Pairs::Remove;
#endif
        const bool message_code = code == Code::SEVERE_MESSAGE
            || code == Code::ERROR_MESSAGE
            || code == Code::WARNING_MESSAGE
            || code == Code::INFO_MESSAGE;
        const bool path_code = code == Code::FILE_ERROR
            || code == Code::FILE_NOT_FOUND
            || code == Code::MAX_INCLUDE_DEPTH_EXCEEDED
            || code == Code::INCLUDE_TARGET_IS_DIRECTORY;
        context = Util::strip_control_chars(context, path_code ? path_c1 : Util::C1Pairs::Remove);
        if (!message_code)
            context = Util::line_breaks_to_spaces(context);
        loc_file = Util::line_breaks_to_spaces(Util::strip_control_chars(loc_file, path_c1));
#endif
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
