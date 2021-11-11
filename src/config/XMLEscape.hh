#ifndef XMLESCAPE_HH
#define XMLESCAPE_HH

#include <string>
#include <string_view>

// The following routines do the following substitutions:
//  & -> &amp;   must always be done
//  < -> &lt;    must always be done
//  > -> &gt;    always allowed, but must be done when it appears as ]]>
//  ' -> &apos;  always allowed, but must be done inside quoted attribute
//  " -> &quot;  always allowed, but must be done inside quoted attribute
//  all chars less than 32 -> &#xnn;
// So to simplify things we always do these 5+32 substitutions.

// This is a low-level version. It takes an input string and an output-functor.
// That functor is called (possibly) multiple times, each with a (small) chunk
// for the final output (chunk are either parts of the input or xml escape
// sequences).
template<typename Output>
inline void XMLEscape(std::string_view s, Output output)
{
	auto normal = [&](auto f, auto l) {
		// c++20 std::string_view(f, l);
		return std::string_view(&*f, l - f);
	};

	auto chunk = s.begin();
	auto last = s.end();
	auto it = chunk;
	while (it != last) {
		char c = *it;
		if (auto uc = static_cast<unsigned char>(c); uc < 32) {
			output(normal(chunk, it));
			char buf[6] = {'&', '#', 'x', '0', '0', ';'};
			auto hex = [](unsigned x) { return (x < 10) ? char(x + '0') : char(x - 10 + 'a'); };
			buf[3] = hex(uc / 16);
			buf[4] = hex(uc % 16);
			output(std::string_view(buf, sizeof(buf)));
			chunk = ++it;
		} else if (c == '<') {
			output(normal(chunk, it));
			output("&lt;");
			chunk = ++it;
		} else if (c == '>') {
			output(normal(chunk, it));
			output("&gt;");
			chunk = ++it;
		} else if (c == '&') {
			output(normal(chunk, it));
			output("&amp;");
			chunk = ++it;
		} else if (c == '"') {
			output(normal(chunk, it));
			output("&quot;");
			chunk = ++it;
		} else if (c == '\'') {
			output(normal(chunk, it));
			output("&apos;");
			chunk = ++it;
		} else {
			// no output yet
			++it;
		}
	}
	output(normal(chunk, last));
}

// Like above, but produces the output as a (single) std::string.
inline std::string XMLEscape(std::string_view s)
{
	std::string result;
	XMLEscape(s, [&](std::string_view chunk) { result += chunk; });
	return result;
}

#endif
