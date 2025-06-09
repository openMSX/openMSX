#include "catch.hpp"
#include "FileOperations.hh"

using namespace openmsx::FileOperations;

TEST_CASE("stem")
{
	auto check = [](std::string_view input) {
		// Verify that stem() is equivalent to stripExtension(getFilename(input))
		auto expected = stripExtension(getFilename(input));
		auto actual = stem(input);
		CHECK(actual == expected);
	};

	check("");                       // Input is empty
	check("filename");               // No '/' or '.'
	check("/path/to/filename");      // Input with '/' but no '.'
	check("filename.ext");           // Input with '.' but no '/'
	check("/path/to/filename.ext");  // Input with both '/' and '.'
	check("/path.to/file.name.ext"); // Input with multiple '.' in filename
	check("/path.to/file");          // Input with '.' in directory name
	check("/path/to/");              // Input with no filename (ends with '/')
	check("/path.to/");              // Input with no filename but has '.' in path
	check(".hidden");                //
	check("/.hidden");               //
	check("/path/to/.hidden");       //
	check("/path/to/.hidden.ext");   //

	// These might be handled differently by std::filesystem::path::stem()
	check(".");
	check("..");
	check("path/.");
	check("path/..");
}
