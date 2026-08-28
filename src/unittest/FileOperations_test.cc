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

TEST_CASE("cleanFilename")
{
	// Characters that are valid on every platform are left alone.
	CHECK(cleanFilename("") == "");
	CHECK(cleanFilename("openmsx") == "openmsx");
	CHECK(cleanFilename("Philips NMS 8250") == "Philips NMS 8250");
	CHECK(cleanFilename("setup-1.oms") == "setup-1.oms");

	// Path separators are always replaced, this is the actual bug.
	CHECK(cleanFilename("Philips VG 8235/39") == "Philips VG 8235_39");
	CHECK(cleanFilename("/") == "_");
	CHECK(cleanFilename("a/b/c") == "a_b_c");

	// Control characters and DEL are always replaced.
	CHECK(cleanFilename("a\tb") == "a_b");
	CHECK(cleanFilename("a\nb") == "a_b");
	CHECK(cleanFilename("a\x7f" "b") == "a_b");

	// Non-ASCII (UTF-8) is left alone: only bytes < 0x80 are considered.
	CHECK(cleanFilename("caf\xc3\xa9") == "caf\xc3\xa9");
	CHECK(cleanFilename("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e") ==
	                    "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"); // 日本語
	// A real YEN SIGN is a valid filename character and must be kept. Note
	// that on a Japanese Windows the path separator is *displayed* as a yen
	// sign, but it is the ordinary backslash (0x5c), which is handled above.
	CHECK(cleanFilename("a\xc2\xa5" "b") == "a\xc2\xa5" "b");         // U+00A5 ¥
	CHECK(cleanFilename("a\xef\xbf\xa5" "b") == "a\xef\xbf\xa5" "b"); // U+FFE5 ￥
	// U+8868 is 0x95 0x5c in Shift-JIS: scanning *those* bytes for a
	// backslash would corrupt it. openMSX filenames are UTF-8 (0xe8 0xa1
	// 0xa8 here), so no such trailing byte can occur.
	CHECK(cleanFilename("\xe8\xa1\xa8") == "\xe8\xa1\xa8");           // U+8868 表

	// The result always has the same length as the input.
	auto checkLen = [](std::string_view input) {
		CHECK(cleanFilename(input).size() == input.size());
	};
	checkLen("Yamaha CX5MII/128");
	checkLen("Mitsubishi ML-G30/model 1");
	checkLen("caf\xc3\xa9/\xe6\x97\xa5\xe6\x9c\xac");

#ifdef _WIN32
	CHECK(cleanFilename(R"(a\b)") == "a_b");
	CHECK(cleanFilename("C:foo") == "C_foo");
	CHECK(cleanFilename("a?b*c") == "a_b_c");
	CHECK(cleanFilename("a[b]c") == "a_b_c");
#else
	// These are legal on UNIX and must be preserved.
	CHECK(cleanFilename(R"(a\b)") == R"(a\b)");
	CHECK(cleanFilename("a:b") == "a:b");
	CHECK(cleanFilename("a[b]c") == "a[b]c");
#endif
}
