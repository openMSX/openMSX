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

bool Completer::equalHead(std::string_view s1, std::string_view s2, bool caseSensitive)
{
	if (s2.size() < s1.size()) return false;
	if (caseSensitive) {
		return std::ranges::equal(s1, subspan(s2, 0, s1.size()));
	} else {
		return strncasecmp(s1.data(), s2.data(), s1.size()) == 0;
	}
}

bool Completer::completeImpl(std::string& str, std::vector<std::string_view> matches,
                             bool caseSensitive)
{
	assert(std::ranges::all_of(matches, [&](auto& m) {
		return equalHead(str, m, caseSensitive);
	}));

	if (matches.empty()) {
		// no matching values
		return false;
	}
	if (matches.size() == 1) {
		// only one match
		str = matches.front();
		return true;
	}

	// Sort and remove duplicates.
	//  For efficiency it's best if the list doesn't contain duplicates to
	//  start with. Though sometimes this is hard to avoid. E.g. when doing
	//  filename completion + some extra allowed strings and one of these
	//  extra strings is the same as one of the filenames.
	std::ranges::sort(matches);
	auto u = std::ranges::unique(matches);
	matches.erase(u.begin(), u.end());

	auto minsize_of_matches = std::ranges::min(
		matches, {}, &std::string_view::size).size();

	bool expanded = false;
	while (str.size() < minsize_of_matches) {
		auto it = begin(matches);
		auto b = begin(*it);
		auto e = b + str.size();
		utf8::unchecked::next(e);
		std::string_view test_str(b, e);
		if (!std::ranges::all_of(matches,
			[&](auto& val) {
				return equalHead(test_str, val, caseSensitive);
			})) { break; }
		str = test_str;
		expanded = true;
	}
	if (!expanded && output) {
		// print all possibilities
		for (const auto& line : formatListInColumns(matches)) {
			output->output(line);
		}
	}
	return false;
}

void Completer::completeFileName(std::vector<std::string>& tokens,
                                 const FileContext& context)
{
	completeFileNameImpl(tokens, context, std::vector<std::string_view>());
}

void Completer::completeFileNameImpl(std::vector<std::string>& tokens,
                                     const FileContext& context,
                                     std::vector<std::string_view> matches)
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

	std::vector<std::string> filenames;
	for (const auto& p : paths) {
		auto pLen = p.size();
		if (!p.empty() && (p.back() != '/')) ++pLen;
		auto fileAction = [&](std::string_view path) {
			const auto& nm = FileOperations::getConventionalPath(
				std::string(path.substr(pLen)));
			if (equalHead(filename, nm, true)) {
				filenames.push_back(nm);
			}
		};
		auto dirAction = [&](std::string& path) {
			path += '/';
			fileAction(path);
			path.pop_back();
		};
		foreach_file_and_directory(
			FileOperations::join(p, dirname1),
			fileAction, dirAction);
	}
	append(matches, filenames);
	bool t = completeImpl(filename, matches, true);
	if (t && !filename.empty() && (filename.back() != '/')) {
		// completed filename, start new token
		tokens.emplace_back();
	}
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
