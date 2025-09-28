#include "MessageCommand.hh"

#include "CliComm.hh"

#include "CommandException.hh"
#include "TclObject.hh"

#include "xrange.hh"

namespace openmsx {

MessageCommand::MessageCommand(CommandController& controller)
	: Command(controller, "message")
{
}

[[nodiscard]] static CliComm::LogLevel getLevel(std::string_view level)
{
	for (auto levels = CliComm::getLevelStrings();
	     auto i : xrange(levels.size())) {
		if (level == levels[i]) {
			return static_cast<CliComm::LogLevel>(i);
		}
	}
	throw CommandException("Unknown level string: ", level);
}

void MessageCommand::execute(std::span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, Between{2, 3}, "string ?level?");
	CliComm& cliComm = getCliComm();
	CliComm::LogLevel level = CliComm::LogLevel::INFO;
	switch (tokens.size()) {
	case 3:
		level = getLevel(tokens[2].getString());
		[[fallthrough]];
	case 2:
		cliComm.log(level, tokens[1].getString());
		break;
	}
}

std::string MessageCommand::help(std::span<const TclObject> /*tokens*/) const
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
		completeString(tokens, CliComm::getLevelStrings());
	}
}

} // namespace openmsx
