// $Id$

#ifndef __FILECONTEXT_HH__
#define __FILECONTEXT_HH__

#include <string>
#include <list>
#include <map>


class FileContext
{
	public:
		virtual ~FileContext();
		virtual const std::list<std::string> &getPathList() const;
		virtual const std::string &getSavePath() const;
		
	protected:
		mutable std::list<std::string> paths;
};

class ConfigFileContext : public FileContext
{
	public:
		ConfigFileContext(const std::string &path,
                                  const std::string &hwDescr,
                                  const std::string &userName);
		virtual const std::string &getSavePath() const;
	
	private:
		std::string savePath;
		static std::map<std::string, int> nonames;
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
