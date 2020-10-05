#ifndef WIN32_ARG_GEN_HH
#define WIN32_ARG_GEN_HH

#ifdef _WIN32

#include "MemBuffer.hh"

namespace openmsx {

class ArgumentGenerator
{
public:
	~ArgumentGenerator();
	[[nodiscard]] char** GetArguments(int& argc);

private:
	MemBuffer<char*> argv;
	int argc;
};

#endif

} // namespace openmsx

#endif // WIN32_ARG_GEN_HH
