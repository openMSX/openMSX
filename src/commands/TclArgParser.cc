#include "TclArgParser.hh"
#include "CommandException.hh"
#include "join.hh"
#include "ranges.hh"
#include "stl.hh"
#include "view.hh"

namespace openmsx {

std::vector<TclObject> parseTclArgs(Interpreter& interp, span<const TclObject> inArgs, span<const ArgsInfo> table)
{
	std::vector<TclObject> outArgs;
	outArgs.reserve(inArgs.size());

	while (!inArgs.empty()) {
		auto arg = inArgs.front();
		auto argStr = arg.getString();
		inArgs = inArgs.subspan<1>();
		if (argStr.starts_with('-')) {
			if (argStr == "--") {
				append(outArgs, inArgs);
				break;
			}
			auto it = ranges::find(table, argStr, &ArgsInfo::name);
			if (it == table.end()) {
				throw CommandException(
					"Invalid option: '", argStr, "'. Must be one of ",
					join(view::transform(table, [](auto& info) {
					             return strCat('\'', info.name, '\'');
					     }), ", "), '.');
			}
			auto consumed = it->func(interp, inArgs);
			inArgs = inArgs.subspan(consumed);
		} else {
			outArgs.push_back(arg);
		}
	}

	return outArgs;
}

} // namespace openmsx
