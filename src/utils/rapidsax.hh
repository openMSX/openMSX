// $Id$

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

#include "string_ref.hh"

namespace rapidsax {

// Parse given XML text and call callback functions in the given handler.
// - XML text must be zero-terminated
// - Handler must implement the methods defined in NullHandler (below). An
//   easy way to do this is to inherit from NullHandler and only reimplement
//   the methods that you need.
// - The behavior of the parser can be fine-tuned with the FLAGS parameter,
//   see below for more details.
// - When a parse error is encounter, an instance of ParseError is thrown.
// - The lifetime of the string_ref's in the callback handler is the same as
//   the lifetime of the input XML data (no string copies are made, instead
//   the XML file is modified in-place and references to this data are passed).
template<int FLAGS, typename HANDLER> void parse(HANDLER& handler, char* xml);


// Flags that influence parsing behavior. The flags can be OR'ed together.

// Should XML entities like &lt; be expanded or not?
static const int noEntityTranslation = 0x1;
// Should leading and trailing whitespace be trimmed?
static const int trimWhitespace      = 0x2;
// Should sequences of whitespace characters be replaced with a single
// space character?
static const int normalizeWhitespace = 0x4;


// Callback handler with all empty implementations (can be used as a base
// class in case you only need to reimplement a few of the methods).
class NullHandler
{
public:
	// Called when an opening XML tag is encountered.
	// 'name' is the name of the XML tag.
	void start(string_ref /*name*/) {}

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
	void text(string_ref /*text*/) {}

	// Called for each parsed attribute.
	// Attributes can occur inside xml tags or inside XML declarations.
	void attribute(string_ref /*name*/, string_ref /*value*/) {}

	// Called for parsed CDATA sections.
	void cdata(string_ref /*value*/) {}

	// Called when a XML comment (<!-- ... -->) is parsed.
	void comment(string_ref /*value*/) {}

	// Called when XML declaration (<?xml .. ?>) is parsed.
	// Inside a XML declaration there can be attributes.
	void declarationStart() {}
	void declarationStop() {}

	// Called when the <!DOCTYPE ..> is parsed.
	void doctype(string_ref /*text*/) {}

	// Called when XML processing instructions (<? .. ?>) are parsed.
	void procInstr(string_ref /*target*/, string_ref /*instr*/) {}
};


class ParseError
{
public:
	ParseError(const char* what, char* where)
		: m_what(what)
		, m_where(where)
	{
	}

