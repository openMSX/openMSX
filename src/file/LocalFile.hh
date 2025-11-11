#ifndef LOCALFILE_HH
#define LOCALFILE_HH

#include "File.hh"
#include "FileBase.hh"
#include "FileOperations.hh"

#include "systemfuncs.hh"

#include <cstdio>

namespace openmsx {

class LocalFile final : public FileBase
{
public:
	LocalFile(std::string filename, File::OpenMode mode);
	LocalFile(std::string filename, const char* mode);

	void read(std::span<uint8_t> buffer) override;
	void write(std::span<const uint8_t> buffer) override;
	[[nodiscard]] MappedFileImpl mmap(size_t extra, bool is_const) override;
	[[nodiscard]] size_t getSize() override;
	void seek(size_t pos) override;
	[[nodiscard]] size_t getPos() override;
#if HAVE_FTRUNCATE
	void truncate(size_t size) override;
#endif
	void flush() override;
	[[nodiscard]] const std::string& getURL() const override;
	[[nodiscard]] std::string getLocalReference() override;
	[[nodiscard]] bool isReadOnly() const override;
	[[nodiscard]] time_t getModificationDate() override;

	void preCacheFile();

	[[nodiscard]] int getFD() const { return fileno(file.get()); }

private:
	std::string filename;
	FileOperations::FILE_t file;
	bool readOnly = false;
};

} // namespace openmsx

#endif
