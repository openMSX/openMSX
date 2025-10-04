#include "OSDText.hh"

#include "CommandException.hh"
#include "Display.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "GLImage.hh"
#include "TTFFont.hh"
#include "TclObject.hh"

#include "StringOp.hh"
#include "join.hh"
#include "narrow.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "utf8_core.hh"

#include <cassert>
#include <cmath>
#include <memory>

using namespace gl;

namespace openmsx {

OSDText::OSDText(Display& display_, const TclObject& name_)
	: OSDImageBasedWidget(display_, name_)
	, fontFile("skins/DejaVuSans.ttf.gz")
{
}

void OSDText::setProperty(
	Interpreter& interp, std::string_view propName, const TclObject& value)
{
	if (propName == "-text") {
		std::string_view val = value.getString();
		if (text != val) {
			text = val;
			// note: don't invalidate font (don't reopen font file)
			OSDImageBasedWidget::invalidateLocal();
			invalidateChildren();
		}
	} else if (propName == "-font") {
		std::string val(value.getString());
		if (fontFile != val) {
			if (std::string file = systemFileContext().resolve(val);
				!FileOperations::isRegularFile(file)) {
				throw CommandException("Not a valid font file: ", val);
			}
			fontFile = val;
			invalidateRecursive();
		}
	} else if (propName == "-fontfaceindex") {
		int val = value.getInt(interp);
		if (val < 0) {
			throw CommandException("Not a valid value for -fontfaceindex, "
					"should be 0 or greater integer: ", val);
		}
		fontFaceIndex = val;
	} else if (propName == "-size") {
		int size2 = value.getInt(interp);
		if (size != size2) {
			size = size2;
			invalidateRecursive();
		}
	} else if (propName == "-wrap") {
		std::string_view val = value.getString();
		WrapMode wrapMode2 = [&] {
			if (val == "none") {
				return NONE;
			} else if (val == "word") {
				return WORD;
			} else if (val == "char") {
				return CHAR;
			} else {
				throw CommandException("Not a valid value for -wrap, "
					"expected one of 'none word char', but got '",
					val, "'.");
			}
		}();
		if (wrapMode != wrapMode2) {
			wrapMode = wrapMode2;
			invalidateRecursive();
		}
	} else if (propName == "-wrapw") {
		float wrapw2 = value.getFloat(interp);
		if (wrapw != wrapw2) {
			wrapw = wrapw2;
			invalidateRecursive();
		}
	} else if (propName == "-wraprelw") {
		float wraprelw2 = value.getFloat(interp);
		if (wraprelw != wraprelw2) {
			wraprelw = wraprelw2;
			invalidateRecursive();
		}
	} else {
		OSDImageBasedWidget::setProperty(interp, propName, value);
	}
}

void OSDText::getProperty(std::string_view propName, TclObject& result) const
{
	if (propName == "-text") {
		result = text;
	} else if (propName == "-font") {
		result = fontFile;
	} else if (propName == "-fontfaceindex") {
		result = fontFaceIndex;
	} else if (propName == "-size") {
		result = size;
	} else if (propName == "-wrap") {
		result = [&]{
			switch (wrapMode) {
				case NONE: return "none"sv;
				case WORD: return "word"sv;
				case CHAR: return "char"sv;
				default: UNREACHABLE;
			}
		}();
	} else if (propName == "-wrapw") {
		result = wrapw;
	} else if (propName == "-wraprelw") {
		result = wraprelw;
	} else {
		OSDImageBasedWidget::getProperty(propName, result);
	}
}

void OSDText::invalidateLocal()
{
	font = TTFFont(); // clear font
	OSDImageBasedWidget::invalidateLocal();
}


std::string_view OSDText::getType() const
{
	return "text";
}

vec2 OSDText::getSize(const OutputSurface& /*output*/) const
{
	if (image) {
		return vec2(image->getSize());
	} else {
		// we don't know the dimensions, must be because of an error
		assert(hasError());
		return {};
	}
}

uint8_t OSDText::getFadedAlpha() const
{
	return narrow_cast<uint8_t>(narrow_cast<float>(getRGBA(0) & 0xff) * getRecursiveFadeValue());
}

std::unique_ptr<GLImage> OSDText::create(OutputSurface& output)
{
	if (text.empty()) {
		return std::make_unique<GLImage>(ivec2(), 0);
	}
	int scale = getScaleFactor(output);
	if (font.empty()) {
		try {
			font = TTFFont(systemFileContext().resolve(fontFile),
			               size * scale, fontFaceIndex);
		} catch (MSXException& e) {
			throw MSXException("Couldn't open font: ", e.getMessage());
		}
	}
	try {
		vec2 pSize = getParent()->getSize(output);
		int maxWidth = narrow_cast<int>(lrintf(wrapw * narrow<float>(scale) + wraprelw * pSize.x));
		// Width can't be negative, if it is make it zero instead.
		// This will put each character on a different line.
		maxWidth = std::max(0, maxWidth);

		// TODO gradient???
		unsigned textRgba = getRGBA(0);
		std::string wrappedText;
		if (wrapMode == NONE) {
			wrappedText = text; // don't wrap
		} else if (wrapMode == WORD) {
			wrappedText = getWordWrappedText(text, maxWidth);
		} else if (wrapMode == CHAR) {
			wrappedText = getCharWrappedText(text, maxWidth);
		} else {
			UNREACHABLE;
		}
		// An alternative is to pass vector<string> to TTFFont::render().
		// That way we can avoid join() (in the wrap functions)
		// followed by // StringOp::split() (in TTFFont::render()).
		SDLSurfacePtr surface(font.render(wrappedText,
		                                  narrow_cast<uint8_t>(textRgba >> 24),
		                                  narrow_cast<uint8_t>(textRgba >> 16),
		                                  narrow_cast<uint8_t>(textRgba >>  8)));
		if (surface) {
			return std::make_unique<GLImage>(std::move(surface));
		} else {
			return std::make_unique<GLImage>(ivec2(), 0);
		}
	} catch (MSXException& e) {
		throw MSXException("Couldn't render text: ", e.getMessage());
	}
}


// Search for a position strictly between min and max which also points to the
// start of a (possibly multi-byte) utf8-character. If no such position exits,
// this function returns 'min'.
static constexpr size_t findCharSplitPoint(std::string_view line, size_t min, size_t max)
{
	auto pos = (min + max) / 2;
	auto beginIt = line.data();
	auto posIt = beginIt + pos;

	auto fwdIt = utf8::sync_forward(posIt);
	auto maxIt = beginIt + max;
	assert(fwdIt <= maxIt);
	if (fwdIt != maxIt) {
		return fwdIt - beginIt;
	}

	auto bwdIt = utf8::sync_backward(posIt);
	auto minIt = beginIt + min;
	assert(minIt <= bwdIt); (void)minIt;
	return bwdIt - beginIt;
}

// Search for a position that's strictly between min and max and which points
// to a character directly following a delimiter character. if no such position
// exits, this function returns 'min'.
// This function works correctly with multi-byte utf8-encoding as long as
// all delimiter characters are single byte chars.
static constexpr size_t findWordSplitPoint(std::string_view line, size_t min, size_t max)
{
	constexpr std::string_view delimiters = " -/";

	// initial guess for a good position
	assert(min < max);
	size_t pos = (min + max) / 2;
	if (pos == min) {
		// can't reduce further
		return min;
	}

	// try searching backward (this also checks current position)
	assert(pos > min);
	if (auto pos2 = line.substr(min, pos - min).find_last_of(delimiters);
	    pos2 != std::string_view::npos) {
		pos2 += min + 1;
		assert(min < pos2);
		assert(pos2 <= pos);
		return pos2;
	}

	// try searching forward
	if (auto pos2 = line.substr(pos, max - pos).find_first_of(delimiters);
	    pos2 != std::string_view::npos) {
		pos2 += pos;
		assert(pos2 < max);
		pos2 += 1; // char directly after a delimiter;
		if (pos2 < max) {
			return pos2;
		}
	}

	return min;
}

static constexpr size_t takeSingleChar(std::string_view /*line*/, unsigned /*maxWidth*/)
{
	return 1;
}

template<typename FindSplitPointFunc, typename CantSplitFunc>
size_t OSDText::split(const std::string& line, unsigned maxWidth,
                      FindSplitPointFunc findSplitPoint,
                      CantSplitFunc cantSplit,
                      bool removeTrailingSpaces) const
{
	if (line.empty()) {
		// empty line always fits (explicitly handle this because
		// SDL_TTF can't handle empty strings)
		return 0;
	}

	if (unsigned width = font.getSize(line).x; width <= maxWidth) {
		// whole line fits
		return line.size();
	}

	// binary search till we found the largest initial substring that is
	// not wider than maxWidth
	size_t min = 0;
	size_t max = line.size();
	// invariant: line.substr(0, min) DOES     fit
	//            line.substr(0, max) DOES NOT fit
	size_t cur = findSplitPoint(line, min, max);
	if (cur == 0) {
		// Could not find a valid split point, then split on char
		// (this also handles the case of a single too wide char)
		return cantSplit(line, maxWidth);
	}
	while (true) {
		assert(min < cur);
		assert(cur < max);
		std::string curStr = line.substr(0, cur);
		if (removeTrailingSpaces) {
			StringOp::trimRight(curStr, ' ');
		}
		unsigned width2 = font.getSize(curStr).x;
		if (width2 <= maxWidth) {
			// still fits, try to enlarge
			size_t next = findSplitPoint(line, cur, max);
			if (next == cur) {
				return cur;
			}
			min = cur;
			cur = next;
		} else {
			// doesn't fit anymore, try to shrink
			size_t next = findSplitPoint(line, min, cur);
			if (next == min) {
				if (min == 0) {
					// even the first word does not fit,
					// split on char (see above)
					return cantSplit(line, maxWidth);
				}
				return min;
			}
			max = cur;
			cur = next;
		}
	}
}

size_t OSDText::splitAtChar(const std::string& line, unsigned maxWidth) const
{
	return split(line, maxWidth, findCharSplitPoint, takeSingleChar, false);
}

struct SplitAtChar {
	explicit SplitAtChar(const OSDText& osdText_) : osdText(osdText_) {}
	[[nodiscard]] size_t operator()(const std::string& line, unsigned maxWidth) const {
		return osdText.splitAtChar(line, maxWidth);
	}
	const OSDText& osdText;
};
size_t OSDText::splitAtWord(const std::string& line, unsigned maxWidth) const
{
	return split(line, maxWidth, findWordSplitPoint, SplitAtChar(*this), true);
}

std::string OSDText::getCharWrappedText(const std::string& txt, unsigned maxWidth) const
{
	std::vector<std::string_view> wrappedLines;
	for (auto line : StringOp::split_view(txt, '\n')) {
		do {
			auto p = splitAtChar(std::string(line), maxWidth);
			wrappedLines.push_back(line.substr(0, p));
			line = line.substr(p);
		} while (!line.empty());
	}
	return join(wrappedLines, '\n');
}

std::string OSDText::getWordWrappedText(const std::string& txt, unsigned maxWidth) const
{
	std::vector<std::string_view> wrappedLines;
	for (auto line : StringOp::split_view(txt, '\n')) {
		do {
			auto p = splitAtWord(std::string(line), maxWidth);
			std::string_view first = line.substr(0, p);
			StringOp::trimRight(first, ' '); // remove trailing spaces
			wrappedLines.push_back(first);
			line = line.substr(p);
			StringOp::trimLeft(line, ' '); // remove leading spaces
		} while (!line.empty());
	}
	return join(wrappedLines, '\n');
}

} // namespace openmsx