	const char* what() const { return m_what; }
	char* where() const { return m_where; }

private:
	const char* m_what;
	char* m_where;
};


namespace internal {

typedef unsigned char u8;

extern const bool lutWhitespace[256];         // Whitespace table
extern const bool lutNodeName[256];           // Node name table
extern const bool lutText[256];               // Text table
extern const bool lutTextPureNoWs[256];       // Text table
extern const bool lutTextPureWithWs[256];     // Text table
extern const bool lutAttributeName[256];      // Attribute name table
extern const bool lutAttributeData1[256];     // Attribute data table, single quote
extern const bool lutAttributeData1Pure[256]; // Attribute data table, single quote
extern const bool lutAttributeData2[256];     // Attribute data table, double quotes
extern const bool lutAttributeData2Pure[256]; // Attribute data table, double quotes
extern const u8   lutDigits[256];             // Digits

// Detect whitespace character
struct WhitespacePred {
	static bool test(char ch) {
		return lutWhitespace[u8(ch)];
	}
};

// Detect node name character
struct NodeNamePred {
	static bool test(char ch) {
		return lutNodeName[u8(ch)];
	}
};

// Detect attribute name character
struct AttributeNamePred {
	static bool test(char ch) {
		return lutAttributeName[u8(ch)];
	}
};

// Detect text character (PCDATA)
struct TextPred {
	static bool test(char ch) {
		return lutText[u8(ch)];
	}
};

// Detect text character (PCDATA) that does not require processing
struct TextPureNoWsPred {
	static bool test(char ch) {
		return lutTextPureNoWs[u8(ch)];
	}
};

// Detect text character (PCDATA) that does not require processing
struct TextPureWithWsPred {
	static bool test(char ch) {
		return lutTextPureWithWs[u8(ch)];
	}
};

// Detect attribute value character (single quote)
struct AttPred1 {
	static bool test(char ch) {
		return lutAttributeData1[u8(ch)];
	}
};
// Detect attribute value character (double quote)
struct AttPred2 {
	static bool test(char ch) {
		return lutAttributeData2[u8(ch)];
	}
};

// Detect attribute value character (single quote)
struct AttPurePred1 {
	static bool test(char ch) {
		return lutAttributeData1Pure[u8(ch)];
	}
};
// Detect attribute value character (double quote)
struct AttPurePred2 {
	static bool test(char ch) {
		return lutAttributeData2Pure[u8(ch)];
	}
};

// Insert coded character, using UTF8
static inline void insertUTF8char(char*& text, unsigned long code)
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

template<char C0, char C1> static inline bool next(const char* p)
{
	return (p[0] == C0) && (p[1] == C1);
}
template<char C0, char C1, char C2> static inline bool next(const char* p)
{
	return (p[0] == C0) && (p[1] == C1) && (p[2] == C2);
}
template<char C0, char C1, char C2, char C3> static inline bool next(const char* p)
{
	return (p[0] == C0) && (p[1] == C1) && (p[2] == C2) && (p[3] == C3);
}
template<char C0, char C1, char C2, char C3, char C4, char C5>
static inline bool next(const char* p)
{
	return (p[0] == C0) && (p[1] == C1) && (p[2] == C2) &&
	       (p[3] == C3) && (p[4] == C4) && (p[5] == C5);
}


// Skip characters until predicate evaluates to true
template<class StopPred> static inline void skip(char*& text)
{
	char* tmp = text;
	while (StopPred::test(*tmp)) ++tmp;
	text = tmp;
}

// Skip characters until predicate evaluates to true while doing the following:
// - replacing XML character entity references with proper characters
//   (&apos; &amp; &quot; &lt; &gt; &#...;)
// - condensing whitespace sequences to single space character
template<class StopPred, class StopPredPure, int FLAGS>
static inline char* skipAndExpand(char*& text)
{
	// If entity translation, whitespace condense and whitespace
	// trimming is disabled, use plain skip.
	if ( (FLAGS & noEntityTranslation) &&
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
				if (next<'m','p',';'>(&src[2])) {
					*dest = '&';
					++dest;
					src += 5;
					continue;
				}
				if (next<'p','o','s',';'>(&src[2])) {
					*dest = '\'';
					++dest;
					src += 6;
					continue;
				}
				break;

			case 'q': // &quot;
				if (next<'u','o','t',';'>(&src[2])) {
					*dest = '"';
					++dest;
					src += 6;
					continue;
				}
				break;

			case 'g': // &gt;
				if (next<'t',';'>(&src[2])) {
					*dest = '>';
					++dest;
					src += 4;
					continue;
				}
				break;

			case 'l': // &lt;
				if (next<'t',';'>(&src[2])) {
					*dest = '<';
					++dest;
					src += 4;
					continue;
				}
				break;

			case '#': // &#...; - assumes ASCII
				if (src[2] == 'x') {
					unsigned long code = 0;
					src += 3; // skip &#x
					while (true) {
						u8 digit = lutDigits[u8(*src)];
						if (digit == 0xFF) break;
						code = code * 16 + digit;
						++src;
					}
					insertUTF8char(dest, code);
				} else {
					unsigned long code = 0;
					src += 2; // skip &#
					while (1) {
						   u8 digit = lutDigits[u8(*src)];
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

static inline void skipBOM(char*& text)
{
	if (next<char(0xEF), char(0xBB), char(0xBF)>(text)) {
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
		parseAttributes(text);
		handler.declarationStop();

		// skip ?>
		if (!next<'?','>'>(text)) {
			throw ParseError("expected ?>", text);
		}
		text += 2;
	}

	// Parse XML comment (<!--...)
	void parseComment(char*& text)
	{
		// Skip until end of comment
		char* value = text; // remember value start
		while (!next<'-','-','>'>(text)) {
			if (text[0] == 0) {
				throw ParseError("unexpected end of data", text);
			}
			++text;
		}
		handler.comment(string_ref(value, text));
		text += 3; // skip '-->'
	}

	void parseDoctype(char*& text)
	{
		char* value = text; // remember value start

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

		handler.doctype(string_ref(value, text));
		text += 1; // skip '>'
	}

	void parsePI(char*& text)
	{
		// Extract PI target name
		char* name = text;
		skip<NodeNamePred>(text);
		char* nameEnd = text;
		if (name == nameEnd) {
			throw ParseError("expected PI target", text);
		}

		// Skip whitespace between pi target and pi
		skip<WhitespacePred>(text);

		// Skip to '?>'
		char* value = text; // Remember start of pi
		while (!next<'?','>'>(text)) {
			if (*text == 0) {
				throw ParseError("unexpected end of data", text);
			}
			++text;
		}
		// Set pi value (verbatim, no entity expansion or ws normalization)
		handler.procInstr(string_ref(name,  nameEnd),
			          string_ref(value, text));
		text += 2; // skip '?>'
	}

	void parseText(char*& text, char* contentsStart)
	{
		// Backup to contents start if whitespace trimming is disabled
		if (!(FLAGS & trimWhitespace)) {
			text = contentsStart;
		}
		// Skip until end of data
		char* value = text;
		char* end = (FLAGS & normalizeWhitespace)
			? skipAndExpand<TextPred, TextPureWithWsPred, FLAGS>(text)
			: skipAndExpand<TextPred, TextPureNoWsPred  , FLAGS>(text);

		// Trim trailing whitespace; leading was already trimmed by
		// whitespace skip after >
		if (FLAGS & trimWhitespace) {
			if (FLAGS & normalizeWhitespace) {
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

		// Handle text, but only if non-empty.
		unsigned len = end - value;
		if (len) handler.text(string_ref(value, len));
	}

	void parseCdata(char*& text)
	{
		// Skip until end of cdata
		char* value = text;
		while (!next<']',']','>'>(text)) {
			if (text[0] == 0) {
				throw ParseError("unexpected end of data", text);
			}
			++text;
		}
		handler.cdata(string_ref(value, text));
		text += 3; // skip ]]>
	}

	void parseElement(char*& text)
	{
		// Extract element name
		char* name = text;
		skip<NodeNamePred>(text);
		char* nameEnd = text;
		if (name == nameEnd) {
			throw ParseError("expected element name", text);
		}
		handler.start(string_ref(name, nameEnd));

		skip<WhitespacePred>(text); // skip ws before attributes or >
		parseAttributes(text);

		// Determine ending type
		if (*text == '>') {
			++text;
			parseNodeContents(text);
		} else if (*text == '/') {
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
			if ((next<'x','m','l'>(text) ||
			     next<'X','M','L'>(text)) &&
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
				if (next<'C','D','A','T','A','['>(&text[2])) {
					// '<![CDATA[' - cdata
					text += 8; // skip '![CDATA['
					parseCdata(text);
					return;
				}
				break;

			case 'D': // <!D
				if (next<'O','C','T','Y','P','E'>(&text[2]) &&
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

afterText:		// After parseText() jump here instead of continuing
			// the loop, because skipping whitespace is unnecessary.
			switch (*text) {
			case '<': // Node closing or child node
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
	void parseAttributes(char*& text)
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
			if (quote != '\'' && quote != '"') {
				throw ParseError("expected ' or \"", text);
			}
			++text;

			// Extract attribute value and expand char refs in it
			// No whitespace normalization in attributes
			static const int FLAGS2 = FLAGS & ~normalizeWhitespace;
			char* value = text;
			char* valueEnd = (quote == '\'')
				? skipAndExpand<AttPred1, AttPurePred1, FLAGS2>(text)
				: skipAndExpand<AttPred2, AttPurePred2, FLAGS2>(text);
			handler.attribute(string_ref(name, nameEnd),
			                  string_ref(value, valueEnd));

			// Make sure that end quote is present
			if (*text != quote) {
				throw ParseError("expected ' or \"", text);
			}
			++text; // skip quote

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
