#ifndef XMLOUTPUTSTREAM_HH
#define XMLOUTPUTSTREAM_HH

#include "XMLEscape.hh"

#include "ranges.hh"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/**
 * 'XMLOutputStream' is a helper to write an XML file in a streaming way.
 *
 * That is: all information must be provided in the exact same order as it
 * appears in the output. So attributes for a tag must be provided before the
 * data for that tag. It's not possible to go back to an earlier tag and
 * add/change/remove attributes or data, nor is it possible to add/remove
 * subtags to already streamed tags.
 *
 * In other words: this helper class immediately write the provided data to the
 * output stream, and it cannot make changes to already outputted data. So
 * basically the only thing this class does is take care of the syntax details
 * and formatting of the XML file. The main advantage of this class is that it's
 * very cheap: it does not need to keep (much) state in memory (e.g. it does not
 * build the whole XML tree in memory and then dump it all at the end).
 *
 * One limitation of this helper is that it does not support mixed-content tags.
 * A tag either contains data or it contains subtags, but not both (neither data
 * nor subtags is fine).
 *
 * ---
 *
 * After instantiation of the 'XMLOutputStream', you must call the
 *     begin(), attribute(), data(), end()
 * methods. These directly correspond to elements in the XML file, so the
 * meaning should be clear. There are also constraints on the order in which
 * these methods are allowed to be called:
 * - begin() and end() calls should be properly paired. That is for each begin()
 *   there must be a corresponding end() and the 'tag' parameter for both must
 *   match. One pair must be either fully nested inside another pair or it must
 *   be fully outside that other pair.
 * - attribute() must be called directly after begin() or after another
 *   attribute() call, but not after data() or end()
 * - data() can only be called once per tag, it must be called after begin()
 *   or after attribute(), but not after data() or end().
 * - A call to data() must be followed by a call to end(). Not another data()
 *   call nor attribute() nor begin().
 *
 * ---
 *
 * 'XMLOutputStream' is templatized on an 'Operations' type. This must be a class
 * that implements the following 3 methods:
 *     struct MyOperations {
 *         void write(const char* buf, size_t len);
 *         void write1(char c);
 *         void check(bool condition) const;
 *     };
 *
 * The write() and write1() methods write data (e.g. to a file). Either a whole
 * buffer or a single character. In some implementations it's more efficient to
 * write a single char than to write a buffer of length 1, but in your
   implementation feel free to implement write1() as:
 *     write1(char c) { write(&c, 1); }
 *
 * The check() method should be implemented as:
 *     assert(condition);
 * Though in the unittests we instead make this throw an exception so that we
 * can test the error cases.
 */
template<typename Operations>
class XMLOutputStream
{
public:
	explicit XMLOutputStream(Operations& ops_)
		: ops(ops_) {}

	void begin(std::string_view tag);
	void attribute(std::string_view name, std::string_view value);
	void data(std::string_view value);
	void end(std::string_view tag);

	void with_tag(std::string_view tag, std::invocable auto next);

private:
	void writeSpaces(unsigned n);
	void writeChar(char c);
	void writeString(std::string_view s);
	void writeEscapedString(std::string_view s);

private:
	Operations& ops;
	unsigned level = 0;
	enum State : uint8_t {
		INDENT, // the next (begin or end) tag needs to be indented
		CLOSE,  // the begin tag still needs to be closed
		DATA,   // we got data, but no end-tag yet
	};
	State state = INDENT;
#ifdef DEBUG
	std::vector<std::string> stack;
#endif
};


template<typename Writer>
void XMLOutputStream<Writer>::begin(std::string_view tag)
{
#ifdef DEBUG
	stack.emplace_back(tag);
#endif
	ops.check(state != DATA);
	if (state == CLOSE) {
		writeString(">\n");
	}
	writeSpaces(2 * level);
	writeChar('<');
	writeString(tag);
	++level;
	state = CLOSE;
}

template<typename Writer>
void XMLOutputStream<Writer>::attribute(std::string_view name, std::string_view value)
{
	ops.check(level > 0);
	ops.check(state == CLOSE);
	writeChar(' ');
	writeString(name);
	writeString("=\"");
	writeEscapedString(value);
	writeChar('"');
}

template<typename Writer>
void XMLOutputStream<Writer>::data(std::string_view value)
{
	ops.check(level > 0);
	ops.check(state == CLOSE);

	if (value.empty()) return;

	writeChar('>');
	writeEscapedString(value);
	state = DATA;
}

template<typename Writer>
void XMLOutputStream<Writer>::end(std::string_view tag)
{
#ifdef DEBUG
	ops.check(stack.size() == level);
	ops.check(!stack.empty());
	ops.check(stack.back() == tag);
	stack.pop_back();
#endif
	ops.check(level > 0);
	--level;
	if (state == CLOSE) {
		writeString("/>\n");
	} else {
		if (state == INDENT) {
			writeSpaces(2 * level);
		}
		writeString("</");
		writeString(tag);
		writeString(">\n");
	}
	state = INDENT;
}

template<typename Writer>
void XMLOutputStream<Writer>::with_tag(std::string_view tag, std::invocable auto next)
{
	begin(tag);
	next();
	end(tag);
}

template<typename Writer>
void XMLOutputStream<Writer>::writeSpaces(unsigned n)
{
	static constexpr std::array<char, 64> spaces = {
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
	};
	while (n) {
		auto t = std::min(64u, n);
		ops.write(subspan(spaces, 0, t));
		n -= t;
	}
}

template<typename Writer>
void XMLOutputStream<Writer>::writeChar(char c)
{
	ops.write1(c);
}

template<typename Writer>
void XMLOutputStream<Writer>::writeString(std::string_view s)
{
	ops.write(s);
}

template<typename Writer>
void XMLOutputStream<Writer>::writeEscapedString(std::string_view s)
{
	XMLEscape(s, [&](std::string_view chunk) { writeString(chunk); });
}

#endif
