// $Id$

#ifndef __FILECONTEXT_HH__
#define __FILECONTEXT_HH__

#include <string>
#include <list>


class FileContext
{
	public:
		virtual ~FileContext();
		virtual const std::list<std::string> &getPathList() const;
		
	protected:
		mutable std::list<std::string> paths;
};

class ConfigFileContext : public FileContext
{
	public:
		ConfigFileContext(const std::string &path);
};

class SystemFileContext : public FileContext
{
	public:
		SystemFileContext();
};

class UserFileContext : public FileContext
{
	public:
		UserFileContext();
		virtual const std::list<std::string> &getPathList() const;
		
};

#endif
