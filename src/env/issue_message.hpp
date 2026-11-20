#pragma once

#include <optional>
#include <string>

#include "issue.hpp"

class IssueMessage
{
public:
    virtual void message(std::ostream& out, Issue::Severity severity, Issue::Code code, std::string context, std::string file, LineNo lineno, int column, std::source_location loc_ = std::source_location::current()) = 0;
    // One "In file included from" line (SPEC 16), without the line break:
    // `file` included the file at `lineno`; no line number for the command
    // line (`file` is Issue::COMMAND_LINE_LOCATION).
    virtual void included_from(std::ostream& out, const std::string& file, std::optional<LineNo> lineno) = 0;
};

class PlainTextMessage : public IssueMessage
{
public:
    static PlainTextMessage& instance() {
        static PlainTextMessage instance;
        return instance;
    }

    static std::string strip_source_location(const std::string& line);
    static Issue::Code parse_code(const std::string& line);

public:
    void message(std::ostream& output, Issue::Severity severity, Issue::Code code, std::string context, std::string file, LineNo lineno, int column, std::source_location loc_ = std::source_location::current()) override;
    void included_from(std::ostream& output, const std::string& file, std::optional<LineNo> lineno) override;
};
