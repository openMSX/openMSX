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
		const std::string resolve(const std::string &filename);
		virtual const std::string resolveSave(
		                          const std::string &filename);
		
	protected:
		virtual const std::list<std::string> &getPaths() = 0;
		const std::string resolve(
			const std::list<std::string> &pathList,
        		const std::string &filename);
};

class ConfigFileContext : public FileContext
{
	public:
		ConfigFileContext(const std::string &path,
                                  const std::string &hwDescr,
                                  const std::string &userName);
		virtual const std::string resolveSave(
		                          const std::string &filename);
		
	protected:
		virtual const std::list<std::string> &getPaths();
	
	private:
		std::list<std::string> paths;
		std::string savePath;
		static std::map<std::string, int> nonames;
};

class SystemFileContext : public FileContext
{
	public:
		SystemFileContext();
		virtual const std::list<std::string> &getPaths();

	private:
		std::list<std::string> paths;
};

class UserFileContext : public FileContext
{
	public:
		UserFileContext();

	protected:
		virtual const std::list<std::string> &getPaths();
	
	private:
		std::list<std::string> paths;
};

#endif
