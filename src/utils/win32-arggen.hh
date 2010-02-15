#ifndef WIN32_ARG_GEN_HH
#define WIN32_ARG_GEN_HH

#ifdef _WIN32

namespace openmsx {

class ArgumentGenerator
{
private:
	int cArgs;
	char** ppszArgv;
public:
	ArgumentGenerator();
	~ArgumentGenerator();
	char** GetArguments(int& argc);
};

#endif

} // namespace openmsx

#endif // WIN32_ARG_GEN_HH
