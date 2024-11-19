#include "serialize.hh"

#include "Base64.hh"
#include "HexDump.hh"
#include "XMLElement.hh"
#include "XMLException.hh"
#include "DeltaBlock.hh"
#include "MemBuffer.hh"
#include "FileOperations.hh"
#include "Version.hh"
#include "Date.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "stl.hh"
#include "build-info.hh"

#include <bit>
#include "cstdiop.hh" // for dup()
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

using std::string;
using std::string_view;

namespace openmsx {

template<typename Derived>
void ArchiveBase<Derived>::attribute(const char* name, const char* value)
{
	string valueStr(value);
	self().attribute(name, valueStr);
}
template class ArchiveBase<MemOutputArchive>;
template class ArchiveBase<XmlOutputArchive>;

////

unsigned OutputArchiveBase2::generateID1(const void* p)
{
	#ifdef linux
	assert("Can't serialize ID of object located on the stack" &&
	       !addressOnStack(p));
	#endif
	++lastId;
	assert(!polyIdMap.contains(p));
	polyIdMap.emplace_noDuplicateCheck(p, lastId);
	return lastId;
}
unsigned OutputArchiveBase2::generateID2(
	const void* p, const std::type_info& typeInfo)
{
	#ifdef linux
	assert("Can't serialize ID of object located on the stack" &&
	       !addressOnStack(p));
	#endif
	++lastId;
	auto key = std::pair(p, std::type_index(typeInfo));
	assert(!idMap.contains(key));
	idMap.emplace_noDuplicateCheck(key, lastId);
	return lastId;
}

unsigned OutputArchiveBase2::getID1(const void* p)
{
	const auto* v = lookup(polyIdMap, p);
	return v ? *v : 0;
}
unsigned OutputArchiveBase2::getID2(
	const void* p, const std::type_info& typeInfo)
{
	const auto* v = lookup(idMap, std::pair(p, std::type_index(typeInfo)));
	return v ? *v : 0;
}


template<typename Derived>
void OutputArchiveBase<Derived>::serialize_blob(
	const char* tag, std::span<const uint8_t> data, bool /*diff*/)
{
	string encoding;
	string tmp;
	if (false) {
		// useful for debugging
		encoding = "hex";
		tmp = HexDump::encode(data);
	} else if (false) {
		encoding = "base64";
		tmp = Base64::encode(data);
	} else {
		encoding = "gz-base64";
		// TODO check for overflow?
		auto len = data.size();
		auto dstLen = uLongf(len + len / 1000 + 12 + 1); // worst-case
		MemBuffer<uint8_t> buf(dstLen);
		if (compress2(buf.data(), &dstLen,
		              std::bit_cast<const Bytef*>(data.data()),
		              uLong(len), 9)
		    != Z_OK) {
			throw MSXException("Error while compressing blob.");
		}
		tmp = Base64::encode(buf.first(dstLen));
	}
	this->self().beginTag(tag);
	this->self().attribute("encoding", encoding);
	Saver<string> saver;
	saver(this->self(), tmp, false);
	this->self().endTag(tag);
}

template class OutputArchiveBase<MemOutputArchive>;
template class OutputArchiveBase<XmlOutputArchive>;

////

void* InputArchiveBase2::getPointer(unsigned id)
{
	auto* v = lookup(idMap, id);
	return v ? *v : nullptr;
}

void InputArchiveBase2::addPointer(unsigned id, const void* p)
{
	assert(!idMap.contains(id));
	idMap.emplace_noDuplicateCheck(id, const_cast<void*>(p));
}

unsigned InputArchiveBase2::getId(const void* ptr) const
{
	for (const auto& [id, pt] : idMap) {
		if (pt == ptr) return id;
	}
	return 0;
}

template<typename Derived>
void InputArchiveBase<Derived>::serialize_blob(
	const char* tag, std::span<uint8_t> data, bool /*diff*/)
{
	this->self().beginTag(tag);
	string encoding;
	this->self().attribute("encoding", encoding);

	string_view tmp = this->self().loadStr();
	this->self().endTag(tag);

	if (encoding == "gz-base64") {
		auto buf = Base64::decode(tmp);
		auto dstLen = uLongf(data.size()); // TODO check for overflow?
		if ((uncompress(std::bit_cast<Bytef*>(data.data()), &dstLen,
		                std::bit_cast<const Bytef*>(buf.data()), uLong(buf.size()))
		     != Z_OK) ||
		    (dstLen != data.size())) {
			throw MSXException("Error while decompressing blob.");
		}
	} else if (encoding == one_of("hex", "base64")) {
		bool ok = (encoding == "hex")
		        ? HexDump::decode_inplace(tmp, data)
		        : Base64 ::decode_inplace(tmp, data);
		if (!ok) {
			throw XMLException(
				"Length of decoded blob different from "
				"expected value (", data.size(), ')');
		}
	} else {
		throw XMLException("Unsupported encoding \"", encoding, "\" for blob");
	}
}

template class InputArchiveBase<MemInputArchive>;
template class InputArchiveBase<XmlInputArchive>;

////

void MemOutputArchive::save(std::string_view s)
{
	auto size = s.size();
	auto buf = buffer.allocate(sizeof(size) + size);
	memcpy(buf.data(), &size, sizeof(size));
	ranges::copy(s, subspan(buf, sizeof(size)));
}

////

void MemInputArchive::load(std::string& s)
{
	size_t length;
	load(length);
	s.resize(length);
	if (length) {
		buffer.read(s.data(), length);
	}
}

string_view MemInputArchive::loadStr()
{
	size_t length;
	load(length);
	const uint8_t* p = buffer.getCurrentPos();
	buffer.skip(length);
	return {std::bit_cast<const char*>(p), length};
}

////

// Too small inputs don't compress very well (often the compressed size is even
// bigger than the input). It also takes a relatively long time (because often
// compression has a relatively large setup time). I choose this value
// semi-arbitrary. I only made it >= 52 so that the (incompressible) RP5C01
// registers won't be compressed.
static constexpr size_t SMALL_SIZE = 64;
void MemOutputArchive::serialize_blob(const char* /*tag*/, std::span<const uint8_t> data,
                                      bool diff)
{
	// Delta-compress in-memory blobs, see DeltaBlock.hh for more details.
	if (data.size() > SMALL_SIZE) {
		auto deltaBlockIdx = unsigned(deltaBlocks.size());
		save(deltaBlockIdx); // see comment below in MemInputArchive
		deltaBlocks.push_back(diff
			? lastDeltaBlocks.createNew(data.data(), data)
			: lastDeltaBlocks.createNullDiff(data.data(), data));
	} else {
		auto buf = buffer.allocate(data.size());
		ranges::copy(data, buf);
	}
}

void MemInputArchive::serialize_blob(const char* /*tag*/, std::span<uint8_t> data,
                                     bool /*diff*/)
{
	if (data.size() > SMALL_SIZE) {
		// Usually blobs are saved in the same order as they are loaded
		// (via the serialize_blob() methods in respectively
		// MemOutputArchive and MemInputArchive). In that case keeping
		// track of the deltaBlockIdx in the savestate itself is
		// redundant (it will simply be an increasing value). However
		// in rare cases, via the {begin,end,skip)Section() methods, it
		// is possible that certain blobs are stored in the savestate,
		// but skipped while loading. That's why we do need the index.
		unsigned deltaBlockIdx; load(deltaBlockIdx);
		deltaBlocks[deltaBlockIdx]->apply(data);
	} else {
		ranges::copy(std::span{buffer.getCurrentPos(), data.size()}, data);
		buffer.skip(data.size());
	}
}

////

XmlOutputArchive::XmlOutputArchive(zstring_view filename_)
	: filename(filename_)
	, writer(*this)
{
	{
		auto f = FileOperations::openFile(filename, "wb");
		if (!f) error();
		int duped_fd = dup(fileno(f.get()));
		if (duped_fd == -1) error();
		file = gzdopen(duped_fd, "wb9");
		if (!file) {
			::close(duped_fd);
			error();
		}
		// on scope-exit 'f' is closed, and 'file'
		// uses the dup()'ed file descriptor.
	}

	static constexpr std::string_view header =
		"<?xml version=\"1.0\" ?>\n"
		"<!DOCTYPE openmsx-serialize SYSTEM 'openmsx-serialize.dtd'>\n";
	write(header);

	writer.begin("serial");
	writer.attribute("openmsx_version", Version::full());
	writer.attribute("date_time", Date::toString(time(nullptr)));
	writer.attribute("platform", TARGET_PLATFORM);
}

void XmlOutputArchive::close()
{
	if (!file) return; // already closed

	writer.end("serial");

	if (gzclose(file) != Z_OK) {
		error();
	}
	file = nullptr;
}

XmlOutputArchive::~XmlOutputArchive()
{
	try {
		close();
	} catch (...) {
		// Eat exception. Explicitly call close() if you want to handle errors.
	}
}

void XmlOutputArchive::write(std::span<const char> buf)
{
	if ((gzwrite(file, buf.data(), unsigned(buf.size())) == 0) && !buf.empty()) {
		error();
	}
}

void XmlOutputArchive::write1(char c)
{
	if (gzputc(file, c) == -1) {
		error();
	}
}

void XmlOutputArchive::check(bool condition) const
{
	assert(condition); (void)condition;
}

void XmlOutputArchive::error()
{
	if (file) {
		gzclose(file);
		file = nullptr;
	}
	throw XMLException("could not write \"", filename, '"');
}

void XmlOutputArchive::saveChar(char c)
{
	writer.data(std::string_view(&c, 1));
}
void XmlOutputArchive::save(std::string_view str)
{
	writer.data(str);
}
void XmlOutputArchive::save(bool b)
{
	writer.data(b ? "true" : "false");
}
void XmlOutputArchive::save(unsigned char b)
{
	save(unsigned(b));
}
void XmlOutputArchive::save(signed char c)
{
	save(int(c));
}
void XmlOutputArchive::save(char c)
{
	save(int(c));
}
void XmlOutputArchive::save(int i)
{
	saveImpl(i);
}
void XmlOutputArchive::save(unsigned u)
{
	saveImpl(u);
}
void XmlOutputArchive::save(unsigned long long ull)
{
	saveImpl(ull);
}

void XmlOutputArchive::attribute(const char* name, std::string_view str)
{
	writer.attribute(name, str);
}
void XmlOutputArchive::attribute(const char* name, int i)
{
	attributeImpl(name, i);
}
void XmlOutputArchive::attribute(const char* name, unsigned u)
{
	attributeImpl(name, u);
}

void XmlOutputArchive::beginTag(const char* tag)
{
	writer.begin(tag);
}
void XmlOutputArchive::endTag(const char* tag)
{
	writer.end(tag);
}

////

XmlInputArchive::XmlInputArchive(const string& filename)
{
	xmlDoc.load(filename, "openmsx-serialize.dtd");
	const auto* root = xmlDoc.getRoot();
	elems.emplace_back(root, root->getFirstChild());
}

string_view XmlInputArchive::loadStr() const
{
	if (currentElement()->hasChildren()) {
		throw XMLException("No child tags expected for primitive type");
	}
	return currentElement()->getData();
}
void XmlInputArchive::load(string& t) const
{
	t = loadStr();
}
void XmlInputArchive::loadChar(char& c) const
{
	std::string str;
	load(str);
	std::istringstream is(str);
	is >> c;
}
void XmlInputArchive::load(bool& b) const
{
	string_view s = loadStr();
	if (s == one_of("true", "1")) {
		b = true;
	} else if (s == one_of("false", "0")) {
		b = false;
	} else {
		throw XMLException("Bad value found for boolean: ", s);
	}
}

// This function parses a number from a string. It's similar to the generic
// templatized XmlInputArchive::load() method, but _much_ faster. It does
// have some limitations though:
//  - it can't handle leading whitespace
//  - it can't handle extra characters at the end of the string
//  - it can only handle one base (only decimal, not octal or hexadecimal)
//  - it doesn't understand a leading '+' sign
//  - it doesn't detect overflow or underflow (The generic implementation sets
//    a 'bad' flag on the stream and clips the result to the min/max allowed
//    value. Though this 'bad' flag was ignored by the openMSX code).
// This routine is only used to parse strings we've written ourselves (and the
// savestate/replay XML files are not meant to be manually edited). So the
// above limitations don't really matter. And we can use the speed gain.
template<std::integral T> static inline void fastAtoi(string_view str, T& t)
{
	t = 0;
	bool neg = false;
	size_t i = 0;
	size_t l = str.size();

	if constexpr (std::numeric_limits<T>::is_signed) {
		if (l == 0) return;
		if (str[0] == '-') {
			neg = true;
			i = 1;
		}
	}
	for (/**/; i < l; ++i) {
		unsigned d = str[i] - '0';
		if (d > 9) [[unlikely]] {
			throw XMLException("Invalid integer: ", str);
		}
		t = 10 * t + d;
	}
	if constexpr (std::numeric_limits<T>::is_signed) {
		if (neg) t = -t;
	} else {
		assert(!neg); (void)neg;
	}
}
void XmlInputArchive::load(int& i) const
{
	string_view str = loadStr();
	fastAtoi(str, i);
}
void XmlInputArchive::load(unsigned& u) const
{
	string_view str = loadStr();
	try {
		fastAtoi(str, u);
	} catch (XMLException&) {
		// One reason could be that the type of a member was corrected
		// from 'int' to 'unsigned'. In that case loading an old
		// savestate (that contains a negative value) might fail. So try
		// again parsing as an 'int'.
		int i;
		fastAtoi(str, i);
		u = narrow_cast<unsigned>(i);
	}
}
void XmlInputArchive::load(unsigned long long& ull) const
{
	string_view str = loadStr();
	fastAtoi(str, ull);
}
void XmlInputArchive::load(unsigned char& b) const
{
	unsigned u;
	load(u);
	b = narrow_cast<unsigned char>(u);
}
void XmlInputArchive::load(signed char& c) const
{
	int i;
	load(i);
	c = narrow_cast<signed char>(i);
}
void XmlInputArchive::load(char& c) const
{
	int i;
	load(i);
	c = narrow_cast<char>(i);
}

void XmlInputArchive::beginTag(const char* tag)
{
	const auto* child = currentElement()->findChild(tag, elems.back().second);
	if (!child) {
		string path;
		for (const auto& [e, _] : elems) {
			strAppend(path, e->getName(), '/');
		}
		throw XMLException("No child tag \"", tag,
		                   "\" found at location \"", path, '\"');
	}
	elems.emplace_back(child, child->getFirstChild());
}
void XmlInputArchive::endTag(const char* tag)
{
	const auto& elem = *currentElement();
	if (elem.getName() != tag) {
		throw XMLException("End tag \"", elem.getName(),
		                   "\" not equal to begin tag \"", tag, "\"");
	}
	auto& elem2 = const_cast<XMLElement&>(elem);
	elem2.clearName(); // mark this elem for later beginTag() calls
	elems.pop_back();
}

void XmlInputArchive::attribute(const char* name, string& t) const
{
	const auto* attr = currentElement()->findAttribute(name);
	if (!attr) {
		throw XMLException("Missing attribute \"", name, "\".");
	}
	t = attr->getValue();
}
void XmlInputArchive::attribute(const char* name, int& i) const
{
	attributeImpl(name, i);
}
void XmlInputArchive::attribute(const char* name, unsigned& u) const
{
	attributeImpl(name, u);
}
bool XmlInputArchive::hasAttribute(const char* name) const
{
	return currentElement()->findAttribute(name);
}
int XmlInputArchive::countChildren() const
{
	return int(currentElement()->numChildren());
}

} // namespace openmsx
