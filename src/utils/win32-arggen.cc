// $Id$

#ifdef _WIN32

#include "win32-arggen.hh"
#include "MSXException.hh"
#include "utf8_checked.hh"
#include <windows.h>
#include <shellapi.h>

namespace openmsx {

ArgumentGenerator::~ArgumentGenerator()
{
	for (unsigned i = 0; i < argv.size(); ++i) {
		free(argv[i]);
	}
}

char** ArgumentGenerator::GetArguments(int& argc)
{
	if (argv.empty()) {
		int cArgs;
		LPWSTR* pszArglist = CommandLineToArgvW(GetCommandLineW(), &cArgs);
		if (pszArglist == NULL) {
			throw MSXException("Failed to obtain command line arguments");
		}

		argv.resize(cArgs);
		for (int i = 0; i < cArgs; ++i) {
			argv[i] = strdup(utf8::utf16to8(pszArglist[i]).c_str());
		}
		LocalFree(pszArglist);
	}

	argc = argv.size();
	return argv.data();
}

} // namespace openmsx

#endif
