#ifndef COMPLETER_HH
#define COMPLETER_HH

#include "inline.hh"
#include "string_ref.hh"
#include <vector>

namespace openmsx {

class FileContext;
class InterpreterOutput;

class Completer
{
public:
	const std::string& getName() const { return name; }

	/** Print help for this command.
	  */
	virtual std::string help(const std::vector<std::string>& tokens) const = 0;

	/** Attempt tab completion for this command.
	  * @param tokens Tokenized command line;
	  *     tokens[0] is the command itself.
	  *     The last token is incomplete, this method tries to complete it.
	  */
	virtual void tabCompletion(std::vector<std::string>& tokens) const = 0;

	template<typename ITER>
	static void completeString(std::vector<std::string>& tokens,
	                           ITER begin, ITER end,
	                           bool caseSensitive = true);
	template<typename RANGE>
	static void completeString(std::vector<std::string>& tokens,
	                           const RANGE& possibleValues,
	                           bool caseSensitive = true);
	template<typename RANGE>
	static void completeFileName(std::vector<std::string>& tokens,
	                             const FileContext& context,
	                             const RANGE& extra);
	static void completeFileName(std::vector<std::string>& tokens,
	                             const FileContext& context);

	static std::vector<std::string> formatListInColumns(
		const std::vector<string_ref>& input);

	// should only be called by CommandConsole
	static void setOutput(InterpreterOutput* output_) { output = output_; }

protected:
	explicit Completer(string_ref name);
	~Completer() {}

private:
	static bool equalHead(string_ref s1, string_ref s2, bool caseSensitive);
	template<typename ITER>
	static std::vector<string_ref> filter(
		string_ref str, ITER begin, ITER end, bool caseSensitive);
	template<typename RANGE>
	static std::vector<string_ref> filter(
		string_ref str, const RANGE& range, bool caseSensitive);
	static bool completeImpl(std::string& str, std::vector<string_ref> matches,
	                         bool caseSensitive);
	static void completeFileNameImpl(std::vector<std::string>& tokens,
	                                 const FileContext& context,
	                                 std::vector<string_ref> matches);

	const std::string name;
	static InterpreterOutput* output;
};


template<typename ITER>
NEVER_INLINE std::vector<string_ref> Completer::filter(
	string_ref str, ITER begin, ITER end, bool caseSensitive)
{
	std::vector<string_ref> result;
	for (auto it = begin; it != end; ++it) {
		if (equalHead(str, *it, caseSensitive)) {
			result.push_back(*it);
		}
	}
	return result;
}

template<typename RANGE>
inline std::vector<string_ref> Completer::filter(
	string_ref str, const RANGE& range, bool caseSensitive)
{
	return filter(str, std::begin(range), std::end(range), caseSensitive);
}

template<typename RANGE>
void Completer::completeString(
	std::vector<std::string>& tokens,
	const RANGE& possibleValues,
	bool caseSensitive)
{
	auto& str = tokens.back();
	if (completeImpl(str,
	                 filter(str, possibleValues, caseSensitive),
	                 caseSensitive)) {
		tokens.emplace_back();
	}
}

template<typename ITER>
void Completer::completeString(
	std::vector<std::string>& tokens,
	ITER begin, ITER end,
	bool caseSensitive)
{
	auto& str = tokens.back();
	if (completeImpl(str,
	                 filter(str, begin, end, caseSensitive),
	                 caseSensitive)) {
		tokens.emplace_back();
	}
}

template<typename RANGE>
void Completer::completeFileName(
	std::vector<std::string>& tokens,
	const FileContext& context,
	const RANGE& extra)
{
	completeFileNameImpl(tokens, context, filter(tokens.back(), extra, true));
}

} // namespace openmsx

#endif
