#include "PreCacheFile.hh"
#include "FileOperations.hh"
#include "statp.hh"
#include <cstdio>
#include <sys/types.h>

namespace openmsx {

PreCacheFile::PreCacheFile(const std::string& name_)
	: name(name_), thread(this), exitLoop(false)
{
	thread.start();
}

PreCacheFile::~PreCacheFile()
{
	exitLoop = true;
	thread.join();
}

void PreCacheFile::run()
{
	struct stat st;
	if (stat(name.c_str(), &st)) return;
	if (!S_ISREG(st.st_mode)) {
		// don't pre-cache non regular files (e.g. /dev/fd0)
		return;
	}

	auto file = FileOperations::openFile(name, "rb");
	if (!file) return;

	fseek(file.get(), 0, SEEK_END);
	auto size = ftell(file.get());
	if (size < 1024 * 1024) {
		// only pre-cache small files

		const unsigned BLOCK_SIZE = 4096;
		unsigned block = 0;
		unsigned repeat = 0;
		while (true) {
			if (exitLoop) break;

			char buf[BLOCK_SIZE];
			if (fseek(file.get(), block * BLOCK_SIZE, SEEK_SET)) break;
			size_t read = fread(buf, 1, BLOCK_SIZE, file.get());
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
