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

const std::string &FileContext::getSavePath() const
{
	// we should only save from a system file context
	assert(false);
	static std::string dummy;
	return dummy;
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
	savePath = "~/.openMSX/persistent/" + hwDescr + '/' + userName + '/';
}

const std::string &ConfigFileContext::getSavePath() const
{
	return savePath;
}


// class SystemFileContext

SystemFileContext::SystemFileContext()
{
	paths.push_back("~/.openMSX/");		// user directory
	paths.push_back("/opt/openMSX/");	// system directory
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
