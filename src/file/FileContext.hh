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

	[[nodiscard]] std::string resolve      (std::string_view filename) const;
	[[nodiscard]] std::string resolveCreate(std::string_view filename) const;

	[[nodiscard]] std::vector<std::string> getPaths() const;
	[[nodiscard]] bool isUserContext() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::vector<std::string> paths;
	std::vector<std::string> savePaths;
};

[[nodiscard]] FileContext configFileContext(std::string_view path, std::string_view hwDescr, std::string_view userName);
[[nodiscard]] FileContext systemFileContext();
[[nodiscard]] FileContext preferSystemFileContext();
[[nodiscard]] FileContext userFileContext(std::string_view savePath = {});
[[nodiscard]] FileContext userDataFileContext(std::string_view subdir);
[[nodiscard]] FileContext currentDirFileContext();

} // namespace openmsx

#endif
