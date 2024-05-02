#ifndef WIN32_ARG_GEN_HH
#define WIN32_ARG_GEN_HH

#ifdef _WIN32

#include "dynarray.hh"

#include <span>

namespace openmsx {

class ArgumentGenerator
{
public:
	ArgumentGenerator(const ArgumentGenerator&) = delete;
	ArgumentGenerator(ArgumentGenerator&&) = delete;
	ArgumentGenerator& operator=(const ArgumentGenerator&) = delete;
	ArgumentGenerator& operator=(ArgumentGenerator&&) = delete;

	ArgumentGenerator();
	~ArgumentGenerator();

	[[nodiscard]] std::span<char*> getArgs() {
		return {args.data(), args.size()};
	}

private:
	dynarray<char*> args;
};

#endif

} // namespace openmsx

#endif // WIN32_ARG_GEN_HH
