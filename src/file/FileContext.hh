#ifndef FILECONTEXT_HH
#define FILECONTEXT_HH

#include "span.hh"
#include <string>
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

	[[nodiscard]] span<const std::string> getPaths() const;
	[[nodiscard]] bool isUserContext() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::vector<std::string> paths;
	mutable std::vector<std::string> paths2; // calculated from paths
	std::vector<std::string> savePaths;
	mutable std::vector<std::string> savePaths2; // calc from savePaths
};

[[nodiscard]] FileContext configFileContext(std::string_view path, std::string_view hwDescr, std::string_view userName);
[[nodiscard]] FileContext userDataFileContext(std::string_view subdir);
[[nodiscard]] FileContext userFileContext(std::string_view savePath);
[[nodiscard]] const FileContext& userFileContext();
[[nodiscard]] const FileContext& systemFileContext();
[[nodiscard]] const FileContext& preferSystemFileContext();
[[nodiscard]] const FileContext& currentDirFileContext();

} // namespace openmsx

#endif
