#include "TclParser.hh"
#include "ScopedAssign.hh"
#include <iostream>
#include <cassert>
#include <cstdlib>

using std::string;

#if DEBUG_TCLPARSER
void TclParser::DEBUG_PRINT(const string& s)
{
	std::cout << string(2 * level, ' ') << s << std::endl;
}

static string_ref type2string(int type)
{
	switch (type) {
	case TCL_TOKEN_WORD:
		return "word";
	case TCL_TOKEN_SIMPLE_WORD:
		return "simple word";
	case TCL_TOKEN_EXPAND_WORD:
		return "expand word";
	case TCL_TOKEN_TEXT:
		return "text";
	case TCL_TOKEN_BS:
		return "bs";
	case TCL_TOKEN_COMMAND:
		return "command";
	case TCL_TOKEN_VARIABLE:
		return "variable";
	case TCL_TOKEN_SUB_EXPR:
		return "sub expr";
	case TCL_TOKEN_OPERATOR:
		return "operator";
	default:
		assert(false);
		return "";
	}
}
#endif

static bool isNumber(string_ref str)
{
	unsigned idx;
	stoll(str, &idx);
	return idx == str.size();
}


TclParser::TclParser(Tcl_Interp* interp_, string_ref input)
	: interp(interp_)
	, colors(input.size(), '.')
	, parseStr(input.str())
	, offset(0)
#if DEBUG_TCLPARSER
	, level(0)
#endif
{
	parse(parseStr.data(), int(parseStr.size()), COMMAND);
}

void TclParser::parse(const char* p, int size, ParseType type)
{
	ScopedAssign<int>    sa1(offset, offset + (p - parseStr.data()));
	ScopedAssign<string> sa2(parseStr, string(p, size));
	last.push_back(offset);

	// The functions Tcl_ParseCommand() and Tcl_ParseExpr() are meant to
	// operate on a complete command. For interactive syntax highlighting
	// we also want to pass incomplete commands (e.g. with an opening, but
	// not yet a closing brace). This loop tries to parse and depening on
	// the parse error retries with a completed command.
	Tcl_Parse parseInfo;
	int retryCount = 0;
	while (true) {
		int parseStatus = (type == EXPRESSION)
			? Tcl_ParseExpr(interp, parseStr.data(), int(parseStr.size()), &parseInfo)
			: Tcl_ParseCommand(interp, parseStr.data(), int(parseStr.size()), 1, &parseInfo);
		if (parseStatus == TCL_OK) break;
		Tcl_FreeParse(&parseInfo);
		++retryCount;

		bool allowComplete = ((offset + parseStr.size()) >= colors.size()) &&
		                     (retryCount < 10);
		Tcl_Obj* resObj = Tcl_GetObjResult(interp);
		int resLen;
		const char* resStr = Tcl_GetStringFromObj(resObj, &resLen);
		string_ref error(resStr, resLen);

		if (allowComplete && error.starts_with("missing close-brace")) {
			parseStr += '}';
		} else if (allowComplete && error.starts_with("missing close-bracket")) {
			parseStr += ']';
		} else if (allowComplete && error.starts_with( "missing \"")) {
			parseStr += '"';
		} else if (allowComplete && error.starts_with("unbalanced open paren")) {
			parseStr += ')';
		} else if (allowComplete && error.starts_with("missing operand")) {
			// This also triggers for a (wrong) expression like
			//    'if { / 3'
			// and that can't be solved by adding something at the
			// end. Without the retryCount stuff we would get in an
			// infinte loop here.
			parseStr += '0';
		} else if (allowComplete && error.starts_with("missing )")) {
			parseStr += ')';
		} else {
			DEBUG_PRINT("ERROR: " + parseStr + ": " + error);
			setColors(parseStr.data(), int(parseStr.size()), 'E');
			if ((offset + size) < int(colors.size())) last.pop_back();
			return;
		}
	}

	if (type == EXPRESSION) {
		DEBUG_PRINT("EXPRESSION: " + parseStr);
	} else {
		if (parseInfo.commentSize) {
			DEBUG_PRINT("COMMENT: " + string_ref(parseInfo.commentStart, parseInfo.commentSize));
			setColors(parseInfo.commentStart, parseInfo.commentSize, 'c');
		}
		DEBUG_PRINT("COMMAND: " + string_ref(parseInfo.commandStart, parseInfo.commandSize));
	}
	printTokens(parseInfo.tokenPtr, parseInfo.numTokens);

	// If the current sub-command stops before the end of the original
	// full command, then it's not the last sub-command. Note that
	// sub-commands can be nested.
	if ((offset + size) < int(colors.size())) last.pop_back();

	const char* nextStart = parseInfo.commandStart + parseInfo.commandSize;
	Tcl_FreeParse(&parseInfo);

	if (type == COMMAND) {
		// next command
		int nextSize = int((parseStr.data() + parseStr.size()) - nextStart);
		if (nextSize > 0) {
			parse(nextStart, nextSize, type);
		}
	}
}

