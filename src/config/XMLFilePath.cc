// $Id$
//

#include <cstdlib>

#include "XMLFilePath.hh"

namespace XMLConfig
{

FilePath::FilePath()
:MSXConfig::FilePath(), XMLConfig::CustomConfig(),
 internet_(false), caching_(false), cacheexpire_(false), cachedays_(1),
 cachedir_("~/.openmsx/cache"),
 persistantdir_("~/.openmsx/persistant")
{
	PRT_DEBUG("XMLConfig::FilePath::FilePath()");
}

FilePath::~FilePath()
{
}

void FilePath::backendInit(XML::Element *e)
{
	std::list<XML::Element*>::const_iterator i = e->children.begin();
	while (i != e->children.end())
	{
		if ((*i)->name=="internet")
		{
			handleInternet((*i));
		}
		else if ((*i)->name=="datafiles")
		{
			handleDatafiles((*i));
		}
		else if ((*i)->name=="cache")
		{
			handleCache((*i));
		}
		else if ((*i)->name=="persistantdata")
		{
			handlePersistantdata((*i));
		}
		else
		{
			std::ostringstream s;
			s << "<filepath> contains unknown element <" << (*i)->name << ">";
			throw MSXConfig::Exception(s);
		}
	}
}

void FilePath::handleInternet(XML::Element *e)
{
	std::string enabled(e->getAttribute("enabled"));
	if (enabled=="true" || enabled=="TRUE")
	{
		internet_ = true;
	}
}

void FilePath::handleDatafiles(XML::Element *e)
{
	std::list<XML::Element*>::const_iterator i=e->children.begin();
	while (i != e->children.end())
	{
		if ((*i)->name=="path")
		{
			std::string* t_str = new std::string((*i)->pcdata);
			FilePath::expandPath(*t_str);
			paths.push_back(t_str);
		}
	}
}

void FilePath::handleCache(XML::Element *e)
{
	std::string enabled(e->getAttribute("enabled"));
	if (enabled=="true" || enabled=="TRUE")
	{
		caching_ = true;
	}
	std::string t_cachedir = e->getElementPcdata("path");
	if (t_cachedir != "")
	{
		cachedir_ = t_cachedir;
	}
	FilePath::expandPath(cachedir_);

	std::string t_enabled = e->getElementAttribute("expire", "enabled");
	if  (t_enabled=="true" || t_enabled=="TRUE")
	{
		cacheexpire_ = true;
	}
	std::string t_days = e->getElementAttribute("expire", "days");
	if (t_days != "")
	{
		cachedays_ = strtol(t_days.c_str(), 0, 0);
		if (cachedays_ == 0) cachedays_++;
	}
}

void FilePath::handlePersistantdata(XML::Element *e)
{
	std::string t_persistant = e->getElementPcdata("path");
	if (t_persistant != "")
	{
		persistantdir_ = t_persistant;
	}
	expandPath(persistantdir_);
}

bool FilePath::internet()
{
	return internet_;
}

bool FilePath::caching()
{
	return caching_;
}

const std::string &FilePath::cachedir()
{
	return cachedir_;
}

bool FilePath::cacheExpire()
{
	return cacheexpire_;
}

int FilePath::cacheDays()
{
	return cachedays_;
}

const std::string &FilePath::persistantdir()
{
	return persistantdir_;
}

const std::string &FilePath::expandPath(std::string &path)
{
	if (path.length() > 0)
	{
		if (path[0]=='~')
		{
			std::string env(getenv("HOME"));
			path = env + path;
		}
	}
	return path;
}

}; // end namespace XMLConfig
