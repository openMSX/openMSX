#include "Completer.hh"

#include "Interpreter.hh"
#include "InterpreterOutput.hh"
#include "TclObject.hh"

#include "FileContext.hh"
#include "FileOperations.hh"
#include "foreach_file.hh"

#include "ranges.hh"
#include "stl.hh"
#include "strCat.hh"
#include "stringsp.hh"
#include "utf8_unchecked.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>
#include <memory>

namespace openmsx {

static bool formatHelper(std::span<const std::string_view> input, size_t columnLimit,
                         std::vector<std::string>& result)
{
	size_t column = 0;
	auto it = begin(input);
	do {
		size_t maxColumn = column;
		for (size_t i = 0; (i < result.size()) && (it != end(input));
		     ++i, ++it) {
			auto curSize = utf8::unchecked::size(result[i]);
			strAppend(result[i], spaces(column - curSize), *it);
			maxColumn = std::max(maxColumn,
			                     utf8::unchecked::size(result[i]));
			if (maxColumn > columnLimit) return false;
		}
		column = maxColumn + 2;
	} while (it != end(input));
	return true;
}

static std::vector<std::string> format(std::span<const std::string_view> input, size_t columnLimit)
{
	std::vector<std::string> result;
	for (auto lines : xrange(1u, input.size())) {
		result.assign(lines, std::string());
		if (formatHelper(input, columnLimit, result)) {
			return result;
		}
	}
	append(result, input);
	return result;
}

std::vector<std::string> Completer::formatListInColumns(std::span<const std::string_view> input)
{
	return format(input, output->getOutputColumns() - 1);
}

bool Completer::equalHead(std::string_view s, const CompletionCandidate& c)
{
	if (c.text.size() < s.size()) return false;
	if (c.caseSensitive) {
		return std::ranges::equal(s, subspan(c.text, 0, s.size()));
	} else {
		return strncasecmp(s.data(), c.text.data(), s.size()) == 0;
	}
}

void Completer::completeImpl(std::vector<std::string>& tokens, std::vector<CompletionCandidate> matches)
{
	auto& str = tokens.back();
	assert(std::ranges::all_of(matches, [&](auto& m) { return equalHead(str, m); }));

	if (matches.empty()) {
		// no matching values
		return;
	}
	if (matches.size() == 1) {
		// only one match
		auto& m = matches.front();
		str = m.text;
		if (!m.partial) {
			tokens.emplace_back();
		}
		return;
	}

	// Sort and remove duplicates.
	//  For efficiency it's best if the list doesn't contain duplicates to
	//  start with. Though sometimes this is hard to avoid. E.g. when doing
	//  filename completion + some extra allowed strings and one of these
	//  extra strings is the same as one of the filenames.
	std::ranges::sort(matches, {}, &CompletionCandidate::text);
	auto u = std::ranges::unique(matches, {}, &CompletionCandidate::text);
	matches.erase(u.begin(), u.end());

	auto minsize_of_matches = std::ranges::min(
		matches, {}, [](const auto& m) { return m.text.size(); }).text.size();

	bool expanded = false;
	while (str.size() < minsize_of_matches) {
		auto it = begin(matches);
		auto b = begin(it->text);
		auto e = b + str.size();
		utf8::unchecked::next(e);
		std::string_view test_str(b, e);
		if (!std::ranges::all_of(matches, [&](auto& val) { return equalHead(test_str, val); })) {
			break;
		}
		str = test_str;
		expanded = true;
	}
	if (!expanded && output) {
		output->setCompletions(matches);
	}
}

void Completer::completeFileNameImpl(std::vector<std::string>& tokens,
                                     const FileContext& context,
                                     std::vector<CompletionCandidate> matches)
{
	std::string& filename = tokens.back();
	filename = FileOperations::expandTilde(std::move(filename));
	filename = FileOperations::expandCurrentDirFromDrive(std::move(filename));
	std::string_view dirname1 = FileOperations::getDirName(filename);

	std::span<const std::string> paths;
	if (FileOperations::isAbsolutePath(filename)) {
		static const std::array<std::string, 1> EMPTY = {""};
		paths = EMPTY;
	} else {
		paths = context.getPaths();
	}

	for (const auto& p : paths) {
		auto pLen = p.size();
		if (!p.empty() && (p.back() != '/')) ++pLen;

		auto fileAction = [&](std::string_view path, bool isDir = false) {
			const auto& nm = FileOperations::getConventionalPath(
				std::string(path.substr(pLen)));
			CompletionCandidate c = {.text = nm};
			if (isDir) c.text += '/';
			if (equalHead(filename, c)) {
				c.display_ = FileOperations::getFilename(nm);
				if (isDir) {
					c.display_ += '/';
					c.partial = true;
				}
				matches.push_back(std::move(c));
			}
		};
		auto dirAction = [&](std::string& path) {
			fileAction(path, true);
		};

		foreach_file_and_directory(
			FileOperations::join(p, dirname1),
			fileAction, dirAction);
	}
	completeImpl(tokens, std::move(matches));
}

void Completer::checkNumArgs(std::span<const TclObject> tokens, unsigned exactly, const char* errMessage) const
{
	checkNumArgs(tokens, exactly, Prefix{exactly - 1}, errMessage);
}

void Completer::checkNumArgs(std::span<const TclObject> tokens, AtLeast atLeast, const char* errMessage) const
{
	checkNumArgs(tokens, atLeast, Prefix{atLeast.min - 1}, errMessage);
}

void Completer::checkNumArgs(std::span<const TclObject> tokens, Between between, const char* errMessage) const
{
	checkNumArgs(tokens, between, Prefix{between.min - 1}, errMessage);
}

void Completer::checkNumArgs(std::span<const TclObject> tokens, unsigned exactly, Prefix prefix, const char* errMessage) const
{
	if (tokens.size() == exactly) return;
	getInterpreter().wrongNumArgs(prefix.n, tokens, errMessage);
}

void Completer::checkNumArgs(std::span<const TclObject> tokens, AtLeast atLeast, Prefix prefix, const char* errMessage) const
{
	if (tokens.size() >= atLeast.min) return;
	getInterpreter().wrongNumArgs(prefix.n, tokens, errMessage);
}

void Completer::checkNumArgs(std::span<const TclObject> tokens, Between between, Prefix prefix, const char* errMessage) const
{
	if (tokens.size() >= between.min && tokens.size() <= between.max) return;
	getInterpreter().wrongNumArgs(prefix.n, tokens, errMessage);
}

} // namespace openmsx
