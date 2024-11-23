#ifndef RAPIDSAX_HH
#define RAPIDSAX_HH

// This code is _heavily_ based on RapidXml 1.13
//   http://rapidxml.sourceforge.net/
//
// RapidXml is a very fast XML parser.
//   http://xmlbench.sourceforge.net/results/benchmark200910/index.html
// One of the main reasons it can be this fast is that doesn't do any string
// copies. Instead the XML input data is modified in-place (e.g. for stuff like
// &lt; replacements). Though this also means the output produced by the parser
// is tied to the lifetime of the XML input data.
//
// RapidXml produces a DOM-like output. This parser has a SAX-like interface.

#include "one_of.hh"
#include "small_compare.hh"
#include "zstring_view.hh"

#include <array>
#include <cassert>
#include <cstdint>
#include <string_view>

namespace rapidsax {

// Parse given XML text and call callback functions in the given handler.
// - XML text must be zero-terminated
// - Handler must implement the methods defined in NullHandler (below). An
//   easy way to do this is to inherit from NullHandler and only reimplement
//   the methods that you need.
// - The behavior of the parser can be fine-tuned with the FLAGS parameter,
//   see below for more details.
// - When a parse error is encounter, an instance of ParseError is thrown.
// - The lifetime of the string_view's in the callback handler is the same as
//   the lifetime of the input XML data (no string copies are made, instead
//   the XML file is modified in-place and references to this data are passed).
template<int FLAGS, typename HANDLER> void parse(HANDLER& handler, char* xml);

// When loading an XML file from disk, the buffer needs to be 8 bytes bigger
// than the filesize. The first of these bytes must be filled with zero
// (zero-terminate the xml data). The other bytes are only there to allow to
// read up-to 8 bytes past the end without triggering memory protection errors.
inline constexpr size_t EXTRA_BUFFER_SPACE = 8;


// Flags that influence parsing behavior. The flags can be OR'ed together.

// Should XML entities like &lt; be expanded or not?
inline constexpr int noEntityTranslation = 0x1;
// Should leading and trailing whitespace be trimmed?
inline constexpr int trimWhitespace      = 0x2;
// Should sequences of whitespace characters be replaced with a single
// space character?
inline constexpr int normalizeWhitespace = 0x4;
// Should strings be modified (in-place) with a zero-terminator?
inline constexpr int zeroTerminateStrings = 0x8;


// Callback handler with all empty implementations (can be used as a base
// class in case you only need to reimplement a few of the methods).
//
// Several methods are overloaded with either 'std::string_view' or
// 'zstring_view' parameter types. Which one gets called depends on whether the
// 'zeroTerminateStrings' flag was passed.
class NullHandler
{
public:
	// Called when an opening XML tag is encountered.
	// 'name' is the name of the XML tag.
	void start(std::string_view /*name*/) {}
	void start(zstring_view /*name*/) {}

	// Called when a XML tag is closed.
	// Note: the parser does currently not check whether the name of the
	//       opening nd closing tags matches.
	void stop() {}

	// Called when text inside a tag is parsed.
	// XML entities are replaced (optional)
	// Whitespace is (optionally) trimmed or normalized.
	// This method is not called for an empty text string.
	// (Unlike other SAX parsers) the whole text string is always
	// passed in a single chunk (so no need to concatenate this text
	// with previous chunks in the callback).
	void text(std::string_view /*text*/) {}
	void text(zstring_view /*text*/) {}

	// Called for each parsed attribute.
	// Attributes can occur inside xml tags or inside XML declarations.
	void attribute(std::string_view /*name*/, std::string_view /*value*/) {}
	void attribute(zstring_view /*name*/, zstring_view /*value*/) {}

	// Called for parsed CDATA sections.
	void cdata(std::string_view /*value*/) {}
	void cdata(zstring_view /*value*/) {}

	// Called when a XML comment (<!-- ... -->) is parsed.
	void comment(std::string_view /*value*/) {}
	void comment(zstring_view /*value*/) {}

	// Called when XML declaration (<?xml .. ?>) is parsed.
	// Inside a XML declaration there can be attributes.
	void declarationStart() {}
	void declAttribute(std::string_view /*name*/, std::string_view /*value*/) {}
	void declAttribute(zstring_view /*name*/, zstring_view /*value*/) {}
	void declarationStop() {}

