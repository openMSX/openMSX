#ifndef TCLPARSER_HH
#define TCLPARSER_HH

#include "string_ref.hh"
#include <vector>
#include <tcl.h>

#define DEBUG_TCLPARSER 0

class TclParser
{
public:
	/** Input: Tcl interpreter and command to parse	 */
	TclParser(Tcl_Interp* interp, string_ref input);

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
	std::string getColors() const { return colors; }

	/** Get Start of the last subcommand. This is the command that should
	  * be completed by tab-completion. */
	int getLast() const { return last.back(); }

private:
	enum ParseType { COMMAND, EXPRESSION, OTHER };

	void parse(const char* p, int size, ParseType type);
	void printTokens(Tcl_Token* tokens, int numTokens);
	static ParseType guessSubType(Tcl_Token* tokens, int i);
	bool isProc(string_ref str) const;
	void setColors(const char* p, int size, char c);

private:
	Tcl_Interp* interp;
	std::string colors;
	std::string parseStr;
	std::vector<int> last;
	int offset;

#if DEBUG_TCLPARSER
	void DEBUG_PRINT(const std::string& s);
	int level;
#else
	#define DEBUG_PRINT(x)
#endif
};

#endif
