#include "FileContext.hh"

#include "FileOperations.hh"
#include "FileException.hh"
#include "serialize.hh"
#include "serialize_stl.hh"

#include "join.hh"
#include "stl.hh"

#include <cassert>
#include <utility>

namespace openmsx {

using namespace std::literals;

const std::string USER_DIRS    = "{{USER_DIRS}}";
const std::string USER_OPENMSX = "{{USER_OPENMSX}}";
const std::string USER_DATA    = "{{USER_DATA}}";
const std::string SYSTEM_DATA  = "{{SYSTEM_DATA}}";

[[nodiscard]] static std::string subst(std::string_view path, std::string_view before, std::string_view after)
{
	assert(path.starts_with(before));
	return strCat(after, path.substr(before.size()));
}

[[nodiscard]] static std::vector<std::string> getPathsHelper(std::span<const std::string> input)
{
	std::vector<std::string> result;
	for (const auto& s : input) {
		if (s.starts_with(USER_OPENMSX)) {
			result.push_back(subst(s, USER_OPENMSX,
			                       FileOperations::getUserOpenMSXDir()));
		} else if (s.starts_with(USER_DATA)) {
			result.push_back(subst(s, USER_DATA,
			                       FileOperations::getUserDataDir()));
		} else if (s.starts_with(SYSTEM_DATA)) {
			result.push_back(subst(s, SYSTEM_DATA,
			                       FileOperations::getSystemDataDir()));
		} else if (s == USER_DIRS) {
			// Nothing. Keep USER_DIRS for isUserContext()
		} else {
			result.push_back(s);
		}
	}
	for (const auto& s : result) {
		assert(!FileOperations::needsTildeExpansion(s)); (void)s;
	}
	return result;
}

[[nodiscard]] static std::string resolveHelper(std::span<const std::string> pathList,
                            std::string_view filename)
{
	if (std::string filepath = FileOperations::expandTilde(
	                          FileOperations::expandCurrentDirFromDrive(std::string(filename)));
	    FileOperations::isAbsolutePath(filepath)) {
		// absolute path, don't resolve
		return filepath;
	}

	for (const auto& p : pathList) {
		std::string name = FileOperations::join(p, filename);
		assert(!FileOperations::needsTildeExpansion(name));
		if (FileOperations::exists(name)) {
			return name;
		}
	}
	// not found in any path
	throw FileException("Couldn't find ", filename, " in ", (pathList.size() == 1 ? ""sv : "any of: "sv),
		join(std::views::transform(pathList, [](const auto& p) { return FileOperations::getAbsolutePath(p); }), ", "));
}

FileContext::FileContext(std::vector<std::string>&& paths_, std::vector<std::string>&& savePaths_)
	: paths(std::move(paths_)), savePaths(std::move(savePaths_))
{
}

std::string FileContext::resolve(std::string_view filename) const
{
	std::string result = resolveHelper(getPaths(), filename);
	assert(!FileOperations::needsTildeExpansion(result));
	return result;
}

std::string FileContext::resolveCreate(std::string_view filename) const
{
	if (savePaths2.empty()) {
		savePaths2 = getPathsHelper(savePaths);
	}

	std::string result;
	try {
		result = resolveHelper(savePaths2, filename);
	} catch (FileException&) {
		const std::string& path = savePaths2.front();
		try {
			FileOperations::mkdirp(path);
		} catch (FileException&) {
			// ignore
		}
		result = FileOperations::join(path, filename);
	}
	assert(!FileOperations::needsTildeExpansion(result));
	return result;
}

std::span<const std::string> FileContext::getPaths() const
{
	if (paths2.empty()) {
		paths2 = getPathsHelper(paths);
	}
	return paths2;
}

bool FileContext::isUserContext() const
{
	return contains(paths, USER_DIRS);
}

template<typename Archive>
void FileContext::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("paths",     paths,
	             "savePaths", savePaths);
}
INSTANTIATE_SERIALIZE_METHODS(FileContext);

///

static std::string backSubstSymbols(std::string_view path)
{
	if (const std::string& systemData = FileOperations::getSystemDataDir();
	    path.starts_with(systemData)) {
		return subst(path, systemData, SYSTEM_DATA);
	}
	if (const std::string& userData = FileOperations::getUserDataDir();
	    path.starts_with(userData)) {
		return subst(path, userData, USER_DATA);
	}
	if (const std::string& userDir = FileOperations::getUserOpenMSXDir();
	    path.starts_with(userDir)) {
		return subst(path, userDir, USER_OPENMSX);
	}
	// TODO USER_DIRS (not needed ATM)
	return std::string(path);
}

FileContext configFileContext(std::string_view path, std::string_view hwDescr, std::string_view userName)
{
	return { { backSubstSymbols(path) },
	         { strCat(USER_OPENMSX, "/persistent/", hwDescr, '/', userName) } };
}

const FileContext& systemFileContext()
{
	static const FileContext result{
		{ USER_DATA, SYSTEM_DATA },
		{ USER_DATA } };
	return result;
}

const FileContext& preferSystemFileContext()
{
	static const FileContext result{
		{ SYSTEM_DATA, USER_DATA },  // first system dir
		{} };
	return result;
}

FileContext userFileContext(std::string_view savePath)
{
	return { { std::string{}, USER_DIRS },
	         { strCat(USER_OPENMSX, "/persistent/", savePath) } };
}

const FileContext& userFileContext()
{
	static const FileContext result{ { std::string{}, USER_DIRS }, {} };
	return result;
}

FileContext userDataFileContext(std::string_view subDir)
{
	return { { std::string{}, strCat(USER_OPENMSX, '/', subDir) },
	         {} };
}

const FileContext& currentDirFileContext()
{
	static const FileContext result{{std::string{}}, {std::string{}}};
	return result;
}

} // namespace openmsx
