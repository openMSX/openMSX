#include "catch.hpp"

#include "FilePoolCore.hh"
#include "File.hh"
#include "FileOperations.hh"
#include "one_of.hh"
#include "StringOp.hh"
#include "Timer.hh"
#include <iostream>
#include <fstream>

using namespace openmsx;

static void createFile(const std::string filename, const std::string content)
{
	std::ofstream of(filename);
	of << content;
}

static std::vector<std::string> readLines(const std::string& filename)
{
	std::vector<std::string> result;
	std::ifstream is(filename);
	std::string line;
	while (std::getline(is, line)) {
		result.push_back(line);
	}
	return result;
}

TEST_CASE("FilePoolCore")
{
	auto tmp = FileOperations::getTempDir() + "/filepool_unittest";
	FileOperations::deleteRecursive(tmp);
	FileOperations::mkdirp(tmp);
	createFile(tmp + "/a",  "aaa"); // 7e240de74fb1ed08fa08d38063f6a6a91462a815
	createFile(tmp + "/a2", "aaa"); // same content, different filename
	createFile(tmp + "/b",  "bbb"); // 5cb138284d431abd6a053a56625ec088bfb88912

	auto getDirectories = [&] {
		FilePoolCore::Directories result;
		result.push_back(FilePoolCore::Dir{tmp, FileType::ROM});
		return result;
	};

	{
		// create pool
		FilePoolCore pool(tmp + "/cache",
				  getDirectories,
				  [](std::string_view) { /* report progress: nothing */});

		// lookup, success
		{
			auto file = pool.getFile(FileType::ROM, Sha1Sum("7e240de74fb1ed08fa08d38063f6a6a91462a815"));
			CHECK(file.is_open());
			CHECK(file.getURL() == one_of(tmp + "/a", tmp + "/a2"));
		}
		// lookup, not (yet) present
		{
			auto file = pool.getFile(FileType::ROM, Sha1Sum("f36b4825e5db2cf7dd2d2593b3f5c24c0311d8b2"));
			CHECK(!file.is_open());
		}
		// lookup, success
		{
			auto file = pool.getFile(FileType::ROM, Sha1Sum("5cb138284d431abd6a053a56625ec088bfb88912"));
			CHECK(file.is_open());
			CHECK(file.getURL() == tmp + "/b");
		}
		// create new file, and lookup
		createFile(tmp + "/c",  "ccc"); // f36b4825e5db2cf7dd2d2593b3f5c24c0311d8b2
		{
			auto file = pool.getFile(FileType::ROM, Sha1Sum("f36b4825e5db2cf7dd2d2593b3f5c24c0311d8b2"));
			CHECK(file.is_open());
			CHECK(file.getURL() == tmp + "/c");
		}
		// modify file, old no longer present, new is present
		FileOperations::unlink(tmp + "/b");
		Timer::sleep(1'000'000); // sleep because timestamps are only accurate to 1 second
		createFile(tmp + "/b",  "BBB"); // aa6878b1c31a9420245df1daffb7b223338737a3
		{
			auto file1 = pool.getFile(FileType::ROM, Sha1Sum("5cb138284d431abd6a053a56625ec088bfb88912"));
			CHECK(!file1.is_open());
			auto file2 = pool.getFile(FileType::ROM, Sha1Sum("aa6878b1c31a9420245df1daffb7b223338737a3"));
			CHECK(file2.is_open());
			CHECK(file2.getURL() == tmp + "/b");
		}
		// modify file, but keep same content, IOW only update timestamp
		FileOperations::unlink(tmp + "/b");
		Timer::sleep(1'000'000); // sleep because timestamps are only accurate to 1 second
		createFile(tmp + "/b",  "BBB"); // aa6878b1c31a9420245df1daffb7b223338737a3
		{
			auto file = pool.getFile(FileType::ROM, Sha1Sum("aa6878b1c31a9420245df1daffb7b223338737a3"));
			CHECK(file.is_open());
			CHECK(file.getURL() == tmp + "/b");
		}
		// remove file
		FileOperations::unlink(tmp + "/b");
		{
			auto file = pool.getFile(FileType::ROM, Sha1Sum("aa6878b1c31a9420245df1daffb7b223338737a3"));
			CHECK(!file.is_open());
		}

		// calc sha1 of cached file
		{
			File file(tmp + "/a2");
			CHECK(file.is_open());
			auto sum = pool.getSha1Sum(file);
			CHECK(sum == Sha1Sum("7e240de74fb1ed08fa08d38063f6a6a91462a815"));
		}
		// calc sha1 of new file
		createFile(tmp + "/e",  "eee");
		{
			File file(tmp + "/e");
			CHECK(file.is_open());
			auto sum = pool.getSha1Sum(file);
			CHECK(sum == Sha1Sum("637a81ed8e8217bb01c15c67c39b43b0ab4e20f1"));
		}
	}

	// write 'filecache' to disk
	auto lines = readLines(tmp + "/cache");
	CHECK(lines.size() == 4);
	CHECK(lines[0].starts_with("637a81ed8e8217bb01c15c67c39b43b0ab4e20f1"));
	CHECK(lines[0].ends_with(tmp + "/e"));
	CHECK(lines[1].starts_with("7e240de74fb1ed08fa08d38063f6a6a91462a815"));
	CHECK((lines[1].ends_with(tmp + "/a") ||
	       lines[1].ends_with(tmp + "/a2")));
	CHECK(lines[2].starts_with("7e240de74fb1ed08fa08d38063f6a6a91462a815"));
	CHECK((lines[2].ends_with(tmp + "/a") ||
	       lines[2].ends_with(tmp + "/a2")));
	CHECK(lines[3].starts_with("f36b4825e5db2cf7dd2d2593b3f5c24c0311d8b2"));
	CHECK(lines[3].ends_with(tmp + "/c"));

	FileOperations::deleteRecursive(tmp);
}
