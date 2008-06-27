// $Id$

#ifndef FILECONTEXT_HH
#define FILECONTEXT_HH

#include <string>
#include <vector>

namespace openmsx {

class CommandController;

class FileContext
{
public:
	virtual ~FileContext();
	const std::string resolve(const std::string& filename);
	const std::string resolveCreate(const std::string& filename);
	const std::vector<std::string>& getPaths() const;

protected:
	FileContext();
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
};

class SystemFileContext : public FileContext
{
public:
	explicit SystemFileContext(bool preferSystemDir = false);
};

class SettingFileContext : public FileContext
{
public:
	explicit SettingFileContext(const std::string& url);
};

class UserFileContext : public FileContext
{
public:
	explicit UserFileContext(CommandController& commandController,
	          const std::string& savePath = "", bool skipUserDirs = false);
};

} // namespace openmsx

#endif
