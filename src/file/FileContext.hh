// $Id$

#ifndef __FILECONTEXT_HH__
#define __FILECONTEXT_HH__

#include <string>
#include <vector>
#include <map>

using namespace std;


class FileContext
{
	public:
		virtual ~FileContext();
		const string resolve(const string &filename);
		const string resolveCreate(const string &filename);
		const string resolveSave(const string &filename);
		
	protected:
		virtual const vector<string> &getPaths() = 0;
		const string resolve(
			const vector<string> &pathList,
        		const string &filename);
		vector<string> paths;
		string savePath;
};

class ConfigFileContext : public FileContext
{
	public:
		ConfigFileContext(const string &path,
		                  const string &hwDescr,
		                  const string &userName);
	
	protected:
		virtual const vector<string> &getPaths();
	
	private:
		static map<string, int> nonames;
};

class SystemFileContext : public FileContext
{
	public:
		SystemFileContext();
		virtual const vector<string> &getPaths();
};

class SettingFileContext : public FileContext
{
	public:
		SettingFileContext(const string &url);
		virtual const vector<string> &getPaths();
};

class UserFileContext : public FileContext
{
	public:
		UserFileContext();
		UserFileContext(const string &savePath);
		virtual const vector<string> &getPaths();
	
	private:
		bool alreadyInit;
};

#endif
