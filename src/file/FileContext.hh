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
	const vector<string>& getPaths() const;
	
	virtual FileContext* clone() const = 0;

protected:
	FileContext();
	FileContext(const FileContext& rhs);
	string resolve(const vector<string>& pathList,
	               const string& filename) const;

	vector<string> paths;
	vector<string> savePaths;
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
	
	static map<string, int> nonames;
};

class SystemFileContext : public FileContext
{
public:
	SystemFileContext(bool preferSystemDir = false);
	virtual SystemFileContext* clone() const;

private:
	SystemFileContext(const SystemFileContext& rhs);
};

class SettingFileContext : public FileContext
{
public:
	SettingFileContext(const string& url);
	virtual SettingFileContext* clone() const;

private:
	SettingFileContext(const SettingFileContext& rhs);
};

class UserFileContext : public FileContext
{
public:
	UserFileContext(const string& savePath = "", bool skipUserDirs = false);
	virtual UserFileContext* clone() const;

private:
	UserFileContext(const UserFileContext& rhs);
};

} // namespace openmsx

#endif
