#ifndef COMPLETER_HH
#define COMPLETER_HH

#include "inline.hh"

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
	template<std::ranges::forward_range Range>
	static void completeFileName(std::vector<std::string>& tokens,
	                             const FileContext& context,
	                             const Range& extra);
	static void completeFileName(std::vector<std::string>& tokens,
	                             const FileContext& context);

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
	static bool equalHead(std::string_view s1, std::string_view s2, bool caseSensitive);
	template<std::ranges::forward_range Range>
	static std::vector<std::string_view> filter(
		std::string_view str, Range&& range, bool caseSensitive);
	static bool completeImpl(std::string& str, std::vector<std::string_view> matches,
	                         bool caseSensitive);
	static void completeFileNameImpl(std::vector<std::string>& tokens,
	                                 const FileContext& context,
	                                 std::vector<std::string_view> matches);

	const std::string theName;
	static inline InterpreterOutput* output = nullptr;
};

template<std::ranges::forward_range Range>
std::vector<std::string_view> Completer::filter(
	std::string_view str, Range&& range, bool caseSensitive)
{
	std::vector<std::string_view> result;
	std::ranges::copy_if(std::forward<Range>(range),
	                     std::back_inserter(result),
	                     [&](auto value) { return equalHead(str, value, caseSensitive); });
	return result;
}

template<std::ranges::forward_range Range>
void Completer::completeString(
	std::vector<std::string>& tokens,
	Range&& possibleValues,
	bool caseSensitive)
{
	auto& str = tokens.back();
	if (completeImpl(str,
	                 filter(str, std::forward<Range>(possibleValues), caseSensitive),
	                 caseSensitive)) {
		tokens.emplace_back();
	}
}

template<std::ranges::forward_range Range>
void Completer::completeFileName(
	std::vector<std::string>& tokens,
	const FileContext& context,
	const Range& extra)
{
	completeFileNameImpl(tokens, context, filter(tokens.back(), extra, true));
}

} // namespace openmsx

#endif
