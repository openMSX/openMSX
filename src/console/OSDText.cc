// $Id$

#include "OSDText.hh"
#include "TTFFont.hh"
#include "SDLImage.hh"
#include "OutputSurface.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "TclObject.hh"
#include "StringOp.hh"
#include "unreachable.hh"
#include "components.hh"
#include <cassert>
#if COMPONENT_GL
#include "GLImage.hh"
#endif

using std::string;
using std::vector;

namespace openmsx {

OSDText::OSDText(const OSDGUI& gui, const string& name)
	: OSDImageBasedWidget(gui, name)
	, fontfile("skins/Vera.ttf.gz")
	, size(12)
	, wrapMode(NONE), wrapw(0.0), wraprelw(1.0)
{
}

OSDText::~OSDText()
{
}

void OSDText::getProperties(std::set<string>& result) const
{
	result.insert("-text");
	result.insert("-font");
	result.insert("-size");
	result.insert("-wrap");
	result.insert("-wrapw");
	result.insert("-wraprelw");
	OSDImageBasedWidget::getProperties(result);
}

void OSDText::setProperty(const string& name, const TclObject& value)
{
	if (name == "-text") {
		string val = value.getString();
		if (text != val) {
			text = val;
			// note: don't invalidate font (don't reopen font file)
			OSDImageBasedWidget::invalidateLocal();
			invalidateChildren();
		}
	} else if (name == "-font") {
		string val = value.getString();
		if (fontfile != val) {
			SystemFileContext context;
			CommandController* controller = NULL; // ok for SystemFileContext
			string file = context.resolve(*controller, val);
			if (!FileOperations::isRegularFile(file)) {
				throw CommandException("Not a valid font file: " + val);
			}
			fontfile = val;
			invalidateRecursive();
		}
	} else if (name == "-size") {
		int size2 = value.getInt();
		if (size != size2) {
			size = size2;
			invalidateRecursive();
		}
	} else if (name == "-wrap") {
		string val = value.getString();
		WrapMode wrapMode2;
		if (val == "none") {
			wrapMode2 = NONE;
		} else if (val == "word") {
			wrapMode2 = WORD;
		} else if (val == "char") {
			wrapMode2 = CHAR;
		} else {
			throw CommandException("Not a valid value for -wrap, "
				"expected one of 'none word char', but got '" +
				val + "'.");
		}
		if (wrapMode != wrapMode2) {
			wrapMode = wrapMode2;
			invalidateRecursive();
		}
	} else if (name == "-wrapw") {
		double wrapw2 = value.getDouble();
		if (wrapw != wrapw2) {
			wrapw = wrapw2;
			invalidateRecursive();
		}
	} else if (name == "-wraprelw") {
		double wraprelw2 = value.getDouble();
		if (wraprelw != wraprelw2) {
			wraprelw = wraprelw2;
			invalidateRecursive();
		}
	} else {
		OSDImageBasedWidget::setProperty(name, value);
	}
}

void OSDText::getProperty(const string& name, TclObject& result) const
{
	if (name == "-text") {
		result.setString(text);
	} else if (name == "-font") {
		result.setString(fontfile);
	} else if (name == "-size") {
		result.setInt(size);
	} else if (name == "-wrap") {
		string wrapString;
		switch (wrapMode) {
			case NONE: wrapString = "none"; break;
			case WORD: wrapString = "word"; break;
			case CHAR: wrapString = "char"; break;
			default: UNREACHABLE;
		}
		result.setString(wrapString);
	} else if (name == "-wrapw") {
		result.setDouble(wrapw);
	} else if (name == "-wraprelw") {
		result.setDouble(wraprelw);
	} else {
		OSDImageBasedWidget::getProperty(name, result);
	}
}

void OSDText::invalidateLocal()
{
	font.reset();
	OSDImageBasedWidget::invalidateLocal();
}


string OSDText::getType() const
{
	return "text";
}

void OSDText::getWidthHeight(const OutputRectangle& /*output*/,
                             double& width, double& height) const
{
	if (image.get()) {
		width  = image->getWidth();
		height = image->getHeight();
	} else {
		// we don't know the dimensions, must be because of an error
		assert(hasError());
		width  = 0;
		height = 0;
	}
}

byte OSDText::getFadedAlpha() const
{
	return byte((getRGBA(0) & 0xff) * getRecursiveFadeValue());
}

template <typename IMAGE> BaseImage* OSDText::create(OutputSurface& output)
{
	if (text.empty()) {
		return new IMAGE(0, 0, unsigned(0));
	}
	int scale = getScaleFactor(output);
	if (!font.get()) {
		try {
			SystemFileContext context;
			CommandController* controller = NULL; // ok for SystemFileContext
			string file = context.resolve(*controller, fontfile);
			int ptSize = size * scale;
			font.reset(new TTFFont(file, ptSize));
		} catch (MSXException& e) {
			throw MSXException("Couldn't open font: " + e.getMessage());
		}
	}
	try {
		double pWidth, pHeight;
		getParent()->getWidthHeight(output, pWidth, pHeight);
		int maxWidth = int(wrapw * scale + wraprelw * pWidth + 0.5);
		// Width can't be negative, if it is make it zero instead.
		// This will put each character on a different line.
		maxWidth = std::max(0, maxWidth);

		// TODO gradient???
		unsigned rgba = getRGBA(0);
		string wrappedText;
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
		// That way we can avoid StringOp::join() (in the wrap functions)
		// followed by // StringOp::split() (in TTFFont::render()).
		SDLSurfacePtr surface(font->render(wrappedText,
			(rgba >> 24) & 0xff, (rgba >> 16) & 0xff, (rgba >> 8) & 0xff));
		if (surface.get()) {
			return new IMAGE(surface);
		} else {
			return new IMAGE(0, 0, unsigned(0));
		}
	} catch (MSXException& e) {
		throw MSXException("Couldn't render text: " + e.getMessage());
	}
}

unsigned OSDText::splitAtChar(const string& line, unsigned maxWidth) const
{
	// TODO handle multi-byte utf8-characters correctly

	if (line.empty()) {
		// empty line always fits (explicitly handle this because
		// SDL_TTF can't handle empty strings)
		return 0;
	}

	unsigned width, height;
	font->getSize(line, width, height);
	if (width <= maxWidth) {
		// whole line fits
		return line.size();
	}

	// binary search till we found the largest initial substring that is
	// not wider than maxWidth
	unsigned min = 0;
	unsigned max = line.size();
	// invariant: line.substr(0, min) DOES     fit
	//            line.substr(0, max) DOES NOT fit
	unsigned cur = (min + max) / 2;
	if (cur == 0) {
		// We should at least return 1 character (even if that doesn't
		// fit), otherwise we'll get in an endless loop.
		return 1;
	}
	while (true) {
		assert(min < cur);
		assert(cur < max);
		string curStr = line.substr(0, cur);
		font->getSize(curStr, width, height);
		if (width <= maxWidth) {
			// still fits, try to enlarge
			unsigned next = (cur + max) / 2;
			if (next == cur) {
				return cur;
			}
			min = cur;
			cur = next;
		} else {
			// doesn't fit anymore, try to shrink
			unsigned next = (min + cur) / 2;
			if (next == min) {
				// return at least one char, see above
				return std::max(1u, min);
			}
			max = cur;
			cur = next;
		}
	}
}

// Search for a position that's strictly between min and max and which points
// to a character directly following a delimiter character. if no such position
// exits, this function returns 'min'.
// This function works correctly with multi-byte utf8-encoding as long as
// all delimiter characters are single byte chars.
static unsigned findSplitPoint(const string& line, unsigned min, unsigned max)
{
	static const char* const delimiters = " -/";

	// initial guess for a good position
	assert(min < max);
	string::size_type pos = (min + max) / 2;
	if (pos == min) {
		// can't reduce further
		return min;
	}

	// try searching backward (this also checks current position)
	assert(pos > min);
	string::size_type pos2 = line.substr(min, pos - min).find_last_of(delimiters);
	if (pos2 != string::npos) {
		pos2 += min + 1;
		assert(min < pos2);
		assert(pos2 <= pos);
		return pos2;
	}

	// try searching forward
	string::size_type pos3 = line.substr(pos, max - pos).find_first_of(delimiters);
	if (pos3 != string::npos) {
		pos3 += pos;
		assert(pos3 < max);
		pos3 += 1; // char directly after a delimiter;
		if (pos3 < max) {
			return pos3;
		}
	}

	return min;
}

unsigned OSDText::splitAtWord(const string& line, unsigned maxWidth) const
{
	// TODO this is very similar to splitAtChar, try to merge common code

	if (line.empty()) {
		// empty line always fits (explicitly handle this because
		// SDL_TTF can't handle empty strings)
		return 0;
	}

	unsigned width, height;
	font->getSize(line, width, height);
	if (width <= maxWidth) {
		// whole line fits
		return line.size();
	}

	// binary search till we found the largest initial substring that is
	// not wider than maxWidth
	unsigned min = 0;
	unsigned max = line.size();
	// invariant: line.substr(0, min) DOES     fit
	//            line.substr(0, max) DOES NOT fit
	unsigned cur = findSplitPoint(line, min, max);
	if (cur == 0) {
		// Could not find a valid split point, then split on char
		// (this also handles the case of a single too wide char)
		return splitAtChar(line, maxWidth);
	}
	while (true) {
		assert(min < cur);
		assert(cur < max);
		string curStr = line.substr(0, cur);
		StringOp::trimRight(curStr, " "); // remove trailing spaces
		font->getSize(curStr, width, height);
		if (width <= maxWidth) {
			// still fits, try to enlarge
			unsigned next = findSplitPoint(line, cur, max);
			if (next == cur) {
				return cur;
			}
			min = cur;
			cur = next;
		} else {
			// doesn't fit anymore, try to shrink
			unsigned next = findSplitPoint(line, min, cur);
			if (next == min) {
				if (min == 0) {
					// even the first word does not fit,
					// split on char (see above)
					return splitAtChar(line, maxWidth);
				}
				return min;
			}
			max = cur;
			cur = next;
		}
	}
}


string OSDText::getCharWrappedText(const string& text, unsigned maxWidth) const
{
	vector<string> lines;
	StringOp::split(text, "\n", lines);

	vector<string> wrappedLines;
	for (vector<string>::const_iterator it = lines.begin();
	     it != lines.end(); ++it) {
		string line = *it;
		do {
			unsigned pos = splitAtChar(line, maxWidth);
			wrappedLines.push_back(line.substr(0, pos));
			line = line.substr(pos);
		} while (!line.empty());
	}

	return StringOp::join(wrappedLines, "\n");
}

string OSDText::getWordWrappedText(const string& text, unsigned maxWidth) const
{
	vector<string> lines;
	StringOp::split(text, "\n", lines);

	vector<string> wrappedLines;
	for (vector<string>::const_iterator it = lines.begin();
	     it != lines.end(); ++it) {
		string line = *it;
		do {
			unsigned pos = splitAtWord(line, maxWidth);
			string first = line.substr(0, pos);
			StringOp::trimRight(first, " "); // remove trailing spaces
			wrappedLines.push_back(line.substr(0, pos));
			line = line.substr(pos);
			StringOp::trimLeft(line, " "); // remove leading spaces
		} while (!line.empty());
	}

	return StringOp::join(wrappedLines, "\n");
}

BaseImage* OSDText::createSDL(OutputSurface& output)
{
	return create<SDLImage>(output);
}

BaseImage* OSDText::createGL(OutputSurface& output)
{
#if COMPONENT_GL
	return create<GLImage>(output);
#else
	(void)&output;
	return NULL;
#endif
}

} // namespace openmsx
