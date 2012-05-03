#ifndef FILECONTEXT_HH
#define FILECONTEXT_HH

#include "string_ref.hh"
#include <vector>

namespace openmsx {

class FileContext final
{
public:
	FileContext() {}
	FileContext(std::vector<std::string>&& paths,
	            std::vector<std::string>&& savePaths);

	const std::string resolve      (string_ref filename) const;
	const std::string resolveCreate(string_ref filename) const;

	std::vector<std::string> getPaths() const;
	bool isUserContext() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::vector<std::string> paths;
	std::vector<std::string> savePaths;
};

FileContext configFileContext(string_ref path, string_ref hwDescr, string_ref userName);
FileContext systemFileContext();
FileContext preferSystemFileContext();
FileContext userFileContext(string_ref savePath = {});
FileContext userDataFileContext(string_ref subdir);
FileContext currentDirFileContext();

} // namespace openmsx

#endif
