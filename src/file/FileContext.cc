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
	string result = resolveHelper(getPaths(), filename);
	assert(!FileOperations::needsTildeExpansion(result));
	return result;
}

string FileContext::resolveCreate(string_view filename) const
{
	if (savePaths2.empty()) {
		savePaths2 = getPathsHelper(savePaths);
	}

	string result;
	try {
		result = resolveHelper(savePaths2, filename);
	} catch (FileException&) {
		const string& path = savePaths2.front();
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

const vector<string>& FileContext::getPaths() const
{
	if (paths2.empty()) {
		paths2 = getPathsHelper(paths);
	}
	return paths2;
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

const FileContext& systemFileContext()
{
	static const FileContext result{
		{ USER_DATA, SYSTEM_DATA },
		{ USER_DATA } };
	return result;
}

const FileContext& preferSystemFileContext()
{
	static const FileContext result{
		{ SYSTEM_DATA, USER_DATA },  // first system dir
		{} };
	return result;
}

FileContext userFileContext(string_view savePath)
{
	return { { string{}, USER_DIRS },
	         { strCat(USER_OPENMSX, "/persistent/", savePath) } };
}

const FileContext& userFileContext()
{
	static const FileContext result{ { string{}, USER_DIRS }, {} };
	return result;
}

FileContext userDataFileContext(string_view subDir)
{
	return { { string{}, strCat(USER_OPENMSX, '/', subDir) },
	         {} };
}

const FileContext& currentDirFileContext()
{
	static const FileContext result{{string{}}, {string{}}};
	return result;
}

} // namespace openmsx
