#include "MsxChar2Unicode.hh"

#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "MSXException.hh"

#include "StringOp.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "strCat.hh"
#include "utf8_unchecked.hh"
#include "xrange.hh"

#include <bit>

namespace openmsx {

MsxChar2Unicode::MsxChar2Unicode(std::string_view mappingName)
{
	ranges::fill(msx2unicode, uint32_t(-1));

	std::string filename;
	try {
		filename = systemFileContext().resolve(
			tmpStrCat("unicodemaps/character_set_mappings/", mappingName));
	} catch (FileException& e) {
		throw MSXException("Couldn't find MSX character mapping file that was specified in unicodemap: ", mappingName, " (", e.getMessage(), ")");
	}
	try {
		File file(filename);
		auto buf = file.mmap();
		parseVid(std::string_view(std::bit_cast<const char*>(buf.data()), buf.size()));
	} catch (FileException&) {
		throw MSXException("Couldn't load MSX character mapping file that was specified in unicodemap: ", filename);
	} catch (MSXException& e) {
		throw MSXException(e.getMessage(), " in ", filename);
	}
}

/* Remove the next line from 'text' and return it.
 * A line is everything upto the first newline character. The character is
 * removed from 'text' but not included in the return value.
 */
static constexpr std::string_view getLine(std::string_view& text)
{
	if (auto pos = text.find_first_of('\n'); pos != std::string_view::npos) {
		// handle both 'LF' and 'CR LF'
		auto pos2 = ((pos != 0) && (text[pos - 1] == '\r')) ? pos - 1 : pos;
		auto result = text.substr(0, pos2);
		text.remove_prefix(pos + 1);
		return result;
	}
	return std::exchange(text, {});
}

/* Return the given line with comments at the end of the line removed.
 * Comments start at the first '#' character and continue till the end of the
 * line.
 */
[[nodiscard]] static constexpr std::string_view stripComments(std::string_view line)
{
	if (auto pos = line.find_first_of('#'); pos != std::string_view::npos) {
		line = line.substr(0, pos);
	}
	return line;
}

/* Returns true iff the given character is a separator (whitespace).
 * Newline and hash-mark are handled (already removed) by other functions.
 */
[[nodiscard]] static constexpr bool isSep(char c)
{
	return c == one_of(' ', '\t', '\r'); // whitespace
}

/* Remove one token from 'line' and return it.
 * Tokens are separated by one or more separator character as defined by 'isSep()'.
 * Those separators are removed from 'line' but not included in the return value.
 * The assumption is that there are no leading separator characters before calling this function.
 */
static constexpr std::string_view getToken(std::string_view& line)
{
	size_t s = line.size();
	size_t i = 0;
	while ((i < s) && !isSep(line[i])) {
		++i;
	}
	auto result = line.substr(0, i);
	while ((i < s) && isSep(line[i])) {
		++i;
	}
	line.remove_prefix(i);
	return result;
}

void MsxChar2Unicode::parseVid(std::string_view file)
{
	// The general syntax of this file is
	//     <msx-char> <unicode> # comments
	// Fields are separated via whitespace (tabs or spaces).
	// For example:
	//     0x2A	0x002A	# ASTERISK

	// Usually each msx-char only has a single corresponding unicode, and
	// then 'unicode2msx.size()' will be (close to) 256. But for example
	// this is not the case in 'MSXVIDAR.TXT'.
	unicode2msx.reserve(256);

	while (!file.empty()) {
		auto origLine = getLine(file);
		auto line = stripComments(origLine);

		auto msxTok = getToken(line);
		if (msxTok.empty()) continue; // empty line (or only whitespace / comments)
		auto msx = StringOp::stringTo<uint8_t>(msxTok);
		if (!msx) {
			throw MSXException("Invalid msx character value, expected an "
			                   "integer in range 0x00..0xff, but got: ", msxTok);
		}

		auto unicodeTok = getToken(line);
		if ((unicodeTok.size() >= 5) && (unicodeTok[0] == '<') &&
		    (unicodeTok[3] == '>')   && (unicodeTok[4] == '+')) {
			// In some files the <unicode> field is preceded with an annotation like:
			//   <LR>+0x0020     left-to-right
			//   <RL>+0x0020     right-to-left
			//   <RV>+0x0020     reverse-video
			// Just strip out that annotation and ignore. Current
			// implementation assumes the code 'LR', 'RL', 'RV' is
			// exactly two characters long.
			unicodeTok.remove_prefix(5);
		}
		auto unicode = StringOp::stringTo<uint32_t>(unicodeTok);
		if (!unicode || *unicode > 0x10ffff) {
			throw MSXException("Invalid unicode character value, expected an "
			                   "integer in range 0..0x10ffff, but got: ", unicodeTok);
		}

		if (!line.empty()) {
			throw MSXException("Syntax error, expected \"<msx-char> <unicode>\", "
			                   "but got: ", origLine);
		}

		// There can be duplicates (e.g. in 'MSXVIDAR.TXT'), in that
		// case only keep the first entry ...
		if (msx2unicode[*msx] == uint32_t(-1)) {
			msx2unicode[*msx] = *unicode;
		}
		// ... but (for now) keep all unicode->msx mappings (duplicates are removed below).
		unicode2msx.emplace_back(*unicode, *msx);
	}

	// Sort on unicode (for later binary-search). If there are duplicate
	// unicodes (with different msx-code), then keep the first entry (hence
	// use stable_sort).
	ranges::stable_sort(unicode2msx, {}, &Entry::unicode);
	unicode2msx.erase(ranges::unique(unicode2msx, {}, &Entry::unicode), end(unicode2msx));
}

std::string MsxChar2Unicode::msxToUtf8(
	std::span<const uint8_t> msx, const std::function<uint32_t(uint8_t)>& fallback) const
{
	std::string utf8;
	utf8.reserve(msx.size()); // possibly underestimation, but that's fine
	auto out = std::back_inserter(utf8);
	for (auto m : msx) {
		auto u = msx2unicode[m];
		auto u2 = (u != uint32_t(-1)) ? u : fallback(m);
		out = utf8::unchecked::append(u2, out);
	}
	return utf8;
}

std::vector<uint8_t> MsxChar2Unicode::utf8ToMsx(
	std::string_view utf8, const std::function<uint8_t(uint32_t)>& fallback) const
{
	std::vector<uint8_t> msx;
	auto it = utf8.begin(), et = utf8.end();
	while (it != et) {
		auto u = utf8::unchecked::next(it);
		auto m = binary_find(unicode2msx, u, {}, &Entry::unicode);
		msx.push_back(m ? m->msx : fallback(u));
	}
	return msx;
}

std::string MsxChar2Unicode::msxToUtf8(std::span<const uint8_t> msx, char fallback) const
{
	return msxToUtf8(msx, [&](uint32_t) { return fallback; });
}

std::vector<uint8_t> MsxChar2Unicode::utf8ToMsx(std::string_view utf8, char fallback) const
{
	return utf8ToMsx(utf8, [&](uint8_t) { return fallback; });
}


} // namespace openmsx
