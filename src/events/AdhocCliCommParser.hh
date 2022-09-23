#ifndef ADHOCCLICOMMPARSER_HH
#define ADHOCCLICOMMPARSER_HH

#include <cstdint>
#include <functional>
#include <span>
#include <string>

class AdhocCliCommParser
{
public:
	explicit AdhocCliCommParser(std::function<void(const std::string&)> callback);
	void parse(std::span<const char> buf);

private:
	void parse(char c);

	std::function<void(const std::string&)> callback;
	std::string command;
	uint32_t unicode;
	enum State {
		O0, // no tag char matched yet
		O1, // matched <
		O2, //         <c
		O3, //         <co
		O4, //         <com
		O5, //         <comm
		O6, //         <comma
		O7, //         <comman
		O8, //         <command
		C0, // matched <command>, now parsing xml entities and </command>
		C1, // matched <
		C2, //         </
		C3, //         </c
		C4, //         </co
		C5, //         </com
		C6, //         </comm
		C7, //         </comma
		C8, //         </comman
		C9, //         </command
		A1, // matched &
		A2, //         &a
		A3, //         &am
		A4, //         &amp
		P3, // matched &ap
		P4, //         &apo
		P5, //         &apos
		Q2, // matched &q
		Q3, //         &qu
		Q4, //         &quo
		Q5, //         &quot
		G2, // matched &g
		G3, //         &gt
		L2, // matched &l
		L3, //         &lt
		H2, // matched &#
		H3, //         &#x
	} state;
};

#endif
