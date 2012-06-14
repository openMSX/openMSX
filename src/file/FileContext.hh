// $Id$

#ifndef FILECONTEXT_HH
#define FILECONTEXT_HH

#include "string_ref.hh"
#include <vector>

namespace openmsx {

class FileContext
{
public:
	const std::string resolve(const std::string& filename) const;
	const std::string resolveCreate(const std::string& filename) const;

	std::vector<std::string> getPaths() const;
	bool isUserContext() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	std::vector<std::string> paths;
	std::vector<std::string> savePaths;
};


class ConfigFileContext : public FileContext
{
public:
	ConfigFileContext(string_ref path,
	                  string_ref hwDescr,
	                  string_ref userName);
};

class SystemFileContext : public FileContext
{
public:
	SystemFileContext();
};

class PreferSystemFileContext : public FileContext
{
public:
	PreferSystemFileContext();
};

class UserFileContext : public FileContext
{
public:
	explicit UserFileContext(const std::string& savePath = "");
};

class UserDataFileContext : public FileContext
{
public:
	explicit UserDataFileContext(const std::string& subdir);
};

class CurrentDirFileContext : public FileContext
{
public:
	CurrentDirFileContext();
};

} // namespace openmsx

#endif
