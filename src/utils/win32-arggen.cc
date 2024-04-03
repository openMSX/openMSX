#ifdef _WIN32

#include "win32-arggen.hh"
#include "MSXException.hh"
#include "utf8_checked.hh"
#include "xrange.hh"
#include <Windows.h>
#include <shellapi.h>

namespace openmsx {

ArgumentGenerator::~ArgumentGenerator()
{
	for (auto i : xrange(argc)) {
		free(argv[i]);
	}
}

char** ArgumentGenerator::GetArguments(int& argc_)
{
	if (argv.empty()) {
		LPWSTR* pszArgList = CommandLineToArgvW(GetCommandLineW(), &argc);
		if (!pszArgList) {
			throw MSXException("Failed to obtain command line arguments");
		}

		argv.resize(argc);
		for (auto i : xrange(argc)) {
			argv[i] = strdup(utf8::utf16to8(pszArgList[i]).c_str());
		}
		LocalFree(pszArgList);
	}

	argc_ = argc;
	return argv.data();
}

} // namespace openmsx

#endif
