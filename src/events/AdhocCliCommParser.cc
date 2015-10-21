#include "AdhocCliCommParser.hh"
#include "utf8_unchecked.hh"


AdhocCliCommParser::AdhocCliCommParser(std::function<void(const std::string&)> callback_)
	: callback(std::move(callback_))
	, state(O0)
{
}

void AdhocCliCommParser::parse(const char* buf, size_t n)
{
	for (size_t i = 0; i < n; ++i) parse(buf[i]);
}

void AdhocCliCommParser::parse(char c)
{
	// Whenever there is a parse error we return to the initial state
	switch (state) {
	case O0: // looking for opening tag
		state = (c == '<') ? O1 : O0; break;
	case O1: // matched <
		state = (c == 'c') ? O2 : O0; break;
	case O2: // matched <c
		state = (c == 'o') ? O3 : O0; break;
	case O3: // matched <co
		state = (c == 'm') ? O4 : O0; break;
	case O4: // matched <com
		state = (c == 'm') ? O5 : O0; break;
	case O5: // matched <comm
		state = (c == 'a') ? O6 : O0; break;
	case O6: // matched <comma
		state = (c == 'n') ? O7 : O0; break;
	case O7: // matched <comman
		state = (c == 'd') ? O8 : O0; break;
	case O8: // matched <command
		if (c == '>') {
			state = C0;
			command.clear();
		} else {
			state = O0;
		}
		break;
	case C0: // matched <command>, now parsing xml entities and </command>
		if      (c == '<') state = C1;
		else if (c == '&') state = A1;
		else command += c;
		break;
	case C1: // matched <
		state = (c == '/') ? C2 : O0; break;
	case C2: // matched </
		state = (c == 'c') ? C3 : O0; break;
	case C3: // matched </c
		state = (c == 'o') ? C4 : O0; break;
	case C4: // matched </co
		state = (c == 'm') ? C5 : O0; break;
	case C5: // matched </com
		state = (c == 'm') ? C6 : O0; break;
	case C6: // matched </comm
		state = (c == 'a') ? C7 : O0; break;
	case C7: // matched </comma
		state = (c == 'n') ? C8 : O0; break;
	case C8: // matched </comman
		state = (c == 'd') ? C9 : O0; break;
	case C9: // matched </command
		if (c == '>') callback(command);
		state = O0;
		break;
	case A1: // matched &
		if      (c == 'l') state = L2;
		else if (c == 'a') state = A2;
		else if (c == 'g') state = G2;
		else if (c == 'q') state = Q2;
		else if (c == '#') { state = H2; unicode = 0; }
		else               state = O0; // error
		break;
	case A2: // matched &a
		if      (c == 'm') state = A3;
		else if (c == 'p') state = P3;
		else               state = O0; // error
		break;
	case A3: // matched &am
		state = (c == 'p') ? A4 : O0; break;
	case A4: // matched &amp
		if (c == ';') {
			command += '&';
			state = C0;
		} else {
			state = O0; // error
		}
		break;
	case P3: // matched &ap
		state = (c == 'o') ? P4 : O0; break;
	case P4: // matched &apo
		state = (c == 's') ? P5 : O0; break;
	case P5: // matched &apos
		if (c == ';') {
			command += '\'';
			state = C0;
		} else {
			state = O0; // error
		}
		break;
	case Q2: // matched &q
		state = (c == 'u') ? Q3 : O0; break;
	case Q3: // matched &qu
		state = (c == 'o') ? Q4 : O0; break;
	case Q4: // matched &quo
		state = (c == 't') ? Q5 : O0; break;
	case Q5: // matched &quot
		if (c == ';') {
			command += '"';
			state = C0;
		} else {
			state = O0; // error
		}
		break;
	case G2: // matched &g
		state = (c == 't') ? G3 : O0; break;
	case G3: // matched &gt
		if (c == ';') {
			command += '>';
			state = C0;
		} else {
			state = O0; // error
		}
		break;
	case L2: // matched &l
		state = (c == 't') ? L3 : O0; break;
	case L3: // matched &lt
		if (c == ';') {
			command += '<';
			state = C0;
		} else {
			state = O0; // error
		}
		break;
	case H2: // matched &#
		// This also parses invalid input like '&#12xab;' but let's
		// ignore that. It also doesn't check for overflow etc.
		if (c == ';') {
			utf8::unchecked::append(unicode, back_inserter(command));
			state = C0;
		} else if (c == 'x') {
			state = H3;
		} else {
			unicode *= 10;
			if (('0' <= c) && (c <= '9')) unicode += c - '0';
			else state = O0;
		}
		break;
	case H3: // matched &#x
		if (c == ';') {
			utf8::unchecked::append(unicode, back_inserter(command));
			state = C0;
		} else {
			unicode *= 16;
			if      (('0' <= c) && (c <= '9')) unicode += c - '0';
			else if (('a' <= c) && (c <= 'f')) unicode += c - 'a' + 10;
			else if (('A' <= c) && (c <= 'F')) unicode += c - 'A' + 10;
			else state = O0;
		}
		break;
	}
}



#if 0

#include <iostream>
#include <vector>

using namespace std;

template<typename T>
void print(const T& t)
{
	for (auto& i : t) cout << i << std::endl;
}

void test(const string& stream, const vector<string>& expectedCommands)
{
	vector<string> result;
	AdhocCliCommParser parser([&](const string& cmd) { result.push_back(cmd); });
	parser.parse(stream.data(), stream.size());
	if (result != expectedCommands) {
		cout << "ERROR while parsing " << stream << endl;
		cout << "Expected to see:" << endl;
		print(expectedCommands);
		cout << "but got:" << endl;
		print(result);
	} else {
		cout << "ok" << endl;
	}
}

int main()
{
	test("<command>foo</command>", {"foo"});
	test("  <command>foo</command>", {"foo"});
	test("<command>foo</command>  ", {"foo"});
	test("  <command>foo</command>  ", {"foo"});

	test("<command>foo</command><command>bar</command>", {"foo", "bar"});
	test("<command>foo</command>  <command>bar</command>", {"foo", "bar"});

	test("<command>&amp;</command>",  {"&"});
	test("<command>&apos;</command>", {"'"});
	test("<command>&quot;</command>", {"\""});
	test("<command>&lt;</command>",   {"<"});
	test("<command>&gt;</command>",   {">"});
	test("<command>&#65;</command>",  {"A"});
	test("<command>&#x41;</command>", {"A"});
	test("<command>&lt;command&gt;</command>", {"<command>"});

	test("<openmsx-control> <command>foo</command> </openmsx-control>",
	     {"foo"});
	test("<openmsx-control> <unknown>foo</unknown> <command>foo</command>",
	     {"foo"});

	// errors, but we do recover
	test("<command>&unknown;</command><command>foo</command>", {"foo"});
	test("<command>&#fffffff;</command><command>foo</command>", {"foo"});
	test("<<<<<command>foo</command>", {"foo"});
	test("<foo></bar><command>foo</command>", {"foo"});
	test("</command><command>foo</command>", {"foo"});
	test("<command>foo</foobar><command>foo</command>", {"foo"});

	// This was not accepted before, but now is
	test("<openmsx-control></openmsx-control><command>foo</command>", {"foo"});

	// This was accepted before but now isn't
	test("<command value=\"3\">foo</command>",
	     {}); // should be "foo"
	test("<command><bla></bla>foo</command>",
	     {}); // should be "foo"
	test("<command/>",
	     {}); // sort-of OK, was before an empty command, now no command
}

#endif
