#include "CliComm.hh"
#include "Completer.hh"
#include "Command.hh"
#include "CommandController.hh"
#include "CommandException.hh"

#include "Interpreter.hh"
#include "InterpreterOutput.hh"
#include "TclObject.hh"

#include "FileContext.hh"
#include "FileOperations.hh"
#include "foreach_file.hh"

#include "stl.hh"
#include "strCat.hh"
#include "stringsp.hh"
#include <algorithm>

namespace openmsx {

class GetConsoleWidthCommand : public Command {
public:
	GetConsoleWidthCommand(CommandController& c, int width_)
		: width(width_), Command(c, "tc_get_console_width") { }
	void execute(std::span<const TclObject>, TclObject& result) {
		result.addListElement(width);
	}
	std::string help(std::span<const TclObject>) const {
		return "Temporary command only enable during tab completion.";
	}
private:
	int width;
};

struct NoCaseCompare {
	bool operator()(zstring_view left, zstring_view right) const {
		auto size = std::min(left.size(), right.size());
		return strncasecmp(left.c_str(), right.c_str(), size) < 0;
	}
};

/// Q: Why is this callback necessary?
/// A: Tcl cannot sort in the order defined by the locale.
///    https://wiki.tcl-lang.org/page/lsort#ae700b29c398fa05555ccec915e58adc9e497c914ecd2ca2d14a638446a0ff19
class SortUnderCppCommand : public Command {
public:
	SortUnderCppCommand(CommandController& c)
		: Command(c, "tc_sort_by_cpp"), interp(c.getInterpreter())
	{ }
	void execute(std::span<const TclObject> tokens, TclObject& result) {
		if (tokens.size() < 2) { return; }
		bool caseSensitive = tokens[1].getBoolean(interp);
		std::span<const TclObject> strs = tokens.subspan(2);
		if (caseSensitive) {
			std::set<zstring_view> sorted;
			for (const auto& e : strs) { sorted.insert(e.getString()); }
			result.addListElements(sorted);
		}
		else {
			// When using 'std::set' like below, "AA", "aa", "Aa" and
			// "aA" will be unified. Original code doesn't want to do that.
			//
			// std::set<zstring_view, NoCaseCompare> sorted;
			// for (const auto& e : strs) { sorted.insert(e.getString()); }
			//
			std::set<zstring_view> uniqued;
			for (const auto& e : strs) { uniqued.insert(e.getString()); }
			std::vector<zstring_view> sorted;
			std::ranges::copy(uniqued, std::back_inserter(sorted));
			std::ranges::sort(sorted, NoCaseCompare());
			result.addListElements(sorted);
		}
	}
	std::string help(std::span<const TclObject>) const {
		return "Temporary command only enable during tab completion, "
		       "to support the lack of Tcl's 'lsort' functionality.";
	}
private:
	Interpreter& interp;
};

// Only called from 'GlobalCommandController.cc',
// only when 'help' is invoked without arguments.
std::vector<std::string> Completer::formatListInColumns(
	CommandController& controller, std::span<const std::string_view> input)
{
	std::set<std::string_view> unique_set(input.begin(), input.end());

	TclObject command = makeTclList("format_to_columns_from_cpp");
	TclObject entries = makeTclList();
	entries.addListElements(unique_set);
	command.addListElement(entries);
	std::vector<std::string> result;
	GetConsoleWidthCommand gtwc(controller, output->getOutputColumns());
	SortUnderCppCommand succ(controller);
	try {
		TclObject list = command.executeCommand(controller.getInterpreter());
		for (const auto& element : list) { result.push_back(std::string(element)); }
	}
	catch (CommandException& e) {
		CliComm& cliComm = controller.getCliComm();
		cliComm.printWarning(
			"Error while executing format_to_columns_from_cpp proc: ",
			e.getMessage());
	}
	return result;
}

void Completer::completeImpl(CommandController& controller,
                             std::vector<std::string>& tokens,
	                         std::set<std::string_view> possibleValues,
                             bool caseSensitive)
{
	TclObject command = makeTclList("generic_tab_completion");
	command.addListElement(caseSensitive);
	TclObject possibleValuesObj = makeTclList();
	possibleValuesObj.addListElements(possibleValues);
	command.addListElement(possibleValuesObj);
	try {
		doTabCompletion(controller, command, tokens);
	}
	catch (CommandException& e) {
		CliComm& cliComm = controller.getCliComm();
		cliComm.printWarning(
			"Error while executing tab_completion proc: ",
			e.getMessage());
	}
}

/**
 *
 * throws: CommandException when failed to calling Tcl code.
 */
void Completer::doTabCompletion(CommandController& controller, 
                                TclObject& command,
                                std::vector<std::string>& tokens)
{
	command.addListElements(tokens);
	GetConsoleWidthCommand gtwc(controller, output->getOutputColumns());
	SortUnderCppCommand succ(controller);
	TclObject list = command.executeCommand(controller.getInterpreter());
	auto begin = list.begin();
	auto end = list.end();
	for (/**/; begin != end; ++begin) {
		if (*begin == one_of("---done", "---cont")) {
			bool done = *begin == "---done";
			if (++begin != end) {
				tokens.back() = std::string(*begin);
			}
			if (done) { tokens.emplace_back(); }
			break;
		}
		else {
			if (*begin == "---") { ++begin; }
			break;
		}
	}
}

void Completer::completeFileName(CommandController& controller,
	std::vector<std::string>& tokens,
	const FileContext& context)
{
	completeFileNameImpl(controller, tokens, context, std::set<std::string_view>());
}

void Completer::completeFileNameImpl(CommandController& controller,
                                     std::vector<std::string>& tokens,
                                     const FileContext& context,
                                     std::set<std::string_view> extras)
{
	std::string& filename = tokens.back();
	filename = FileOperations::expandTilde(std::move(filename));
	filename = FileOperations::expandCurrentDirFromDrive(std::move(filename));

	TclObject command = makeTclList("file_tab_completion");
	command.addListElement(true); // case sensitive
	TclObject targetPaths = makeTclList();
	for (const auto& p : context.getPaths()) { targetPaths.addListElement(p); }
	TclObject extrasObj = makeTclList();
	extrasObj.addListElements(extras);
	command.addListElement(targetPaths);
	command.addListElement(extrasObj);
	try {
		doTabCompletion(controller, command, tokens);
	}
	catch (CommandException& e) {
		CliComm& cliComm = controller.getCliComm();
		cliComm.printWarning(
			"Error while executing file_tab_completion proc:",
			e.getMessage());
	}
}

void Completer::checkNumArgs(std::span<const TclObject> tokens, unsigned exactly, const char* errMessage) const
{
	checkNumArgs(tokens, exactly, Prefix{exactly - 1}, errMessage);
}

void Completer::checkNumArgs(std::span<const TclObject> tokens, AtLeast atLeast, const char* errMessage) const
{
	checkNumArgs(tokens, atLeast, Prefix{atLeast.min - 1}, errMessage);
}

void Completer::checkNumArgs(std::span<const TclObject> tokens, Between between, const char* errMessage) const
{
	checkNumArgs(tokens, between, Prefix{between.min - 1}, errMessage);
}

void Completer::checkNumArgs(std::span<const TclObject> tokens, unsigned exactly, Prefix prefix, const char* errMessage) const
{
	if (tokens.size() == exactly) return;
	getInterpreter().wrongNumArgs(prefix.n, tokens, errMessage);
}

void Completer::checkNumArgs(std::span<const TclObject> tokens, AtLeast atLeast, Prefix prefix, const char* errMessage) const
{
	if (tokens.size() >= atLeast.min) return;
	getInterpreter().wrongNumArgs(prefix.n, tokens, errMessage);
}

void Completer::checkNumArgs(std::span<const TclObject> tokens, Between between, Prefix prefix, const char* errMessage) const
{
	if (tokens.size() >= between.min && tokens.size() <= between.max) return;
	getInterpreter().wrongNumArgs(prefix.n, tokens, errMessage);
}

} // namespace openmsx
