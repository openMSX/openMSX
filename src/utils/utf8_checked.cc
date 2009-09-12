// $Id$

#ifdef _WIN32

#include "utf8_checked.hh"
#include "vla.hh"
#include "StringOp.hh"
#include "MSXException.hh"
#include <windows.h>

namespace utf8 {

std::wstring ansitouf16(const std::string& ansi, UINT cp, DWORD dwFlags)
{
	const char* ansiA = ansi.c_str();
	int len = MultiByteToWideChar(cp, dwFlags, ansiA, -1, NULL, 0);
	if (len) {
		VLA(wchar_t, utf16, len);
		len = MultiByteToWideChar(cp, dwFlags, ansiA, -1, utf16, len);
		if (len) {
			return utf16;
		}
	}
	throw openmsx::FatalError(StringOp::Builder() <<
		"MultiByteToWideChar failed: " << GetLastError());
}

std::wstring acptoutf16(const std::string& acp)
{
	return ansitouf16(acp, CP_ACP, MB_ERR_INVALID_CHARS);
}

std::wstring utf8to16(const std::string& utf8)
{
	return ansitouf16(utf8, CP_UTF8, MB_ERR_INVALID_CHARS);
}

std::string utf16to8(const std::wstring& utf16)
{
	const wchar_t* utf16W = utf16.c_str();
	int len = WideCharToMultiByte(CP_UTF8, 0, utf16W, -1, NULL, 0, NULL, NULL);
	if (len) {
		VLA(char, utf8, len);
		len = WideCharToMultiByte(CP_UTF8, 0, utf16W, -1, utf8, len, NULL, NULL);
		if (len) {
			return utf8;
		}
	}
	throw openmsx::FatalError(StringOp::Builder() <<
		"WideCharToMultiByte failed: " << GetLastError());
}

} // namespace utf8

#endif // _WIN32
