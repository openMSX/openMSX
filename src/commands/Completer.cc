// $Id$

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

Completer::~Completer()
{
}

const string& Completer::getName() const
{
	return name;
}

static bool formatHelper(const vector<string_ref>& input, size_t columnLimit,
                         vector<string>& result)
{
	size_t column = 0;
	auto it = input.begin();
	do {
		size_t maxcolumn = column;
		for (size_t i = 0; (i < result.size()) && (it != input.end());
		     ++i, ++it) {
			auto curSize = utf8::unchecked::size(result[i]);
			result[i] += string(column - curSize, ' ');
			result[i] += it->str();
			maxcolumn = std::max(maxcolumn,
			                     utf8::unchecked::size(result[i]));
			if (maxcolumn > columnLimit) return false;
		}
		column = maxcolumn + 2;
	} while (it != input.end());
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
	sort(matches.begin(), matches.end());
	bool expanded = false;
	while (true) {
		auto it = matches.begin();
		if (str.size() == it->size()) {
			// match is as long as first word
			goto out; // TODO rewrite this
		}
		// expand with one char and check all strings
		auto begin = it->begin();
		auto end = begin + str.size();
		utf8::unchecked::next(end); // one more utf8 char
		string_ref string2(begin, end);
		for (/**/; it != matches.end(); ++it) {
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
		for (auto& line : format(matches, output->getOutputColumns() - 1)) {
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
	auto paths = context.getPaths();

	string& filename = tokens.back();
	filename = FileOperations::expandTilde(filename);
	filename = FileOperations::expandCurrentDirFromDrive(filename);
	string_ref basename = FileOperations::getBaseName(filename);
	if (FileOperations::isAbsolutePath(filename)) {
		paths.push_back("");
	}

	vector<string> filenames;
	for (auto& p : paths) {
		string dirname = FileOperations::join(p, basename);
		ReadDir dir(FileOperations::getNativePath(dirname));
		while (dirent* de = dir.getEntry()) {
			string name = FileOperations::join(dirname, de->d_name);
			if (FileOperations::exists(name)) {
				string nm = FileOperations::join(basename, de->d_name);
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
		matches.push_back(f);
	}
	bool t = completeImpl(filename, matches, true);
	if (t && !filename.empty() && (filename.back() != '/')) {
		// completed filename, start new token
		tokens.push_back("");
	}
}

void Completer::setOutput(InterpreterOutput* output_)
{
	output = output_;
}

} // namespace openmsx
