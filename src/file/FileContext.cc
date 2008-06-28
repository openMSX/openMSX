// $Id$

#include "FileContext.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include "GlobalSettings.hh"
#include "CommandController.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include "StringSetting.hh"
#include "openmsx.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

FileContext::FileContext()
{
}

const string FileContext::resolve(const string& filename)
{
	string result = resolve(paths, filename);
	assert(FileOperations::expandTilde(result) == result);
	return result;
}

const string FileContext::resolveCreate(const string& filename)
{
	string result;
	try {
		result = resolve(savePaths, filename);
	} catch (FileException& e) {
		string path = savePaths.front();
		try {
			FileOperations::mkdirp(path);
		} catch (FileException& e) {
			PRT_DEBUG(e.getMessage());
		}
		result = FileOperations::join(path, filename);
	}
	assert(FileOperations::expandTilde(result) == result);
	return result;
}

string FileContext::resolve(const vector<string>& pathList,
                            const string& filename) const
{
	PRT_DEBUG("Context: " << filename);
	string filepath = FileOperations::expandCurrentDirFromDrive(filename);
	filepath = FileOperations::expandTilde(filepath);
	if (FileOperations::isAbsolutePath(filepath)) {
		// absolute path, don't resolve
		return filepath;
	}

	for (vector<string>::const_iterator it = pathList.begin();
	     it != pathList.end(); ++it) {
		string name = FileOperations::join(*it, filename);
		name = FileOperations::expandTilde(name);
		PRT_DEBUG("Context: try " << name);
		if (FileOperations::exists(name)) {
			return name;
		}
	}
	// not found in any path
	throw FileException(filename + " not found in this context");
}

const vector<string>& FileContext::getPaths() const
{
	return paths;
}

///

ConfigFileContext::ConfigFileContext(const string& path,
                                     const string& hwDescr,
                                     const string& userName)
{
	paths.push_back(path);
	savePaths.push_back(FileOperations::join(
		FileOperations::getUserOpenMSXDir(),
		"persistent", hwDescr, userName));
}

SystemFileContext::SystemFileContext()
{
	string userDir   = FileOperations::getUserDataDir();
	string systemDir = FileOperations::getSystemDataDir();
	paths.push_back(userDir);
	paths.push_back(systemDir);
	savePaths.push_back(userDir);
}

OnlySystemFileContext::OnlySystemFileContext()
{
	paths.push_back(FileOperations::getSystemDataDir());
}

UserFileContext::UserFileContext(CommandController& commandController,
                                 const string& savePath)
{
	paths.push_back("");
	try {
		const string& list = commandController.getGlobalSettings().
			getUserDirSetting().getValue();
		vector<string> dirs;
		commandController.splitList(list, dirs);
		paths.insert(paths.end(), dirs.begin(), dirs.end());
	} catch (CommandException& e) {
		commandController.getCliComm().printWarning(
			"user directories: " + e.getMessage());
	}

	if (!savePath.empty()) {
		savePaths.push_back(FileOperations::join(
			FileOperations::getUserOpenMSXDir(),
			"persistent", savePath));
	}
}

CurrentDirFileContext::CurrentDirFileContext()
{
	paths.push_back("");
}

} // namespace openmsx
