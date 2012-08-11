// $Id$

#include "MessageCommand.hh"
#include "CommandException.hh"
#include "CliComm.hh"

namespace openmsx {

MessageCommand::MessageCommand(CommandController& controller)
	: Command(controller, "message")
{
}

static CliComm::LogLevel getLevel(const std::string& level)
{
	const char* const* levels = CliComm::getLevelStrings();
	for (int i = 0; i < CliComm::NUM_LEVELS; ++i) {
		if (level == levels[i]) {
			return static_cast<CliComm::LogLevel>(i);
		}
	}
	throw CommandException("Unknown level string: " + level);
}

std::string MessageCommand::execute(const std::vector<std::string>& tokens)
{
	CliComm& cliComm = getCliComm();
	CliComm::LogLevel level = CliComm::INFO;
	switch (tokens.size()) {
	case 3:
		level = getLevel(tokens[2]);
		// fall-through
	case 2:
		cliComm.log(level, tokens[1]);
		break;
	default:
		throw SyntaxError();
	}
	return "";
}

std::string MessageCommand::help(const std::vector<std::string>& /*tokens*/) const
{
	return "message <text> [<level>]\n"
	       "Print a message. (By default) this message will be shown in "
	       "a colored box at the top of the screen. It's possible to "
	       "specify a level for the message (e.g. 'info', 'warning' or "
	       "'error').";
}

void MessageCommand::tabCompletion(std::vector<std::string>& tokens) const
{
	if (tokens.size() == 3) {
		const char* const* levels = CliComm::getLevelStrings();
		std::set<std::string> levelSet(levels, levels + CliComm::NUM_LEVELS);
		completeString(tokens, levelSet);
	}
}

} // namespace openmsx
