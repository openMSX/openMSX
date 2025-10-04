#ifndef FREETYPE_UTILS_HH
#define FREETYPE_UTILS_HH

#include "FileOperations.hh"
#include "MSXException.hh"

#include <cstdint>
#include <span>
#include <string>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace openmsx {

/* Example usage:
 *   FreeTypeLibrary library;
 *   std::vector<std::string> result;
 *   auto fontData = File(fontFilename).mmap<uint8_t>();
 *   for (auto i : xrange(getNumFaces(library, fontData))) {
 *       result.push_back(getFontDisplayName(library, fontData, i, fontFilename));
 *   }
 *   return result;
 */

struct FreeTypeLibrary
{
	FreeTypeLibrary(const FreeTypeLibrary&) = delete;
	FreeTypeLibrary(FreeTypeLibrary&&) = delete;
	FreeTypeLibrary& operator=(const FreeTypeLibrary&) = delete;
	FreeTypeLibrary& operator=(FreeTypeLibrary&&) = delete;

	FreeTypeLibrary()
	{
		if (FT_Init_FreeType(&library)) {
			throw MSXException("Could not initialize FreeType library");
		}
	}

	~FreeTypeLibrary()
	{
		FT_Done_FreeType(library);
	}

	FT_Library library;
};

struct FreeTypeFace
{
	FreeTypeFace(const FreeTypeFace&) = delete;
	FreeTypeFace(FreeTypeFace&&) = delete;
	FreeTypeFace& operator=(const FreeTypeFace&) = delete;
	FreeTypeFace& operator=(FreeTypeFace&&) = delete;

	FreeTypeFace(FreeTypeLibrary& library, std::span<const uint8_t> fontData, long faceIndex)
	{
		if (FT_New_Memory_Face(library.library, fontData.data(), fontData.size(), faceIndex, &face)) {
			throw MSXException("Could not parse font");
		}
	}

	~FreeTypeFace()
	{
		FT_Done_Face(face);
	}

	FT_Face face;
};

[[nodiscard]] inline long getNumFaces(FreeTypeLibrary& library, std::span<const uint8_t> fontData)
{
	FreeTypeFace face(library, fontData, -1);
	return face.face->num_faces;
}

[[nodiscard]] inline std::string getFontDisplayName(
	FreeTypeLibrary& library, const std::span<const uint8_t>& fontData, long faceIndex,
	std::string_view fontFile)
{
	FreeTypeFace face(library, fontData, faceIndex);
	auto toSV = [](const char* s) { return std::string_view(s ? s : ""); };
	auto family = toSV(face.face->family_name);
	auto style = toSV(face.face->style_name);

	auto name = !family.empty()
	          ? std::string(family)
	          : std::string(FileOperations::stem(fontFile));
	if (!style.empty() && (!family.empty() || style != family)) {
		strAppend(name, " - ", style);
	}
	return name;
}

} // namespace openmsx

#endif // FREETYPE_UTILS_HH
