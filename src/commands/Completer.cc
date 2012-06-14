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
using std::set;

namespace openmsx {

InterpreterOutput* Completer::output = 0;


Completer::Completer(string_ref name_)
	: name(name_.data(), name_.size())
{
}

Completer::~Completer()
{
}

const string& Completer::getName() const
{
	return name;
}

static bool isEqual(string_ref s1, string_ref s2, bool caseSensitive)
{
	if (caseSensitive) {
		return s1 == s2;
	} else {
		if (s1.size() != s2.size()) return false;
		return strncasecmp(s1.data(), s2.data(), s1.size()) == 0;
	}
}

static bool formatHelper(const set<string>& input, unsigned columnLimit,
                         vector<string>& result)
{
	unsigned column = 0;
	set<string>::const_iterator it = input.begin();
	do {
		unsigned maxcolumn = column;
		for (unsigned i = 0; (i < result.size()) && (it != input.end());
		     ++i, ++it) {
			unsigned curSize = utf8::unchecked::size(result[i]);
			result[i] += string(column - curSize, ' ');
			result[i] += *it;
			maxcolumn = std::max<unsigned>(maxcolumn,
			                               utf8::unchecked::size(result[i]));
			if (maxcolumn > columnLimit) return false;
		}
		column = maxcolumn + 2;
	} while (it != input.end());
	return true;
}

static vector<string> format(const set<string>& input, unsigned columnLimit)
{
	vector<string> result;
	for (unsigned lines = 1; lines < input.size(); ++lines) {
		result.assign(lines, string());
		if (formatHelper(input, columnLimit, result)) {
			return result;
		}
	}
	result.assign(input.begin(), input.end());
	return result;
}

bool Completer::completeString2(string& str, set<string>& st,
                                bool caseSensitive)
{
	set<string>::iterator it = st.begin();
	while (it != st.end()) {
		if (isEqual(str, string_ref(*it).substr(0, str.size()), caseSensitive)) {
			++it;
		} else {
			set<string>::iterator it2 = it;
			++it;
			st.erase(it2);
		}
	}
	if (st.empty()) {
		// no matching commands
		return false;
	}
	if (st.size() == 1) {
		// only one match
		str = *(st.begin());
		return true;
	}
	bool expanded = false;
	while (true) {
		it = st.begin();
		if (isEqual(str, *it, caseSensitive)) {
			// match is as long as first word
			goto out; // TODO rewrite this
		}
		// expand with one char and check all strings
		string_ref string2 = string_ref(*it).substr(0, str.size() + 1);
		for (/**/; it != st.end(); ++it) {
			if (!isEqual(string2, string_ref(*it).substr(0, string2.size()),
				   caseSensitive)) {
				goto out; // TODO rewrite this
			}
		}
		// no conflict found
		str.assign(string2.data(), string2.size());
		expanded = true;
	}
	out:
	if (!expanded && output) {
		// print all possibilities
		vector<string> lines = format(st, output->getOutputColumns() - 1);
		for (vector<string>::const_iterator it = lines.begin();
		     it != lines.end(); ++it) {
			output->output(*it);
		}
	}
	return false;
}

void Completer::completeString(vector<string>& tokens, set<string>& st,
                               bool caseSensitive)
{
	if (completeString2(tokens.back(), st, caseSensitive)) {
		tokens.push_back("");
	}
}

void Completer::completeFileName(vector<string>& tokens,
                                 const FileContext& context)
{
	set<string> empty;
	completeFileName(tokens, context, empty);
}

void Completer::completeFileName(vector<string>& tokens,
                                 const FileContext& context,
                                 const set<string>& extra)
{
	vector<string> paths(context.getPaths());

	string& filename = tokens.back();
	filename = FileOperations::expandTilde(filename);
	filename = FileOperations::expandCurrentDirFromDrive(filename);
	string_ref basename = FileOperations::getBaseName(filename);
	if (FileOperations::isAbsolutePath(filename)) {
		paths.push_back("");
	}

	set<string> filenames(extra);
	for (vector<string>::const_iterator it = paths.begin();
	     it != paths.end();
	     ++it) {
		string dirname = FileOperations::join(*it, basename);
		ReadDir dir(FileOperations::getNativePath(dirname));
		while (dirent* de = dir.getEntry()) {
			string name = FileOperations::join(dirname, de->d_name);
			if (FileOperations::exists(name)) {
				string nm = FileOperations::join(basename, de->d_name);
				if (FileOperations::isDirectory(name)) {
					nm += '/';
				}
				filenames.insert(FileOperations::getConventionalPath(nm));
			}
		}
	}
	bool t = completeString2(filename, filenames, true);
	if (t && !filename.empty() && (*filename.rbegin() != '/')) {
		// completed filename, start new token
		tokens.push_back("");
	}
}

void Completer::setOutput(InterpreterOutput* output_)
{
	output = output_;
}

} // namespace openmsx