void TclParser::printTokens(Tcl_Token* tokens, int numTokens)
{
#if DEBUG_TCLPARSER
	ScopedAssign<int> sa(level, level + 1);
#endif
	for (int i = 0; i < numTokens; /**/) {
		Tcl_Token& token = tokens[i];
		string_ref tokenStr(token.start, token.size);
		DEBUG_PRINT(type2string(token.type) + " -> " + tokenStr);
		switch (token.type) {
		case TCL_TOKEN_VARIABLE:
			assert(token.numComponents >= 1);
			setColors(tokens[i + 1].start - 1, tokens[i + 1].size + 1, 'v');
			break;
		case TCL_TOKEN_WORD:
		case TCL_TOKEN_SIMPLE_WORD:
			if (*token.start == '"') {
				setColors(token.start, token.size, 'l');
			}
			if ((i == 0) && isProc(tokenStr)) {
				setColors(token.start, token.size, 'p');
			}
			break;
		case TCL_TOKEN_EXPAND_WORD:
			setColors(token.start, 3, 'o');
			break;
		case TCL_TOKEN_OPERATOR:
		case TCL_TOKEN_BS:
			setColors(token.start, token.size, 'o');
			break;
		case TCL_TOKEN_TEXT:
			if (isNumber(tokenStr) || (*token.start == '"')) {
				// TODO only works if the same as 'l'
				setColors(token.start, token.size, 'l');
			}
			break;
		}
		if (token.type == TCL_TOKEN_COMMAND) {
			parse(token.start + 1, token.size - 2, COMMAND);
		} else if (token.type == TCL_TOKEN_SIMPLE_WORD) {
			ParseType subType = guessSubType(tokens, i);
			if (subType != OTHER) {
				parse(tokens[i + 1].start, tokens[i + 1].size, subType);
			}
		}
		printTokens(&tokens[++i], token.numComponents);
		i += token.numComponents;
	}
}

TclParser::ParseType TclParser::guessSubType(Tcl_Token* tokens, int i)
{
	// heuristic: if previous token is 'if' then assume this is an expression
	if ((i >= 1) && (tokens[i - 1].type == TCL_TOKEN_TEXT)) {
		string_ref prevText(tokens[i - 1].start, tokens[i - 1].size);
		if ((prevText == "if") ||
		    (prevText == "elseif") ||
		    (prevText == "expr")) {
			return EXPRESSION;
		}
	}

	// heuristic: parse text that starts with { as a subcommand
	if (*tokens[i].start == '{') {
		return COMMAND;
	}

	// a plain text element
	return OTHER;
}

bool TclParser::isProc(string_ref str) const
{
	return Tcl_FindCommand(interp, str.str().c_str(), NULL, 0) != NULL;
}

void TclParser::setColors(const char* p, int size, char c)
{
	int start = (p - parseStr.data()) + offset;
	int stop = std::min(start + size, int(colors.size()));
	for (int i = start; i < stop; ++i) {
		colors[i] = c;
	}
}
