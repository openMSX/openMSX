// $Id$

#ifndef __FILECONTEXT_HH__
#define __FILECONTEXT_HH__

#include <string>
#include <vector>
#include <map>

using  std::string;
using  std::vector;
using  std::map;


namespace openmsx {

class FileContext
{
public:
	virtual ~FileContext();
	const string resolve(const string& filename);
	const string resolveCreate(const string& filename);
	const string resolveSave(const string& filename);
	
	virtual FileContext* clone() const = 0;
	
protected:
	FileContext();
	FileContext(const FileContext& rhs);
	virtual const vector<string>& getPaths() = 0;
	const string resolve(const vector<string>& pathList,
	                     const string& filename);

	vector<string> paths;
	string savePath;
};

class ConfigFileContext : public FileContext
{
public:
	ConfigFileContext(const string& path,
			  const string& hwDescr,
			  const string& userName);
	virtual ConfigFileContext* clone() const;

private:
	ConfigFileContext(const ConfigFileContext& rhs);
	virtual const vector<string>& getPaths();
	
	static map<string, int> nonames;
};

class SystemFileContext : public FileContext
{
public:
	SystemFileContext();
	virtual const vector<string>& getPaths();
	virtual SystemFileContext* clone() const;

private:
	SystemFileContext(const SystemFileContext& rhs);
};

class SettingFileContext : public FileContext
{
public:
	SettingFileContext(const string& url);
	virtual const vector<string>& getPaths();
	virtual SettingFileContext* clone() const;

private:
	SettingFileContext(const SettingFileContext& rhs);
};

class UserFileContext : public FileContext
{
public:
	UserFileContext();
	UserFileContext(const string& savePath);
	virtual const vector<string>& getPaths();
	virtual UserFileContext* clone() const;

private:
	UserFileContext(const UserFileContext& rhs);
	
	bool alreadyInit;
};

} // namespace openmsx

#endif
