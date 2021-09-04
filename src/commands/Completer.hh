#ifndef COMPLETER_HH
#define COMPLETER_HH

#include "inline.hh"
#include "span.hh"
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

class FileContext;
class Interpreter;
class InterpreterOutput;
class TclObject;

class Completer
{
public:
	Completer(const Completer&) = delete;
	Completer& operator=(const Completer&) = delete;

	[[nodiscard]] const std::string& getName() const { return theName; }

	/** Print help for this command.
	  */
	[[nodiscard]] virtual std::string help(span<const TclObject> tokens) const = 0;

	/** Attempt tab completion for this command.
	  * @param tokens Tokenized command line;
	  *     tokens[0] is the command itself.
	  *     The last token is incomplete, this method tries to complete it.
	  */
	virtual void tabCompletion(std::vector<std::string>& tokens) const = 0;

	[[nodiscard]] virtual Interpreter& getInterpreter() const = 0;

	template<typename ITER>
	static void completeString(std::vector<std::string>& tokens,
	                           ITER begin, ITER end,
	                           bool caseSensitive = true);
	template<typename RANGE>
	static void completeString(std::vector<std::string>& tokens,
	                           RANGE&& possibleValues,
	                           bool caseSensitive = true);
	template<typename RANGE>
	static void completeFileName(std::vector<std::string>& tokens,
	                             const FileContext& context,
	                             const RANGE& extra);
	static void completeFileName(std::vector<std::string>& tokens,
	                             const FileContext& context);

	static std::vector<std::string> formatListInColumns(
		const std::vector<std::string_view>& input);

	// helper functions to check the number of arguments
	struct AtLeast { unsigned min; };
	struct Between { unsigned min; unsigned max; };
	struct Prefix { unsigned n; }; // how many items from 'tokens' to show in error
	void checkNumArgs(span<const TclObject> tokens, unsigned exactly, const char* errMessage) const;
	void checkNumArgs(span<const TclObject> tokens, AtLeast atLeast,  const char* errMessage) const;
	void checkNumArgs(span<const TclObject> tokens, Between between,  const char* errMessage) const;
	void checkNumArgs(span<const TclObject> tokens, unsigned exactly, Prefix prefix, const char* errMessage) const;
	void checkNumArgs(span<const TclObject> tokens, AtLeast atLeast,  Prefix prefix, const char* errMessage) const;
	void checkNumArgs(span<const TclObject> tokens, Between between,  Prefix prefix, const char* errMessage) const;

	// should only be called by CommandConsole
	static void setOutput(InterpreterOutput* output_) { output = output_; }

protected:
	template<typename String>
	explicit Completer(String&& name_)
		: theName(std::forward<String>(name_))
	{
	}

	~Completer() = default;

private:
	static bool equalHead(std::string_view s1, std::string_view s2, bool caseSensitive);
	template<typename ITER>
	static std::vector<std::string_view> filter(
		std::string_view str, ITER begin, ITER end, bool caseSensitive);
	template<typename RANGE>
	static std::vector<std::string_view> filter(
		std::string_view str, RANGE&& range, bool caseSensitive);
	static bool completeImpl(std::string& str, std::vector<std::string_view> matches,
	                         bool caseSensitive);
	static void completeFileNameImpl(std::vector<std::string>& tokens,
	                                 const FileContext& context,
	                                 std::vector<std::string_view> matches);

	const std::string theName;
	static inline InterpreterOutput* output = nullptr;
};


template<typename ITER>
NEVER_INLINE std::vector<std::string_view> Completer::filter(
	std::string_view str, ITER begin, ITER end, bool caseSensitive)
{
	std::vector<std::string_view> result;
	for (auto it = begin; it != end; ++it) {
		if (equalHead(str, *it, caseSensitive)) {
			result.push_back(*it);
		}
	}
	return result;
}

template<typename RANGE>
inline std::vector<std::string_view> Completer::filter(
	std::string_view str, RANGE&& range, bool caseSensitive)
{
	return filter(str, std::begin(range), std::end(range), caseSensitive);
}

template<typename RANGE>
void Completer::completeString(
	std::vector<std::string>& tokens,
	RANGE&& possibleValues,
	bool caseSensitive)
{
	auto& str = tokens.back();
	if (completeImpl(str,
	                 filter(str, std::forward<RANGE>(possibleValues), caseSensitive),
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
