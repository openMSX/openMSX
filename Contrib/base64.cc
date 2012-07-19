// compile with:
//   g++ -Wall -Os base64.cc -I ../src/utils/ ../src/utils/Base64.cc -lz -o encode-gz-base64
//   g++ -Wall -Os base64.cc -I ../src/utils/ ../src/utils/Base64.cc -lz -o decode-gz-base64

#include "Base64.hh"
#include <vector>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <zlib.h>

using namespace std;

string encode(const void* data, unsigned len)
{
	uLongf dstLen = len + len / 1000 + 12 + 1; // worst-case
	vector<unsigned char> buf(dstLen);
	if (compress2(buf.data(), &dstLen,
		      reinterpret_cast<const Bytef*>(data), len, 9)
	    != Z_OK) {
		cerr << "Error while compressing blob." << endl;
		exit(1);
	}
	return Base64::encode(buf.data(), dstLen);
}

string decode(const char* data, unsigned len)
{
	static const unsigned MAX_SIZE = 1024 * 1024; // 1MB
	string tmp = Base64::decode(string(data, len));
	vector<char> buf(MAX_SIZE);
	uLongf dstLen = MAX_SIZE;
	if (uncompress(reinterpret_cast<Bytef*>(buf.data()), &dstLen,
			reinterpret_cast<const Bytef*>(tmp.data()), uLong(tmp.size()))
	    != Z_OK) {
		cerr << "Error while decompressing blob." << endl;
		exit(1);
	}
	return string(buf.data(), dstLen);
}

int main(int argc, char** argv)
{
	if (argc != 3) {
		cerr << "Usage: " << argv[0] << " <input> <output>\n";
		exit(1);
	}
	FILE* inf = fopen(argv[1], "rb");
	if (!inf) {
		cerr << "Error while opening " << argv[1] << endl;
		exit(1);
	}
	struct stat st;
	fstat(fileno(inf), &st);
	size_t size = st.st_size;
	vector<char> inBuf(size);
	if (fread(inBuf.data(), size, 1, inf) != 1) {
		cerr << "Error whle reading " << argv[1] << endl;
		exit(1);
	}

	string result;
	if        (strstr(argv[0], "encode-gz-base64")) {
		result = encode(inBuf.data(), inBuf.size());
	} else if (strstr(argv[0], "decode-gz-base64")) {
		result = decode(inBuf.data(), inBuf.size());
	} else {
		cerr << "This executable should be named 'encode-gz-base64' or "
		        "'decode-gz-base64'." << endl;
		exit(1);
	}

	FILE* outf = fopen(argv[2], "wb+");
	if (!outf) {
		cerr << "Error while opening " << argv[2] << endl;
		exit(1);
	}

	if (fwrite(result.data(), result.size(), 1, outf) != 1) {
		cerr << "Error whle writing " << argv[2] << endl;
		exit(1);
	}
}
