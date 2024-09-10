#ifdef _WIN32

#include "win32-arggen.hh"
#include "MSXException.hh"
#include "utf8_checked.hh"
#include "xrange.hh"
#include <windows.h>
#include <shellapi.h>

namespace openmsx {

ArgumentGenerator::ArgumentGenerator()
{
	int argc = 0;
	LPWSTR* pszArgList = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (!pszArgList) {
		throw MSXException("Failed to obtain command line arguments");
	}

	args = dynarray<char*>(argc);
	for (auto i : xrange(argc)) {
		args[i] = strdup(utf8::utf16to8(pszArgList[i]).c_str());
	}
	LocalFree(pszArgList);
}

ArgumentGenerator::~ArgumentGenerator()
{
	for (auto* arg : args) {
		free(arg);
	}
}

} // namespace openmsx

#endif
