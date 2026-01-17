#ifndef COMPLETER_HH
#define COMPLETER_HH

#include "CompletionCandidate.hh"

#include <algorithm>
#include <concepts>
#include <ranges>
#include <span>
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
	Completer(Completer&&) = delete;
	Completer& operator=(const Completer&) = delete;
	Completer& operator=(Completer&&) = delete;

	[[nodiscard]] const std::string& getName() const { return theName; }

	/** Print help for this command.
	  */
	[[nodiscard]] virtual std::string help(std::span<const TclObject> tokens) const = 0;

	/** Attempt tab completion for this command.
	  * @param tokens Tokenized command line;
	  *     tokens[0] is the command itself.
	  *     The last token is incomplete, this method tries to complete it.
	  */
	virtual void tabCompletion(std::vector<std::string>& tokens) const = 0;

	[[nodiscard]] virtual Interpreter& getInterpreter() const = 0;

	template<std::ranges::forward_range Range>
	static void completeString(std::vector<std::string>& tokens,
	                           Range&& possibleValues,
	                           bool caseSensitive = true);
	template<std::ranges::forward_range Range = std::vector<CompletionCandidate>>
	static void completeFileName(std::vector<std::string>& tokens,
	                             const FileContext& context,
	                             const Range& extra = {});

	static std::vector<std::string> formatListInColumns(
		std::span<const std::string_view> input);

	// helper functions to check the number of arguments
	struct AtLeast { unsigned min; };
	struct Between { unsigned min; unsigned max; };
	struct Prefix { unsigned n; }; // how many items from 'tokens' to show in error
	void checkNumArgs(std::span<const TclObject> tokens, unsigned exactly, const char* errMessage) const;
	void checkNumArgs(std::span<const TclObject> tokens, AtLeast atLeast,  const char* errMessage) const;
	void checkNumArgs(std::span<const TclObject> tokens, Between between,  const char* errMessage) const;
	void checkNumArgs(std::span<const TclObject> tokens, unsigned exactly, Prefix prefix, const char* errMessage) const;
	void checkNumArgs(std::span<const TclObject> tokens, AtLeast atLeast,  Prefix prefix, const char* errMessage) const;
	void checkNumArgs(std::span<const TclObject> tokens, Between between,  Prefix prefix, const char* errMessage) const;

	// should only be called by CommandConsole
	static void setOutput(InterpreterOutput* output_) { output = output_; }

protected:
	template<typename String>
		requires(!std::same_as<Completer, std::remove_cvref_t<String>>) // don't block copy-constructor
	explicit Completer(String&& name_)
		: theName(std::forward<String>(name_))
	{
	}

	~Completer() = default;

private:
	static bool equalHead(std::string_view s, const CompletionCandidate& c);
	template<std::ranges::forward_range Range>
	static std::vector<CompletionCandidate> filter(
		std::string_view str, Range&& range, bool caseSensitive = true);
	static void completeImpl(std::vector<std::string>& tokens, std::vector<CompletionCandidate> matches);
	static void completeFileNameImpl(std::vector<std::string>& tokens,
	                                 const FileContext& context,
	                                 std::vector<CompletionCandidate> matches);

	const std::string theName;
	static inline InterpreterOutput* output = nullptr;
};

template<std::ranges::forward_range Range>
std::vector<CompletionCandidate> Completer::filter(
	std::string_view str, Range&& range, bool caseSensitive)
{
	struct MakeCompletionCandidate {
		CompletionCandidate operator()(std::string_view s, bool caseSensitive) const {
			return {.text = std::string(s), .caseSensitive = caseSensitive};
		}
		CompletionCandidate operator()(CompletionCandidate c, bool /*caseSensitive*/) const {
			return c;
		}
	} make;

	std::vector<CompletionCandidate> result;
	for (auto&& value : range) {
		CompletionCandidate c = make(std::forward<decltype(value)>(value), caseSensitive);
		if (equalHead(str, c)) {
			result.push_back(std::move(c));
		}
	}
	return result;
}

template<std::ranges::forward_range Range>
void Completer::completeString(
	std::vector<std::string>& tokens,
	Range&& possibleValues,
	bool caseSensitive)
{
	completeImpl(tokens,
	             filter(tokens.back(), std::forward<Range>(possibleValues), caseSensitive));
}

template<std::ranges::forward_range Range>
void Completer::completeFileName(
	std::vector<std::string>& tokens,
	const FileContext& context,
	const Range& extra)
{
	completeFileNameImpl(tokens, context, filter(tokens.back(), extra));
}

} // namespace openmsx

#endif
