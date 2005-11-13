// $Id$

#ifndef FILECONTEXT_HH
#define FILECONTEXT_HH

#include <string>
#include <vector>
#include <map>

namespace openmsx {

class CommandController;

class FileContext
{
public:
	virtual ~FileContext();
	const std::string resolve(const std::string& filename);
	const std::string resolveCreate(const std::string& filename);
	const std::vector<std::string>& getPaths() const;

	virtual FileContext* clone() const = 0;

protected:
	FileContext();
	explicit FileContext(const FileContext& rhs);
	std::string resolve(const std::vector<std::string>& pathList,
	               const std::string& filename) const;

	std::vector<std::string> paths;
	std::vector<std::string> savePaths;
};

class ConfigFileContext : public FileContext
{
public:
	ConfigFileContext(const std::string& path,
			  const std::string& hwDescr,
			  const std::string& userName);
	virtual ConfigFileContext* clone() const;

private:
	explicit ConfigFileContext(const ConfigFileContext& rhs);

	static std::map<std::string, int> nonames;
};

class SystemFileContext : public FileContext
{
public:
	explicit SystemFileContext(bool preferSystemDir = false);
	virtual SystemFileContext* clone() const;

private:
	explicit SystemFileContext(const SystemFileContext& rhs);
};

class SettingFileContext : public FileContext
{
public:
	explicit SettingFileContext(const std::string& url);
	virtual SettingFileContext* clone() const;

private:
	explicit SettingFileContext(const SettingFileContext& rhs);
};

class UserFileContext : public FileContext
{
public:
	UserFileContext(CommandController& commandController,
	          const std::string& savePath = "", bool skipUserDirs = false);
	virtual UserFileContext* clone() const;

private:
	explicit UserFileContext(const UserFileContext& rhs);
};

} // namespace openmsx

#endif
