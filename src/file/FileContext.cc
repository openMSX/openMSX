// $Id$

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "FileContext.hh"
#include "FileOperations.hh"
#include "MSXConfig.hh"


// class FileContext

FileContext::~FileContext()
{
}

const std::string FileContext::resolve(const std::string &filename)
{
	return resolve(getPaths(), filename);
}

const std::string FileContext::resolve(const std::list<std::string> &pathList,
                                       const std::string &filename)
{
	// TODO handle url-protocols better
	
	PRT_DEBUG("Context: "<<filename);
	if ((filename.find("://") != std::string::npos) ||
	    (filename[0] == '/')) {
		// protocol specified or absolute path, don't resolve
		return filename;
	}
	
	std::list<std::string>::const_iterator it;
	for (it = pathList.begin(); it != pathList.end(); it++) {
		std::string name = FileOperations::expandTilde(*it + filename);
		unsigned pos = name.find("://");
		if (pos != std::string::npos) {
			name = name.substr(pos + 3);
		}
		struct stat buf;
		PRT_DEBUG("Context: try "<<name);
		if (!stat(name.c_str(), &buf)) {
			// no error
			PRT_DEBUG("Context: found "<<name);
			return name;
		}
	}
	// not found in any path
	return filename;
}


// class ConfigFileContext

std::map<std::string, int> ConfigFileContext::nonames;

ConfigFileContext::ConfigFileContext(const std::string &path,
                                     const std::string &hwDescr,
                                     const std::string &userName_)
{
	paths.push_back(path);

	std::string userName(userName_);
	if (userName == "") {
		int num = ++nonames[hwDescr];
		char buf[20];
		snprintf(buf, 20, "untitled%d", num);
		userName = buf;
	}
	std::string savePath = "~/.openMSX/persistent/" + hwDescr +
	                       '/' + userName + '/';
	savePaths.push_back(savePath);
}

const std::string ConfigFileContext::resolveSave(const std::string &filename)
{
	return resolve(savePaths, filename);
}

const std::list<std::string> &ConfigFileContext::getPaths()
{
	return paths;
}


// class SystemFileContext

SystemFileContext::SystemFileContext()
{
	paths.push_back("~/.openMSX/");		// user directory
	paths.push_back("/opt/openMSX/");	// system directory
}

const std::list<std::string> &SystemFileContext::getPaths()
{
	return paths;
}


// class UserFileContext

UserFileContext::UserFileContext()
{
}

const std::list<std::string> &UserFileContext::getPaths()
{
	static bool alreadyInit = false;
	static bool hasUserContext; // debug

	if (!alreadyInit) {
		try {
			Config *config = MSXConfig::instance()->
				getConfigById("UserDirectories");
			std::list<Device::Parameter*>* pathList;
			pathList = config->getParametersWithClass("");
			std::list<Device::Parameter*>::const_iterator it;
			for (it = pathList->begin(); it != pathList->end(); it++) {
				std::string path = (*it)->value;
				if (path[path.length() - 1] != '/') {
					path += '/';
				}
				paths.push_back(path);
			}
			hasUserContext = true;
		} catch (MSXException &e) {
			// no UserDirectories specified
			hasUserContext = false;
		}
		paths.push_back("");
	} else {
		try {
			MSXConfig::instance()->getConfigById("UserDirectories");
			assert(hasUserContext == true);
		} catch (MSXException &e) {
			assert(hasUserContext == false);
		}
	}
	return paths;
}
