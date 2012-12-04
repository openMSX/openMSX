// $Id$

#include "FileContext.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "openmsx.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

const string USER_DIRS    = "{{USER_DIRS}}";
const string USER_OPENMSX = "{{USER_OPENMSX}}";
const string USER_DATA    = "{{USER_DATA}}";
const string SYSTEM_DATA  = "{{SYSTEM_DATA}}";


static string subst(string_ref path, string_ref before, string_ref after)
{
	assert(path.starts_with(before));
	return after + path.substr(before.size());
}

static vector<string> getPathsHelper(const vector<string>& input)
{
	vector<string> result;
	for (auto& s : input) {
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
	return result;
}

static string resolveHelper(const vector<string>& pathList,
                            string_ref filename)
{
	PRT_DEBUG("Context: " << filename);
	string filepath = FileOperations::expandCurrentDirFromDrive(filename);
	filepath = FileOperations::expandTilde(filepath);
	if (FileOperations::isAbsolutePath(filepath)) {
		// absolute path, don't resolve
		return filepath;
	}

	for (auto& p : pathList) {
		string name = FileOperations::join(p, filename);
		name = FileOperations::expandTilde(name);
		PRT_DEBUG("Context: try " << name);
		if (FileOperations::exists(name)) {
			return name;
		}
	}
	// not found in any path
	throw FileException(filename + " not found in this context");
}

const string FileContext::resolve(string_ref filename) const
{
	vector<string> pathList = getPathsHelper(paths);
	string result = resolveHelper(pathList, filename);
	assert(FileOperations::expandTilde(result) == result);
	return result;
}

const string FileContext::resolveCreate(string_ref filename) const
{
	string result;
	vector<string> pathList = getPathsHelper(savePaths);
	try {
		result = resolveHelper(pathList, filename);
	} catch (FileException&) {
		string path = pathList.front();
		try {
			FileOperations::mkdirp(path);
		} catch (FileException& e) {
			PRT_DEBUG(e.getMessage());
			(void)&e; // Prevent warning
		}
		result = FileOperations::join(path, filename);
	}
	assert(FileOperations::expandTilde(result) == result);
	return result;
}

vector<string> FileContext::getPaths() const
{
	return getPathsHelper(paths);
}

bool FileContext::isUserContext() const
{
	for (auto& p : paths) {
		if (p == USER_DIRS) {
			return true;
		}
	}
	return false;
}

///

static string backSubstSymbols(const string& path)
{
	string systemData = FileOperations::getSystemDataDir();
	if (StringOp::startsWith(path, systemData)) {
		return subst(path, systemData, SYSTEM_DATA);
	}
	string userData = FileOperations::getSystemDataDir();
	if (StringOp::startsWith(path, userData)) {
		return subst(path, userData, SYSTEM_DATA);
	}
	string userDir = FileOperations::getUserOpenMSXDir();
	if (StringOp::startsWith(path, userDir)) {
		return subst(path, userDir, USER_OPENMSX);
	}
	// TODO USER_DIRS (not needed ATM)
	return path;
}

ConfigFileContext::ConfigFileContext(string_ref path,
                                     string_ref hwDescr,
                                     string_ref userName)
{
	paths.push_back(backSubstSymbols(FileOperations::expandTilde(path)));
	savePaths.push_back(FileOperations::join(
		USER_OPENMSX, "persistent", hwDescr, userName));
}

SystemFileContext::SystemFileContext()
{
	paths.push_back(USER_DATA);
	paths.push_back(SYSTEM_DATA);
	savePaths.push_back(USER_DATA);
}

PreferSystemFileContext::PreferSystemFileContext()
{
	paths.push_back(SYSTEM_DATA); // first system dir
	paths.push_back(USER_DATA);
}

UserFileContext::UserFileContext(string_ref savePath)
{
	paths.push_back("");
	paths.push_back(USER_DIRS);

	if (!savePath.empty()) {
		savePaths.push_back(FileOperations::join(
			USER_OPENMSX, "persistent", savePath));
	}
}

UserDataFileContext::UserDataFileContext(string_ref subDir)
{
	paths.push_back("");
	paths.push_back(USER_OPENMSX + '/' + subDir);
}

CurrentDirFileContext::CurrentDirFileContext()
{
	paths.push_back("");
}


template<typename Archive>
void FileContext::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("paths", paths);
	ar.serialize("savePaths", savePaths);
}
INSTANTIATE_SERIALIZE_METHODS(FileContext);

} // namespace openmsx
