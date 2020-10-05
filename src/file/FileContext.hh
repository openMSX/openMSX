#ifndef FILECONTEXT_HH
#define FILECONTEXT_HH

#include <string_view>
#include <vector>

namespace openmsx {

class FileContext final
{
public:
	FileContext() = default;
	FileContext(std::vector<std::string>&& paths,
	            std::vector<std::string>&& savePaths);

	std::string resolve      (std::string_view filename) const;
	std::string resolveCreate(std::string_view filename) const;

	std::vector<std::string> getPaths() const;
	bool isUserContext() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::vector<std::string> paths;
	std::vector<std::string> savePaths;
};

FileContext configFileContext(std::string_view path, std::string_view hwDescr, std::string_view userName);
FileContext systemFileContext();
FileContext preferSystemFileContext();
FileContext userFileContext(std::string_view savePath = {});
FileContext userDataFileContext(std::string_view subdir);
FileContext currentDirFileContext();

} // namespace openmsx

#endif
