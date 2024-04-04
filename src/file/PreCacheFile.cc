#include "PreCacheFile.hh"
#include "FileOperations.hh"
#include "narrow.hh"
#include <array>
#include <cstdio>
#include <sys/types.h>

namespace openmsx {

// TODO when available use:
//     posix_fadvise(int fd, off_t offset, off_t len, int advise)
//       with advise = POSIX_FADV_WILLNEED

PreCacheFile::PreCacheFile(std::string name_)
	: name(std::move(name_)), exitLoop(false)
{
	thread = std::jthread([this]() { run(); });
}

PreCacheFile::~PreCacheFile()
{
	exitLoop = true;
}

void PreCacheFile::run()
{
	if (!FileOperations::isRegularFile(name)) {
		// don't pre-cache non regular files (e.g. /dev/fd0)
		return;
	}

	auto file = FileOperations::openFile(name, "rb");
	if (!file) return;

	fseek(file.get(), 0, SEEK_END);
	auto size = ftell(file.get());
	if (size < 1024L * 1024L) {
		// only pre-cache small files

		const size_t BLOCK_SIZE = 4096;
		unsigned block = 0;
		unsigned repeat = 0;
		while (true) {
			if (exitLoop) break;

			std::array<char, BLOCK_SIZE> buf;
			if (fseek(file.get(), narrow_cast<long>(block * BLOCK_SIZE), SEEK_SET)) break;
			size_t read = fread(buf.data(), 1, BLOCK_SIZE, file.get());
			if (read != BLOCK_SIZE) {
				// error or end-of-file reached,
				// in both cases stop pre-caching
				break;
			}

			// Just reading a file linearly from front to back
			// makes Linux classify the read as a 'streaming read'.
			// Linux doesn't cache those. To avoid this we read
			// some of the blocks twice.
			if (repeat != 0) {
				--repeat; ++block;
			} else {
				repeat = 5;
			}
		}
	}
}

} // namespace openmsx
