#ifdef _WIN32

#include "utf8_checked.hh"
#include "vla.hh"
#include "MSXException.hh"
#include <windows.h>

namespace utf8 {

static bool multibytetoutf16(const std::string& multibyte, UINT cp, DWORD dwFlags, std::wstring& utf16)
{
	const char* multibyteA = multibyte.c_str();
	int len = MultiByteToWideChar(cp, dwFlags, multibyteA, -1, nullptr, 0);
	if (len) {
		VLA(wchar_t, utf16W, len);
		len = MultiByteToWideChar(cp, dwFlags, multibyteA, -1, utf16W, len);
		if (len) {
			utf16 = utf16W;
			return true;
		}
	}
	return false;
}

static bool utf16tomultibyte(const std::wstring& utf16, UINT cp, std::string& multibyte)
{
	const wchar_t* utf16W = utf16.c_str();
	int len = WideCharToMultiByte(cp, 0, utf16W, -1, nullptr, 0, nullptr, nullptr);
	if (len) {
		VLA(char, multibyteA, len);
		len = WideCharToMultiByte(cp, 0, utf16W, -1, multibyteA, len, nullptr, nullptr);
		if (len) {
			multibyte = multibyteA;
			return true;
		}
	}
	return false;
}

std::string utf8toansi(const std::string& utf8)
{
	std::wstring utf16;
	if (!multibytetoutf16(utf8, CP_UTF8, MB_ERR_INVALID_CHARS, utf16))
	{
		throw openmsx::FatalError(
			"MultiByteToWideChar failed: ", GetLastError());
	}

	std::string ansi;
	if (!utf16tomultibyte(utf16, CP_ACP, ansi))
	{
		throw openmsx::FatalError(
			"MultiByteToWideChar failed: ", GetLastError());
	}
	return ansi;
}

std::wstring utf8to16(const std::string& utf8)
{
	std::wstring utf16;
	if (!multibytetoutf16(utf8, CP_UTF8, MB_ERR_INVALID_CHARS, utf16))
	{
		throw openmsx::FatalError(
			"MultiByteToWideChar failed: ", GetLastError());
	}
	return utf16;
}

std::string utf16to8(const std::wstring& utf16)
{
	std::string utf8;
	if (!utf16tomultibyte(utf16, CP_UTF8, utf8))
	{
		throw openmsx::FatalError(
			"MultiByteToWideChar failed: ", GetLastError());
	}
	return utf8;
}

} // namespace utf8

#endif // _WIN32
