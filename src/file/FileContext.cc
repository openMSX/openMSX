// $Id$

#include <cassert>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "FileContext.hh"
#include "FileOperations.hh"
#include "MSXConfig.hh"
#include "File.hh"


const string homedir("~/.openMSX/");
const string systemdir("/opt/openMSX/");



// class FileContext

FileContext::FileContext()
{
}

FileContext::~FileContext()
{
}

const string FileContext::resolve(const string &filename)
{
	return resolve(getPaths(), filename);
}

const string FileContext::resolveCreate(const string &filename)
{
	try {
		return resolve(getPaths(), filename);
	} catch (FileException &e) {
		string path = getPaths().front();
		FileOperations::mkdirp(path);
		return path + filename;
	}
}

const string FileContext::resolve(const vector<string> &pathList,
                                       const string &filename)
{
	// TODO handle url-protocols better
	
	PRT_DEBUG("Context: "<<filename);
	if ((filename.find("://") != string::npos) ||
	    (filename[0] == '/')) {
		// protocol specified or absolute path, don't resolve
		return filename;
	}
	
	for (vector<string>::const_iterator it = pathList.begin();
	     it != pathList.end();
	     ++it) {
		string name = FileOperations::expandTilde(*it + filename);
		unsigned pos = name.find("://");
		if (pos != string::npos) {
			name = name.substr(pos + 3);
		}
		struct stat buf;
		PRT_DEBUG("Context: try "<<name);
		if (!stat(name.c_str(), &buf)) {
			// found
			return name;
		}
	}
	// not found in any path
	throw FileException(filename + " not found in this context");
}

const string FileContext::resolveSave(const string &filename)
{
	assert(!savePath.empty());
	FileOperations::mkdirp(savePath);
	return savePath + filename;
}

FileContext::FileContext(const FileContext& rhs)
	: paths(rhs.paths), savePath(rhs.savePath)
{
}


// class ConfigFileContext

map<string, int> ConfigFileContext::nonames;

ConfigFileContext::ConfigFileContext(const string &path,
                                     const string &hwDescr,
                                     const string &userName_)
{
	paths.push_back(path);

	string userName(userName_);
	if (userName == "") {
		int num = ++nonames[hwDescr];
		char buf[20];
		snprintf(buf, 20, "untitled%d", num);
		userName = buf;
	}
	savePath = homedir + "persistent/" + hwDescr +
	                       '/' + userName + '/';
}

const vector<string> &ConfigFileContext::getPaths()
{
	return paths;
}

ConfigFileContext* ConfigFileContext::clone() const
{
	return new ConfigFileContext(*this);
}

ConfigFileContext::ConfigFileContext(const ConfigFileContext& rhs)
	: FileContext(rhs)
{
}


// class SystemFileContext

SystemFileContext::SystemFileContext()
{
	paths.push_back(homedir);		// user directory
	paths.push_back(systemdir);	// system directory
}

const vector<string> &SystemFileContext::getPaths()
{
	return paths;
}

SystemFileContext* SystemFileContext::clone() const
{
	return new SystemFileContext(*this);
}

SystemFileContext::SystemFileContext(const SystemFileContext& rhs)
	: FileContext(rhs)
{
}


// class SettingFileContext

SettingFileContext::SettingFileContext(const string &url)
{
	string path = FileOperations::getBaseName(url);
	paths.push_back(path);
	PRT_DEBUG("SettingFileContext: "<<path);

	string home(FileOperations::expandTilde(homedir));
	
	unsigned pos1 = path.find(home);
	if (pos1 != string::npos) {
		unsigned len1 = home.length();
		string path1 = path.replace(pos1, len1, systemdir);
		paths.push_back(path1);
		PRT_DEBUG("SettingFileContext: "<<path1);
	}
}

const vector<string> &SettingFileContext::getPaths()
{
	return paths;
}

SettingFileContext* SettingFileContext::clone() const
{
	return new SettingFileContext(*this);
}

SettingFileContext::SettingFileContext(const SettingFileContext& rhs)
	: FileContext(rhs)
{
}


// class UserFileContext

UserFileContext::UserFileContext()
	: alreadyInit(false)
{
}

UserFileContext::UserFileContext(const string &savePath_)
	: alreadyInit(false)
{
	savePath = string(homedir + "persistent/") + savePath_ + '/';
}

const vector<string> &UserFileContext::getPaths()
{
	if (!alreadyInit) {
		alreadyInit = true;
		try {
			Config *config = MSXConfig::instance()->
				getConfigById("UserDirectories");
			list<Device::Parameter*>* pathList =
				config->getParametersWithClass("");
			for (list<Device::Parameter*>::const_iterator it =
			     pathList->begin();
			     it != pathList->end();
			     it++) {
				string path = (*it)->value;
				if (path[path.length() - 1] != '/') {
					path += '/';
				}
				path = FileOperations::expandTilde(path);
				paths.push_back(path);
			}
			config->getParametersWithClassClean(pathList);
		} catch (ConfigException &e) {
			// no UserDirectories specified
		}
		paths.push_back("./");
	}
	return paths;
}

UserFileContext* UserFileContext::clone() const
{
	return new UserFileContext(*this);
}

UserFileContext::UserFileContext(const UserFileContext& rhs)
	: FileContext(rhs), alreadyInit(rhs.alreadyInit)
{
}
