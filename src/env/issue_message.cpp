#include "issue_message.hpp"

#include <iomanip>
#include <sstream>

namespace {

std::string severity_text(Issue::Severity severity) {
	switch (severity) {
	case Issue::Severity::SEVERE:  return "error";
	case Issue::Severity::ERROR:   return "error";
	case Issue::Severity::WARNING: return "warning";
	case Issue::Severity::INFO:    return "info";
	}
	return "";
}

std::string pp_code(Issue::Code code) {
    std::ostringstream o;
    o << "PP"
        << std::setw(2)
        << std::setfill('0')
        << static_cast<unsigned int>(code);
    return o.str();
}

std::string_view message_for(Issue::Code code) {
	switch (code) {
#define JIEPP_ISSUE_CODE(name, id, severity, message) case Issue::Code::name: return message;
#include "issue_codes.def"
#undef JIEPP_ISSUE_CODE
    }
    return "An unknown error occurred.";
}

} // anonymous namespace

std::string PlainTextMessage::strip_source_location(const std::string& line) {
#ifndef NDEBUG
	auto at = line.rfind("@");
	if (at != std::string::npos) {
		auto colon = line.rfind(':');
		if ((colon != std::string::npos) && (colon > at + 3))
			return line.substr(0, at);
	}
#endif
	return line;
}

Issue::Code PlainTextMessage::parse_code(const std::string& line) {
	// Parse "PPnnn" format from error output
	const std::size_t pos = line.find("PP");
	if (pos != std::string::npos) {
		std::size_t end = pos + 2;
		while (end < line.size() && std::isdigit(static_cast<unsigned char>(line[end])))
			++end;
		if (end != pos + 2) {
			try {
				auto codenum = line.substr(pos + 2, end - (pos + 2));
				unsigned int code = std::stoul(codenum);
				return static_cast<Issue::Code>(code);
			} catch (...) {}
		}
	}
	FATAL();
	return Issue::Code::FATAL; // unreachable
}

void PlainTextMessage::message(std::ostream& output, Issue::Severity severity, Issue::Code code, std::string context, std::string file, int lineno, int column, std::source_location loc_) {
	std::string loc = file + ":" + std::to_string(lineno) + "." + std::to_string(column);

	if ((code == Issue::Code::SEVERE_MESSAGE) || (code == Issue::Code::ERROR_MESSAGE) || (code == Issue::Code::WARNING_MESSAGE) || (code == Issue::Code::INFO_MESSAGE)) {
		output << loc << ": " << severity_text(severity) << ": " << pp_code(code) << ": '" << context << "'";
    } else {
		output << loc << ": " << severity_text(severity) << ": " << pp_code(code) << ": " << std::string(message_for(code));
		if (!context.empty())
			output << "; '" << context << "'";
	}
#ifndef NDEBUG
    output << "@" << loc_.file_name() << ':' << loc_.line();
#endif
}
