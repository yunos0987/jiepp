#pragma once

#include <string>

#include "issue.hpp"

class IssueMessage
{
public:
	virtual void message(std::ostream& out, Issue::Severity severity, Issue::Code code, std::string context, std::string file, int lineno, int column, std::source_location loc_ = std::source_location::current()) = 0;
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
	void message(std::ostream& output, Issue::Severity severity, Issue::Code code, std::string context, std::string file, int lineno, int column, std::source_location loc_ = std::source_location::current()) override;
};
