#ifndef FILECONTEXT_HH
#define FILECONTEXT_HH

#include "string_view.hh"
#include <vector>

namespace openmsx {

class FileContext final
{
public:
	FileContext() {}
	FileContext(std::vector<std::string>&& paths,
	            std::vector<std::string>&& savePaths);

	const std::string resolve      (string_view filename) const;
	const std::string resolveCreate(string_view filename) const;

	std::vector<std::string> getPaths() const;
	bool isUserContext() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::vector<std::string> paths;
	std::vector<std::string> savePaths;
};

FileContext configFileContext(string_view path, string_view hwDescr, string_view userName);
FileContext systemFileContext();
FileContext preferSystemFileContext();
FileContext userFileContext(string_view savePath = {});
FileContext userDataFileContext(string_view subdir);
FileContext currentDirFileContext();

} // namespace openmsx

#endif
