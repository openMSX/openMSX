// $Id$

#ifdef _WIN32

#include "utf8_checked.hh"
#include "vla.hh"
#include "StringOp.hh"
#include "MSXException.hh"
#include <windows.h>

namespace utf8 {

std::wstring utf8to16(const std::string& utf8)
{
	const char* utf8A = utf8.c_str();
	int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8A, -1, NULL, 0);
	if (len) {
		VLA(wchar_t, utf16, len);
		len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8A, -1, utf16, len);
		if (len) {
			return utf16;
		}
	}
	throw openmsx::FatalError("MultiByteToWideChar failed: " + StringOp::toString(GetLastError()));
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
	throw openmsx::FatalError("WideCharToMultiByte failed: " + StringOp::toString(GetLastError()));
}

} // namespace utf8

#endif // _WIN32