	// Called when the <!DOCTYPE ..> is parsed.
	void doctype(std::string_view /*text*/) {}
	void doctype(zstring_view /*text*/) {}

	// Called when XML processing instructions (<? .. ?>) are parsed.
	void procInstr(std::string_view /*target*/, std::string_view /*instr*/) {}
	void procInstr(zstring_view /*target*/, zstring_view /*instr*/) {}
};


class ParseError
{
public:
	ParseError(const char* what_, char* where_)
		: m_what(what_)
		, m_where(where_)
	{
	}

	[[nodiscard]] const char* what() const { return m_what; }
	[[nodiscard]] char* where() const { return m_where; }

private:
	const char* m_what;
	char* m_where;
};


namespace internal {

// Character class lookup table
//   bit 0: \0
//   bit 1: \t \r \r space
//   bit 2: <
//   bit 3: &
//   bit 4: '
//   bit 5: "
//   bit 6: / > ?
//   bit 7: ! =
inline constexpr std::array<const uint8_t, 256> lutChar =
{
	// 0   1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
	0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x02,0x00,0x00,0x02,0x00,0x00, // 0
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 1
	0x02,0x80,0x20,0x00,0x00,0x00,0x08,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40, // 2
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x80,0x40,0x40, // 3
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 4
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 5
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 6
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 7
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 8
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 9
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // A
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // B
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // C
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // D
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // E
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // F
};

// Digits (dec and hex, 255 denotes end of numeric character reference)
inline constexpr std::array<const uint8_t, 256> lutDigits =
{
	// 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
	 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // 0
	 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // 1
	 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // 2
	   0,  1,  2,  3,  4,  5,  6,  7,  8,  9,255,255,255,255,255,255,  // 3
	 255, 10, 11, 12, 13, 14, 15,255,255,255,255,255,255,255,255,255,  // 4
	 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // 5
	 255, 10, 11, 12, 13, 14, 15,255,255,255,255,255,255,255,255,255,  // 6
	 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // 7
	 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // 8
	 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // 9
	 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // A
	 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // B
	 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // C
	 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // D
	 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,  // E
	 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255   // F
};

// Detect whitespace character (space \n \r \t)
struct WhitespacePred {
	[[nodiscard]] static bool test(char ch) { return (lutChar[uint8_t(ch)] & 0x02) != 0; }
};

// Detect node name character (anything but space \n \r \t / > ? \0)
struct NodeNamePred {
	[[nodiscard]] static bool test(char ch) { return !(lutChar[uint8_t(ch)] & 0x43); }
};

// Detect attribute name character (anything but space \n \r \t / < > = ? ! \0)
struct AttributeNamePred {
	[[nodiscard]] static bool test(char ch) { return !(lutChar[uint8_t(ch)] & 0xC7); }
};

// Detect text character (PCDATA) (anything but < \0)
struct TextPred {
	[[nodiscard]] static bool test(char ch) { return !(lutChar[uint8_t(ch)] & 0x05); }
};

// Detect text character (PCDATA) that does not require processing when ws
// normalization is disabled (anything but < \0 &)
struct TextPureNoWsPred {
	[[nodiscard]] static bool test(char ch) { return !(lutChar[uint8_t(ch)] & 0x0D); }
};

// Detect text character (PCDATA) that does not require processing when ws
// normalization is enabled (anything but < \0 & space \n \r \t)
struct TextPureWithWsPred {
	[[nodiscard]] static bool test(char ch) { return !(lutChar[uint8_t(ch)] & 0x0F); }
};

// Detect attribute value character, single quote (anything but ' \0)
struct AttPred1 {
	[[nodiscard]] static bool test(char ch) { return !(lutChar[uint8_t(ch)] & 0x11); }
};
// Detect attribute value character, double quote (anything but " \0)
struct AttPred2 {
	[[nodiscard]] static bool test(char ch) { return !(lutChar[uint8_t(ch)] & 0x21); }
};

// Detect attribute value character, single quote, that does not require
// processing (anything but ' \0 &)
struct AttPurePred1 {
	[[nodiscard]] static bool test(char ch) { return !(lutChar[uint8_t(ch)] & 0x19); }
};
// Detect attribute value character, double quote, that does not require
// processing (anything but " \0 &)
struct AttPurePred2 {
	[[nodiscard]] static bool test(char ch) { return !(lutChar[uint8_t(ch)] & 0x29); }
};

// Insert coded character, using UTF8
inline void insertUTF8char(char*& text, uint32_t code)
{
	if (code < 0x80) { // 1 byte sequence
		text[0] = char(code);
		text += 1;
	} else if (code < 0x800) {// 2 byte sequence
		text[1] = char((code | 0x80) & 0xBF); code >>= 6;
		text[0] = char (code | 0xC0);
		text += 2;
	} else if (code < 0x10000) { // 3 byte sequence
		text[2] = char((code | 0x80) & 0xBF); code >>= 6;
		text[1] = char((code | 0x80) & 0xBF); code >>= 6;
		text[0] = char (code | 0xE0);
		text += 3;
	} else if (code < 0x110000) { // 4 byte sequence
		text[3] = char((code | 0x80) & 0xBF); code >>= 6;
		text[2] = char((code | 0x80) & 0xBF); code >>= 6;
		text[1] = char((code | 0x80) & 0xBF); code >>= 6;
		text[0] = char (code | 0xF0);
		text += 4;
	} else { // Invalid, only codes up to 0x10FFFF are allowed in Unicode
		throw ParseError("invalid numeric character entity", text);
	}
}

template<StringLiteral Str> [[nodiscard]] static inline bool next(const char* p)
{
	return small_compare<Str>(p);
}


// Skip characters until predicate evaluates to true
template<typename StopPred> static inline void skip(char*& text)
{
	char* tmp = text;
	while (StopPred::test(*tmp)) ++tmp;
	text = tmp;
}

// Skip characters until predicate evaluates to true while doing the following:
// - replacing XML character entity references with proper characters
//   (&apos; &amp; &quot; &lt; &gt; &#...;)
// - condensing whitespace sequences to single space character
template<typename StopPred, class StopPredPure, int FLAGS>
[[nodiscard]] static inline char* skipAndExpand(char*& text)
{
	// If entity translation, whitespace condense and whitespace
	// trimming is disabled, use plain skip.
	if constexpr ( (FLAGS & noEntityTranslation) &&
	              !(FLAGS & normalizeWhitespace)  &&
	              !(FLAGS & trimWhitespace)) {
		skip<StopPred>(text);
		return text;
	}

	// Use simple skip until first modification is detected
	skip<StopPredPure>(text);

	// Use translation skip
	char* src = text;
	char* dest = src;
	while (StopPred::test(*src)) {
		// Test if replacement is needed
		if (!(FLAGS & noEntityTranslation) &&
		    (src[0] == '&')) {
			switch (src[1]) {
			case 'a': // &amp; &apos;
				if (next<"amp;">(&src[1])) {
					*dest = '&';
					++dest;
					src += 5;
					continue;
				}
				if (next<"pos;">(&src[2])) {
					*dest = '\'';
					++dest;
					src += 6;
					continue;
				}
				break;

			case 'q': // &quot;
				if (next<"uot;">(&src[2])) {
					*dest = '"';
					++dest;
					src += 6;
					continue;
				}
				break;

			case 'g': // &gt;
				if (next<"t;">(&src[2])) {
					*dest = '>';
					++dest;
					src += 4;
					continue;
				}
				break;

			case 'l': // &lt;
				if (next<"t;">(&src[2])) {
					*dest = '<';
					++dest;
					src += 4;
					continue;
				}
				break;

			case '#': // &#...; - assumes ASCII
				if (src[2] == 'x') {
					uint32_t code = 0;
					src += 3; // skip &#x
					while (true) {
						uint8_t digit = lutDigits[uint8_t(*src)];
						if (digit == 0xFF) break;
						code = code * 16 + digit;
						++src;
					}
					insertUTF8char(dest, code);
				} else {
					uint32_t code = 0;
					src += 2; // skip &#
					while (true) {
						   uint8_t digit = lutDigits[uint8_t(*src)];
						   if (digit == 0xFF) break;
						   code = code * 10 + digit;
						   ++src;
					}
					insertUTF8char(dest, code);
				}
				if (*src != ';') {
					throw ParseError("expected ;", src);
				}
				++src;
				continue;

			default:
				// Something else, ignore, just copy '&' verbatim
				break;
			}
		}

		// Test if condensing is needed
		if ((FLAGS & normalizeWhitespace) &&
		    (WhitespacePred::test(*src))) {
			*dest++ = ' '; // single space in dest
			++src;         // skip first whitespace char
			// Skip remaining whitespace chars
			while (WhitespacePred::test(*src)) ++src;
			continue;
		}

		// No replacement, only copy character
		*dest++ = *src++;
	}

	// Return new end
	text = src;
	return dest;
}

inline void skipBOM(char*& text)
{
	if (next<"\357\273\277">(text)) { // char(0xEF), char(0xBB), char(0xBF)
		text += 3; // skip utf-8 bom
	}
}


template<int FLAGS, typename HANDLER> class Parser
{
	HANDLER& handler;

public:
	Parser(HANDLER& handler_, char* text)
		: handler(handler_)
	{
		skipBOM(text);
		while (true) {
			// Skip whitespace before node
			skip<WhitespacePred>(text);
			if (*text == 0) break;

			if (*text != '<') {
				throw ParseError("expected <", text);
			}
			++text; // skip '<'
			parseNode(text);
		}
	}

private:
	// Parse XML declaration (<?xml...)
	void parseDeclaration(char*& text)
	{
		handler.declarationStart();
		skip<WhitespacePred>(text); // skip ws before attributes or ?>
		parseAttributes(text, true);
		handler.declarationStop();

		// skip ?>
		if (!next<"?>">(text)) {
			throw ParseError("expected ?>", text);
		}
		text += 2;
	}

	// Parse XML comment (<!--...)
	void parseComment(char*& text)
	{
		// Skip until end of comment
		const char* value = text; // remember value start
		while (!next<"-->">(text)) {
			if (text[0] == 0) {
				throw ParseError("unexpected end of data", text);
			}
			++text;
		}
		if constexpr (FLAGS & zeroTerminateStrings) {
			*text = '\0';
			handler.comment(zstring_view(value, text - value));
		} else {
			handler.comment(std::string_view(value, text - value));
		}
		text += 3; // skip '-->'
	}

	void parseDoctype(char*& text)
	{
		const char* value = text; // remember value start

		// skip to >
		while (*text != '>') {
			switch (*text) {
			case '[': {
				// If '[' encountered, scan for matching ending
				// ']' using naive algorithm with depth. This
				// works for all W3C test files except for 2
				// most wicked.
				++text; // skip '['
				int depth = 1;
				while (depth > 0) {
					switch (*text) {
						case char('['): ++depth; break;
						case char(']'): --depth; break;
						case 0: throw ParseError(
							"unexpected end of data", text);
					}
					++text;
				}
				break;
			}
			case '\0':
				throw ParseError("unexpected end of data", text);

			default:
				++text;
			}
		}

		if constexpr (FLAGS & zeroTerminateStrings) {
			*text = '\0';
			handler.doctype(zstring_view(value, text - value));
		} else {
			handler.doctype(std::string_view(value, text - value));
		}
		text += 1; // skip '>'
	}

	void parsePI(char*& text)
	{
		// Extract PI target name
		const char* name = text;
		skip<NodeNamePred>(text);
		char* nameEnd = text;
		if (name == nameEnd) {
			throw ParseError("expected PI target", text);
		}

		// Skip whitespace between pi target and pi
		skip<WhitespacePred>(text);

		// Skip to '?>'
		const char* value = text; // Remember start of pi
		while (!next<"?>">(text)) {
			if (*text == 0) {
				throw ParseError("unexpected end of data", text);
			}
			++text;
		}
		// Set pi value (verbatim, no entity expansion or ws normalization)
		if constexpr (FLAGS & zeroTerminateStrings) {
			*nameEnd = '\0';
			*text = '\0';
			handler.procInstr(zstring_view(name,  nameEnd - name),
			                  zstring_view(value, text - value));
		} else {
			handler.procInstr(std::string_view(name,  nameEnd - name),
			                  std::string_view(value, text - value));
		}
		text += 2; // skip '?>'
	}

	void parseText(char*& text, char* contentsStart)
	{
		// Backup to contents start if whitespace trimming is disabled
		if constexpr (!(FLAGS & trimWhitespace)) {
			text = contentsStart;
		}
		// Skip until end of data
		const char* value = text;
		char* end = (FLAGS & normalizeWhitespace)
			? skipAndExpand<TextPred, TextPureWithWsPred, FLAGS>(text)
			: skipAndExpand<TextPred, TextPureNoWsPred  , FLAGS>(text);

		// Trim trailing whitespace; leading was already trimmed by
		// whitespace skip after >
		if constexpr ((FLAGS & trimWhitespace) != 0) {
			if constexpr (FLAGS & normalizeWhitespace) {
				// Whitespace is already condensed to single
				// space characters by skipping function, so
				// just trim 1 char off the end.
				if (end[-1] == ' ') {
					--end;
				}
			} else {
				// Backup until non-whitespace character is found
				while (WhitespacePred::test(end[-1])) {
					--end;
				}
			}
		}

		// check next char before calling handler.text()
		if (*text == '\0') {
			throw ParseError("unexpected end of data", text);
		} else {
			assert(*text == '<');
		}

		// Handle text, but only if non-empty.
		auto len = end - value;
		if (len) {
			if constexpr (FLAGS & zeroTerminateStrings) {
				*end = '\0';
				handler.text(zstring_view(value, len));
			} else {
				handler.text(std::string_view(value, len));
			}
		}
	}

	void parseCdata(char*& text)
	{
		// Skip until end of cdata
		const char* value = text;
		while (!next<"]]>">(text)) {
			if (text[0] == 0) {
				throw ParseError("unexpected end of data", text);
			}
			++text;
		}
		if constexpr (FLAGS & zeroTerminateStrings) {
			*text = '\0';
			handler.cdata(zstring_view(value, text - value));
		} else {
			handler.cdata(std::string_view(value, text - value));
		}
		text += 3; // skip ]]>
	}

	void parseElement(char*& text)
	{
		// Extract element name
		const char* name = text;
		skip<NodeNamePred>(text);
		char* nameEnd = text;
		if (name == nameEnd) {
			throw ParseError("expected element name", text);
		}
		char savedChar = *nameEnd;
		skip<WhitespacePred>(text); // skip ws before attributes or >
		if constexpr (FLAGS & zeroTerminateStrings) {
			*nameEnd = '\0';
			handler.start(zstring_view(name, nameEnd - name));
		} else {
			handler.start(std::string_view(name, nameEnd - name));
		}

		parseAttributes(text, false);

		// Determine ending type
		char endChar = ((FLAGS & zeroTerminateStrings) && (text == nameEnd))
		             ? savedChar : *text;
		if (endChar == '>') {
			++text;
			parseNodeContents(text);
		} else if (endChar == '/') {
			handler.stop();
			++text;
			if (*text != '>') {
				throw ParseError("expected >", text);
			}
			++text;
		} else {
			throw ParseError("expected >", text);
		}
	}

	// Determine node type, and parse it
	void parseNode(char*& text)
	{
		switch (text[0]) {
		case '?': // <?...
			++text; // skip ?
			// Note: this doesn't detect mixed case (xMl), does
			// that matter?
			if ((next<"xml">(text) || next<"XML">(text)) &&
			    WhitespacePred::test(text[3])) {
				// '<?xml ' - xml declaration
				text += 4; // skip 'xml '
				parseDeclaration(text);
			} else {
				parsePI(text);
			}
			break;

		case '!': // <!...
			// Parse proper subset of <! node
			switch (text[1]) {
			case '-': // <!-
				if (text[2] == '-') {
					// '<!--' - xml comment
					text += 3; // skip '!--'
					parseComment(text);
					return;
				}
				break;

			case '[': // <![
				if (next<"CDATA[">(&text[2])) {
					// '<![CDATA[' - cdata
					text += 8; // skip '![CDATA['
					parseCdata(text);
					return;
				}
				break;

			case 'D': // <!D
				if (next<"OCTYPE">(&text[2]) &&
				    WhitespacePred::test(text[8])) {
					// '<!DOCTYPE ' - doctype
					text += 9; // skip '!DOCTYPE '
					parseDoctype(text);
					return;
				}
				break;
			}
			// Attempt to skip other, unrecognized types starting with <!
			++text; // skip !
			while (*text != '>') {
				if (*text == 0) {
					throw ParseError(
						"unexpected end of data", text);
				}
				++text;
			}
			++text; // skip '>'
			break;

		default: // <...
			parseElement(text);
			break;
		}
	}

	// Parse contents of the node - children, data etc.
	void parseNodeContents(char*& text)
	{
		while (true) {
			char* contentsStart = text; // start before ws is skipped
			skip<WhitespacePred>(text); // Skip ws between > and contents

			switch (*text) {
			case '<': // Node closing or child node
afterText:		// After parseText() jump here instead of continuing
			// the loop, because skipping whitespace is unnecessary.
				if (text[1] == '/') {
					// Node closing
					text += 2; // skip '</'
					skip<NodeNamePred>(text);
					// TODO validate closing tag??
					handler.stop();
					// Skip remaining whitespace after node name
					skip<WhitespacePred>(text);
					if (*text != '>') {
						throw ParseError("expected >", text);
					}
					++text; // skip '>'
					return;
				} else {
					// Child node
					++text; // skip '<'
					parseNode(text);
				}
				break;

			case '\0':
				throw ParseError("unexpected end of data", text);

			default:
				parseText(text, contentsStart);
				goto afterText;
			}
		}
	}

	// Parse XML attributes of the node
	void parseAttributes(char*& text, bool declaration)
	{
		// For all attributes
		while (AttributeNamePred::test(*text)) {
			// Extract attribute name
			char* name = text;
			++text; // Skip first character of attribute name
			skip<AttributeNamePred>(text);
			char* nameEnd = text;
			if (name == nameEnd) {
				throw ParseError("expected attribute name", name);
			}

			skip<WhitespacePred>(text); // skip ws after name
			if (*text != '=') {
				throw ParseError("expected =", text);
			}
			++text; // skip =
			skip<WhitespacePred>(text); // skip ws after =

			// Skip quote and remember if it was ' or "
			char quote = *text;
			if (quote != one_of('\'', '"')) {
				throw ParseError("expected ' or \"", text);
			}
			++text;

			// Extract attribute value and expand char refs in it
			// No whitespace normalization in attributes
			constexpr int FLAGS2 = FLAGS & ~normalizeWhitespace;
			const char* value = text;
			char* valueEnd = (quote == '\'')
				? skipAndExpand<AttPred1, AttPurePred1, FLAGS2>(text)
				: skipAndExpand<AttPred2, AttPurePred2, FLAGS2>(text);
			// Make sure that end quote is present
			// check before calling handler.xxx()
			if (*text != quote) {
				throw ParseError("expected ' or \"", text);
			}
			++text; // skip quote

			if constexpr (FLAGS & zeroTerminateStrings) {
				*nameEnd = '\0';
				*valueEnd = '\0';
				if (!declaration) {
					handler.attribute(zstring_view(name, nameEnd - name),
					                  zstring_view(value, valueEnd - value));
				} else {
					handler.declAttribute(zstring_view(name, nameEnd - name),
					                      zstring_view(value, valueEnd - value));
				}
			} else {
				if (!declaration) {
					handler.attribute(std::string_view(name, nameEnd - name),
					                  std::string_view(value, valueEnd - value));
				} else {
					handler.declAttribute(std::string_view(name, nameEnd - name),
					                      std::string_view(value, valueEnd - value));
				}
			}

			skip<WhitespacePred>(text); // skip ws after value
		}
	}
};

} // namespace internal

template<int FLAGS, typename HANDLER>
inline void parse(HANDLER& handler, char* xml)
{
	internal::Parser<FLAGS, HANDLER> parser(handler, xml);
}

} // namespace rapidsax

#endif
