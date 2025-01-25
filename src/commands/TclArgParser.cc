#include "TclArgParser.hh"

#include "CommandException.hh"

#include "join.hh"
#include "stl.hh"

#include <algorithm>
#include <ranges>

namespace openmsx {

std::vector<TclObject> parseTclArgs(Interpreter& interp, std::span<const TclObject> inArgs, std::span<const ArgsInfo> table)
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
			auto it = std::ranges::find(table, argStr, &ArgsInfo::name);
			if (it == table.end()) {
				throw CommandException(
					"Invalid option: '", argStr, "'. Must be one of ",
					join(std::views::transform(table, [](auto& info) {
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
