// $Id$

#include "FileContext.hh"
#include "MSXConfig.hh"


// class FileContext

FileContext::~FileContext()
{
}

const std::list<std::string> &FileContext::getPathList() const
{
	return paths;
}


// class ConfigFileContext

ConfigFileContext::ConfigFileContext(const std::string &path)
{
	paths.push_back(path);
}


// class SystemFileContext

SystemFileContext::SystemFileContext()
{
	paths.push_back("~/.openMSX/");
	paths.push_back("/usr/local/etc/openMSX/");
	paths.push_back("/etc/openMSX/");
}


// class UserFileContext

UserFileContext::UserFileContext()
{
}

const std::list<std::string> &UserFileContext::getPathList() const
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
