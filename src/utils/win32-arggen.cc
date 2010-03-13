#ifdef _WIN32

#include <windows.h>
#include <Shellapi.h>
#include "MSXException.hh"
#include "utf8_checked.hh"
#include "win32-arggen.hh"

namespace openmsx {

ArgumentGenerator::ArgumentGenerator()
{
	cArgs = 0;
	ppszArgv = NULL;
}

ArgumentGenerator::~ArgumentGenerator()
{
	for (int i = 0; i < cArgs; ++i) {
		free(ppszArgv[i]);
	}
	delete[] ppszArgv;
}

char** ArgumentGenerator::GetArguments(int& argc)
{
	if (ppszArgv == NULL) {

		LPWSTR* pszArglist = CommandLineToArgvW(GetCommandLineW(), &cArgs);
		if (pszArglist == NULL) {
			throw MSXException("Failed to obtain command line arguments");
		}

		ppszArgv = new char*[cArgs];
		for (int i = 0; i < cArgs; ++i) {
			ppszArgv[i] = strdup(utf8::utf16to8(pszArglist[i]).c_str());
		}

		LocalFree(pszArglist);
	}

	argc = cArgs;
	return ppszArgv;
}

} // namespace openmsx

#endif
