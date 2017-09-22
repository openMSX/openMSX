#include "Completer.hh"
#include "InterpreterOutput.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "ReadDir.hh"
#include "utf8_unchecked.hh"
#include "stringsp.hh"
#include <algorithm>

using std::vector;
using std::string;

namespace openmsx {

InterpreterOutput* Completer::output = nullptr;


Completer::Completer(string_ref name_)
	: name(name_.str())
{
}

static bool formatHelper(const vector<string_ref>& input, size_t columnLimit,
                         vector<string>& result)
{
	size_t column = 0;
	auto it = begin(input);
	do {
		size_t maxcolumn = column;
		for (size_t i = 0; (i < result.size()) && (it != end(input));
		     ++i, ++it) {
			auto curSize = utf8::unchecked::size(result[i]);
			result[i] += string(column - curSize, ' ');
			result[i] += it->str();
			maxcolumn = std::max(maxcolumn,
			                     utf8::unchecked::size(result[i]));
			if (maxcolumn > columnLimit) return false;
		}
		column = maxcolumn + 2;
	} while (it != end(input));
	return true;
}

static vector<string> format(const vector<string_ref>& input, size_t columnLimit)
{
	vector<string> result;
	for (size_t lines = 1; lines < input.size(); ++lines) {
		result.assign(lines, string());
		if (formatHelper(input, columnLimit, result)) {
			return result;
		}
	}
	for (auto& s : input) {
		result.push_back(s.str());
	}
	return result;
}

vector<string> Completer::formatListInColumns(const vector<string_ref>& input)
{
	return format(input, output->getOutputColumns() - 1);
}

bool Completer::equalHead(string_ref s1, string_ref s2, bool caseSensitive)
{
	if (s2.size() < s1.size()) return false;
	if (caseSensitive) {
		return memcmp(s1.data(), s2.data(), s1.size()) == 0;
	} else {
		return strncasecmp(s1.data(), s2.data(), s1.size()) == 0;
	}
}

bool Completer::completeImpl(string& str, vector<string_ref> matches,
                             bool caseSensitive)
{
	for (auto& m : matches) {
		assert(equalHead(str, m, caseSensitive)); (void)m;
	}

	if (matches.empty()) {
		// no matching values
		return false;
	}
	if (matches.size() == 1) {
		// only one match
		str = matches.front().str();
		return true;
	}

	// Sort and remove duplicates.
	//  For efficiency it's best if the list doesn't contain duplicates to
	//  start with. Though sometimes this is hard to avoid. E.g. when doing
	//  filename completion + some extra allowed strings and one of these
	//  extra strings is the same as one of the filenames.
	sort(begin(matches), end(matches));
	matches.erase(unique(begin(matches), end(matches)), end(matches));

	bool expanded = false;
	while (true) {
		auto it = begin(matches);
		if (str.size() == it->size()) {
			// match is as long as first word
			goto out; // TODO rewrite this
		}
		// expand with one char and check all strings
		auto b = begin(*it);
		auto e = b + str.size();
		utf8::unchecked::next(e); // one more utf8 char
		string_ref string2(b, e);
		for (/**/; it != end(matches); ++it) {
			if (!equalHead(string2, *it, caseSensitive)) {
				goto out; // TODO rewrite this
			}
		}
		// no conflict found
		str = string2.str();
		expanded = true;
	}
	out:
	if (!expanded && output) {
		// print all possibilities
		for (auto& line : formatListInColumns(matches)) {
			output->output(line);
		}
	}
	return false;
}

void Completer::completeFileName(vector<string>& tokens,
                                 const FileContext& context)
{
	completeFileNameImpl(tokens, context, vector<string_ref>());
}

void Completer::completeFileNameImpl(vector<string>& tokens,
                                     const FileContext& context,
                                     vector<string_ref> matches)
{
	string& filename = tokens.back();
	filename = FileOperations::expandTilde(filename);
	filename = FileOperations::expandCurrentDirFromDrive(filename);
	string_ref dirname1 = FileOperations::getDirName(filename);

	vector<string> paths;
	if (FileOperations::isAbsolutePath(filename)) {
		paths.emplace_back();
	} else {
		paths = context.getPaths();
	}

	vector<string> filenames;
	for (auto& p : paths) {
		string dirname = FileOperations::join(p, dirname1);
		ReadDir dir(FileOperations::getNativePath(dirname));
		while (dirent* de = dir.getEntry()) {
			string name = FileOperations::join(dirname, de->d_name);
			if (FileOperations::exists(name)) {
				string nm = FileOperations::join(dirname1, de->d_name);
				if (FileOperations::isDirectory(name)) {
					nm += '/';
				}
				nm = FileOperations::getConventionalPath(nm);
				if (equalHead(filename, nm, true)) {
					filenames.push_back(nm);
				}
			}
		}
	}
	for (auto& f : filenames) {
		matches.emplace_back(f);
	}
	bool t = completeImpl(filename, matches, true);
	if (t && !filename.empty() && (filename.back() != '/')) {
		// completed filename, start new token
		tokens.emplace_back();
	}
}

} // namespace openmsx
