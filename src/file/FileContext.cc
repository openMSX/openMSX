#include "FileContext.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "stl.hh"
#include <cassert>
#include <utility>

using std::string;
using std::string_view;
using std::vector;

namespace openmsx {

const string USER_DIRS    = "{{USER_DIRS}}";
const string USER_OPENMSX = "{{USER_OPENMSX}}";
const string USER_DATA    = "{{USER_DATA}}";
const string SYSTEM_DATA  = "{{SYSTEM_DATA}}";


[[nodiscard]] static string subst(string_view path, string_view before, string_view after)
{
	assert(StringOp::startsWith(path, before));
	return strCat(after, path.substr(before.size()));
}

[[nodiscard]] static vector<string> getPathsHelper(const vector<string>& input)
{
	vector<string> result;
	for (const auto& s : input) {
		if (StringOp::startsWith(s, USER_OPENMSX)) {
			result.push_back(subst(s, USER_OPENMSX,
			                       FileOperations::getUserOpenMSXDir()));
		} else if (StringOp::startsWith(s, USER_DATA)) {
			result.push_back(subst(s, USER_DATA,
			                       FileOperations::getUserDataDir()));
		} else if (StringOp::startsWith(s, SYSTEM_DATA)) {
			result.push_back(subst(s, SYSTEM_DATA,
			                       FileOperations::getSystemDataDir()));
		} else if (s == USER_DIRS) {
			// Nothing. Keep USER_DIRS for isUserContext()
		} else {
			result.push_back(s);
		}
	}
	for (const auto& s : result) {
		assert(!FileOperations::needsTildeExpansion(s)); (void)s;
	}
	return result;
}

[[nodiscard]] static string resolveHelper(const vector<string>& pathList,
                            string_view filename)
{
	string filepath = FileOperations::expandTilde(
	                      FileOperations::expandCurrentDirFromDrive(string(filename)));
	if (FileOperations::isAbsolutePath(filepath)) {
		// absolute path, don't resolve
		return filepath;
	}

	for (const auto& p : pathList) {
		string name = FileOperations::join(p, filename);
		assert(!FileOperations::needsTildeExpansion(name));
		if (FileOperations::exists(name)) {
			return name;
		}
	}
	// not found in any path
	throw FileException(filename, " not found in this context");
}

FileContext::FileContext(vector<string>&& paths_, vector<string>&& savePaths_)
	: paths(std::move(paths_)), savePaths(std::move(savePaths_))
{
}

string FileContext::resolve(string_view filename) const
{
	vector<string> pathList = getPathsHelper(paths);
	string result = resolveHelper(pathList, filename);
	assert(!FileOperations::needsTildeExpansion(result));
	return result;
}

string FileContext::resolveCreate(string_view filename) const
{
	string result;
	vector<string> pathList = getPathsHelper(savePaths);
	try {
		result = resolveHelper(pathList, filename);
	} catch (FileException&) {
		string path = pathList.front();
		try {
			FileOperations::mkdirp(path);
		} catch (FileException&) {
			// ignore
		}
		result = FileOperations::join(path, filename);
	}
	assert(!FileOperations::needsTildeExpansion(result));
	return result;
}

vector<string> FileContext::getPaths() const
{
	return getPathsHelper(paths);
}

bool FileContext::isUserContext() const
{
	return contains(paths, USER_DIRS);
}

template<typename Archive>
void FileContext::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("paths",     paths,
	             "savePaths", savePaths);
}
INSTANTIATE_SERIALIZE_METHODS(FileContext);

///

static string backSubstSymbols(string_view path)
{
	const string& systemData = FileOperations::getSystemDataDir();
	if (StringOp::startsWith(path, systemData)) {
		return subst(path, systemData, SYSTEM_DATA);
	}
	const string& userData = FileOperations::getUserDataDir();
	if (StringOp::startsWith(path, userData)) {
		return subst(path, userData, USER_DATA);
	}
	const string& userDir = FileOperations::getUserOpenMSXDir();
	if (StringOp::startsWith(path, userDir)) {
		return subst(path, userDir, USER_OPENMSX);
	}
	// TODO USER_DIRS (not needed ATM)
	return string(path);
}

FileContext configFileContext(string_view path, string_view hwDescr, string_view userName)
{
	return { { backSubstSymbols(path) },
	         { strCat(USER_OPENMSX, "/persistent/", hwDescr, '/', userName) } };
}

FileContext systemFileContext()
{
	return { { USER_DATA, SYSTEM_DATA },
	         { USER_DATA } };
}

FileContext preferSystemFileContext()
{
	return { { SYSTEM_DATA, USER_DATA },  // first system dir
	         {} };
}

FileContext userFileContext(string_view savePath)
{
	vector<string> savePaths;
	if (!savePath.empty()) {
		savePaths = { strCat(USER_OPENMSX, "/persistent/", savePath) };
	}
	return { { string{}, USER_DIRS }, std::move(savePaths) };
}

FileContext userDataFileContext(string_view subDir)
{
	return { { string{}, strCat(USER_OPENMSX, '/', subDir) },
	         {} };
}

FileContext currentDirFileContext()
{
	return {{string{}}, {string{}}};
}

} // namespace openmsx
