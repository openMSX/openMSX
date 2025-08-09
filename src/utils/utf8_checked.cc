#ifdef _WIN32

#include "utf8_checked.hh"
#include "MSXException.hh"
#include <windows.h>

namespace utf8 {

[[nodiscard]] static std::wstring multiByteToUtf16(std::string_view multiByte, UINT cp, DWORD dwFlags)
{
	if (multiByte.empty()) return {};

	// Get required size for UTF-16 buffer
	const char* in = multiByte.data();
	auto inSize = static_cast<int>(multiByte.size());
	int outSize = MultiByteToWideChar(cp, dwFlags, in, inSize, nullptr, 0);
	if (outSize == 0) {
		throw openmsx::FatalError(
			"MultiByteToWideChar failed: ", GetLastError());
	}

	std::wstring utf16;
	utf16.resize_and_overwrite(outSize, [&](wchar_t* out, size_t) -> size_t {
		return MultiByteToWideChar(cp, dwFlags, in, inSize, out, outSize);
	});
	return utf16;
}


[[nodiscard]] static std::string utf16ToMultiByte(std::wstring_view utf16, UINT cp)
{
	if (utf16.empty()) return {};

	// Get required size for multiByte buffer
	const wchar_t* in = utf16.data();
	auto inSize = static_cast<int>(utf16.size());
	int outSize = WideCharToMultiByte(cp, 0, in, inSize, nullptr, 0, nullptr, nullptr);
	if (outSize == 0) {
		throw openmsx::FatalError(
			"WideCharToMultiByte failed: ", GetLastError());
	}

	std::string multiByte;
	multiByte.resize_and_overwrite(outSize, [&](char* out, size_t) -> size_t {
		return WideCharToMultiByte(cp, 0, in, inSize, out, outSize, nullptr, nullptr);
	});
	return multiByte;
}

std::wstring utf8to16(std::string_view utf8)
{
	return multiByteToUtf16(utf8, CP_UTF8, MB_ERR_INVALID_CHARS);
}

std::string utf16to8(std::wstring_view utf16)
{
	return utf16ToMultiByte(utf16, CP_UTF8);
}

} // namespace utf8

#endif // _WIN32
