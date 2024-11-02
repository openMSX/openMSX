#ifdef _WIN32

#include "utf8_checked.hh"
#include "MSXException.hh"
#include <windows.h>

namespace utf8 {

static bool multiByteToUtf16(zstring_view multiByte, UINT cp, DWORD dwFlags, std::wstring& utf16)
{
	const char* multiByteA = multiByte.c_str();
	if (int len = MultiByteToWideChar(cp, dwFlags, multiByteA, -1, nullptr, 0)) {
		utf16.resize(len); // TODO use c++23 resize_and_overwrite()
		len = MultiByteToWideChar(cp, dwFlags, multiByteA, -1, utf16.data(), len);
		if (len) return true;
	}
	return false;
}

static bool utf16ToMultiByte(const std::wstring& utf16, UINT cp, std::string& multiByte)
{
	const wchar_t* utf16W = utf16.c_str();
	if (int len = WideCharToMultiByte(cp, 0, utf16W, -1, nullptr, 0, nullptr, nullptr)) {
		multiByte.resize(len); // TODO use c++23 resize_and_overwrite()
		len = WideCharToMultiByte(cp, 0, utf16W, -1, multiByte.data(), len, nullptr, nullptr);
		if (len) return true;
	}
	return false;
}

std::string utf8ToAnsi(zstring_view utf8)
{
	std::wstring utf16;
	if (!multiByteToUtf16(utf8, CP_UTF8, MB_ERR_INVALID_CHARS, utf16)) {
		throw openmsx::FatalError(
			"MultiByteToWideChar failed: ", GetLastError());
	}

	std::string ansi;
	if (!utf16ToMultiByte(utf16, CP_ACP, ansi)) {
		throw openmsx::FatalError(
			"MultiByteToWideChar failed: ", GetLastError());
	}
	return ansi;
}

std::wstring utf8to16(zstring_view utf8)
{
	std::wstring utf16;
	if (!multiByteToUtf16(utf8, CP_UTF8, MB_ERR_INVALID_CHARS, utf16))
	{
		throw openmsx::FatalError(
			"MultiByteToWideChar failed: ", GetLastError());
	}
	return utf16;
}

std::string utf16to8(const std::wstring& utf16)
{
	std::string utf8;
	if (!utf16ToMultiByte(utf16, CP_UTF8, utf8))
	{
		throw openmsx::FatalError(
			"MultiByteToWideChar failed: ", GetLastError());
	}
	return utf8;
}

} // namespace utf8

#endif // _WIN32
