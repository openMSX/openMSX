// $Id$

#ifdef _WIN32

#include "utf8_checked.hh"
#include "vla.hh"
#include "StringOp.hh"
#include "MSXException.hh"
#include <windows.h>

namespace utf8 {

bool ansitouf16(const std::string& ansi, UINT cp, DWORD dwFlags, std::wstring& utf16)
{
	const char* ansiA = ansi.c_str();
	int len = MultiByteToWideChar(cp, dwFlags, ansiA, -1, NULL, 0);
	if (len) {
		VLA(wchar_t, utf16W, len);
		len = MultiByteToWideChar(cp, dwFlags, ansiA, -1, utf16W, len);
		if (len) {
			utf16 = utf16W;
			return true;
		}
	}
	return false;
}

std::string unknowntoutf8(const std::string& unknown)
{
	std::wstring utf16;

	// Try UTF8 to UTF16 conversion first
	// If that fails, try CP_ACP to UTF16
	// If that fails, give up
	if (ansitouf16(unknown, CP_UTF8, MB_ERR_INVALID_CHARS, utf16) ||
		(GetLastError() == ERROR_NO_UNICODE_TRANSLATION &&
		ansitouf16(unknown, CP_ACP, MB_ERR_INVALID_CHARS, utf16)))
	{
		return utf16to8(utf16);
	}
	else
	{
		throw openmsx::FatalError(StringOp::Builder() <<
			"MultiByteToWideChar failed: " << GetLastError());
	}
}

std::wstring utf8to16(const std::string& utf8)
{
	std::wstring utf16;
	if (!ansitouf16(utf8, CP_UTF8, MB_ERR_INVALID_CHARS, utf16))
	{
		throw openmsx::FatalError(StringOp::Builder() <<
		"MultiByteToWideChar failed: " << GetLastError());
	}
	return utf16;
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
