#ifndef TCLPARSER_HH
#define TCLPARSER_HH

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <tcl.h>

#define DEBUG_TCLPARSER 0

class TclParser
{
public:
	/** Input: Tcl interpreter and command to parse	 */
	TclParser(Tcl_Interp* interp, std::string_view input);

	/** Ouput: a string of equal length of the input command where
	  * each character indicates the type of the corresponding
	  * character in the command. Currently the possible colors are:
	  *  'E' -> error
	  *  'c' -> comment
	  *  'v' -> variable
	  *  'l' -> literal (string or number)
	  *  'p' -> proc
	  *  'o' -> operator
	  *  '.' -> other
	  */
	[[nodiscard]] const std::string& getColors() const { return colors; }

	/** Get Start of the last subcommand. This is the command that should
	  * be completed by tab-completion. */
	[[nodiscard]] int getLast() const { return last.back(); }

	/** Is the given string a valid Tcl command. This also takes the
	  * 'lazy' Tcl scripts into account. */
	[[nodiscard]] static bool isProc(Tcl_Interp* interp, std::string_view str);

private:
	enum ParseType : uint8_t { COMMAND, EXPRESSION, OTHER };

	void parse(const char* p, int size, ParseType type);
	void printTokens(std::span<const Tcl_Token> tokens);
	[[nodiscard]] static ParseType guessSubType(std::span<const Tcl_Token> tokens, size_t i);
	void setColors(const char* p, int size, char c);

private:
	Tcl_Interp* interp;
	std::string colors;
	std::string parseStr;
	std::vector<int> last;
	int offset = 0;

#if DEBUG_TCLPARSER
	void DEBUG_PRINT(const std::string& s);
	int level = 0;
#else
	#define DEBUG_PRINT(x)
#endif
};

#endif
