#ifndef TCLARGPARSER_HH
#define TCLARGPARSER_HH

#include "CommandException.hh"
#include "TclObject.hh"
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace openmsx {

namespace detail {
	template<typename T> struct GetArg;

	template<> struct GetArg<bool> {
		static void operator()(Interpreter& interp, const TclObject& obj, bool& result) {
			result = obj.getBoolean(interp);
		}
	};
	template<> struct GetArg<int> {
		static void operator()(Interpreter& interp, const TclObject& obj, int& result) {
			result = obj.getInt(interp);
		}
	};
	template<> struct GetArg<double> {
		static void operator()(Interpreter& interp, const TclObject& obj, double& result) {
			result = obj.getDouble(interp);
		}
	};
	template<> struct GetArg<std::string_view> {
		static void operator()(Interpreter& /*interp*/, const TclObject& obj, std::string_view& result) {
			result = obj.getString();
		}
	};
	template<> struct GetArg<std::string> {
		static void operator()(Interpreter& /*interp*/, const TclObject& obj, std::string& result) {
			result = std::string(obj.getString());
		}
	};
	template<> struct GetArg<TclObject> {
		static void operator()(Interpreter& /*interp*/, const TclObject& obj, TclObject& result) {
			result = obj;
		}
	};

	template<typename T> struct GetArg<std::optional<T>> {
		static void operator()(Interpreter& interp, const TclObject& obj, std::optional<T>& result) {
			T t;
			GetArg<T>{}(interp, obj, t);
			result = std::move(t);
		}
	};

	template<typename T> struct GetArg<std::vector<T>> {
		static void operator()(Interpreter& interp, const TclObject& obj, std::vector<T>& result) {
			result.emplace_back();
			GetArg<T>{}(interp, obj, result.back());
		}
	};
}

// A Tcl-argument-parser description is made out of ArgsInfo objects
struct ArgsInfo
{
	std::string_view name;
	std::function<unsigned(Interpreter&, std::span<const TclObject>)> func;
};

// Parse a flag.
[[nodiscard]] inline ArgsInfo flagArg(std::string_view name, bool& flag)
{
	return {
		.name = name,
		.func = [&flag](Interpreter& /*interp*/, std::span<const TclObject> /*args*/) {
			flag = true;
			return 0;
		}
	};
}

// Option with a single value, handled by a callback
[[nodiscard]] inline ArgsInfo funcArg(
	std::string_view name,
	std::function<void(Interpreter&, const TclObject&)> func)
{
	return {
		.name = name,
		.func = [name, func = std::move(func)](Interpreter& interp, std::span<const TclObject> args) {
			if (args.empty()) {
				throw CommandException("missing argument for ", name);
			}
			func(interp, args.front());
			return 1;
		}
	};
}

// Parse a value (like a flag but with associated value).
template<typename T>
[[nodiscard]] ArgsInfo valueArg(std::string_view name, T& value)
{
	return funcArg(name, [&value](Interpreter& interp, const TclObject& arg) {
		detail::GetArg<T>{}(interp, arg, value);
	});
}

// Parse the given 'inArgs' arguments.
// The recognized flags/values are described in 'table'.
// The result of this parser is the collection of non-flag arguments.
// See src/unittest/TclArgParser.cc for example usages.
[[nodiscard]] std::vector<TclObject> parseTclArgs(
	Interpreter& interp, std::span<const TclObject> inArgs, std::span<const ArgsInfo> table);

} // namespace openmsx

#endif
