#include "serialize.hh"
#include "Base64.hh"
#include "HexDump.hh"
#include "XMLLoader.hh"
#include "XMLElement.hh"
#include "ConfigException.hh"
#include "XMLException.hh"
#include "snappy.hh"
#include "MemBuffer.hh"
#include "StringOp.hh"
#include "FileOperations.hh"
#include "Version.hh"
#include "Date.hh"
#include "cstdiop.hh" // for dup()
#include <cstring>
#include <limits>

using std::string;

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

OutputArchiveBase2::OutputArchiveBase2()
	: lastId(0)
{
}

unsigned OutputArchiveBase2::generateID1(const void* p)
{
	#ifdef linux
	assert("Can't serialize ID of object located on the stack" &&
	       !addressOnStack(p));
	#endif
	++lastId;
	assert(polyIdMap.find(p) == end(polyIdMap));
	polyIdMap[p] = lastId;
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
	auto key = std::make_pair(p, std::type_index(typeInfo));
	assert(idMap.find(key) == end(idMap));
	idMap[key] = lastId;
	return lastId;
}

unsigned OutputArchiveBase2::getID1(const void* p)
{
	auto it = polyIdMap.find(p);
	return it != end(polyIdMap) ? it->second : 0;
}
unsigned OutputArchiveBase2::getID2(
	const void* p, const std::type_info& typeInfo)
{
	auto it = idMap.find({p, std::type_index(typeInfo)});
	return it != end(idMap) ? it->second : 0;
}


template<typename Derived>
void OutputArchiveBase<Derived>::serialize_blob(
	const char* tag, const void* data_, size_t len)
{
	auto* data = static_cast<const uint8_t*>(data_);

	string encoding;
	string tmp;
	if (false) {
		// useful for debugging
		encoding = "hex";
		tmp = HexDump::encode(data, len);
	} else if (false) {
		encoding = "base64";
		tmp = Base64::encode(data, len);
	} else {
		encoding = "gz-base64";
		// TODO check for overflow?
		auto dstLen = uLongf(len + len / 1000 + 12 + 1); // worst-case
		MemBuffer<byte> buf(dstLen);
		if (compress2(buf.data(), &dstLen,
		              reinterpret_cast<const Bytef*>(data),
		              uLong(len), 9)
		    != Z_OK) {
			throw MSXException("Error while compressing blob.");
		}
		tmp = Base64::encode(buf.data(), dstLen);
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
	auto it = idMap.find(id);
	return it != end(idMap) ? it->second : nullptr;
}

void InputArchiveBase2::addPointer(unsigned id, const void* p)
{
	assert(idMap.find(id) == end(idMap));
	idMap[id] = const_cast<void*>(p);
}

unsigned InputArchiveBase2::getId(const void* ptr) const
{
	for (const auto& p : idMap) {
		if (p.second == ptr) return p.first;
	}
	return 0;
}

template<typename Derived>
void InputArchiveBase<Derived>::serialize_blob(
	const char* tag, void* data, size_t len)
{
	this->self().beginTag(tag);
	string encoding;
	this->self().attribute("encoding", encoding);

	string_ref tmp = this->self().loadStr();
	this->self().endTag(tag);

	if (encoding == "gz-base64") {
		auto p = Base64::decode(tmp);
		auto dstLen = uLongf(len); // TODO check for overflow?
		if ((uncompress(reinterpret_cast<Bytef*>(data), &dstLen,
		                reinterpret_cast<const Bytef*>(p.first.data()), uLong(p.second))
		     != Z_OK) ||
		    (dstLen != len)) {
			throw MSXException("Error while decompressing blob.");
		}
	} else if ((encoding == "hex") || (encoding == "base64")) {
		bool ok = (encoding == "hex")
		        ? HexDump::decode_inplace(tmp, static_cast<uint8_t*>(data), len)
		        : Base64 ::decode_inplace(tmp, static_cast<uint8_t*>(data), len);
		if (!ok) {
			throw XMLException(StringOp::Builder()
				<< "Length of decoded blob different from "
				   "expected value (" << len << ')');
		}
	} else {
		throw XMLException("Unsupported encoding \"" + encoding + "\" for blob");
	}
}

template class InputArchiveBase<MemInputArchive>;
template class InputArchiveBase<XmlInputArchive>;

////

void MemOutputArchive::save(const std::string& s)
{
	auto size = s.size();
	byte* buf = buffer.allocate(sizeof(size) + size);
	memcpy(buf, &size, sizeof(size));
	memcpy(buf + sizeof(size), s.data(), size);
}

MemBuffer<byte> MemOutputArchive::releaseBuffer(size_t& size)
{
	return buffer.release(size);
}

////

void MemInputArchive::load(std::string& s)
{
	size_t length;
	load(length);
	s.resize(length);
	if (length) {
		get(&s[0], length);
	}
}

string_ref MemInputArchive::loadStr()
{
	size_t length;
	load(length);
	const byte* p = buffer.getCurrentPos();
	buffer.skip(length);
	return string_ref(reinterpret_cast<const char*>(p), length);
}

////

// Too small inputs don't compress very well (often the compressed size is even
// bigger than the input). It also takes a relatively long time (because snappy
// has a relatively large setup time). I choose this value semi-arbitrary. I
// only made it >= 52 so that the (incompressible) RP5C01 registers won't be
// compressed.
static const size_t SMALL_SIZE = 100;
void MemOutputArchive::serialize_blob(const char*, const void* data, size_t len)
{
	// Compress in-memory blobs:
	//
	// This is a bit slower than memcpy, but it uses a lot less memory.
	// Memory usage is important for the reverse feature, where we keep a
	// lot of savestates in memory.
	//
	// I compared 'gzip level=1' (fastest version with lowest compression
	// ratio) with 'lzo'. lzo was considerably faster. Compression ratio
	// was about the same (maybe lzo was slightly better (OTOH on higher
	// levels gzip compresses better)). So I decided to go with lzo.
	//
	// Later I compared 'lzo' with 'snappy', lzo compresses 6-25% better,
	// but 'snappy' is about twice as fast. So I switched to 'snappy'.
	if (len >= SMALL_SIZE) {
		size_t dstLen = snappy::maxCompressedLength(len);
		byte* buf = buffer.allocate(sizeof(dstLen) + dstLen);
		snappy::compress(static_cast<const char*>(data), len,
		                 reinterpret_cast<char*>(&buf[sizeof(dstLen)]), dstLen);
		memcpy(buf, &dstLen, sizeof(dstLen)); // fill-in actual size
		buffer.deallocate(&buf[sizeof(dstLen) + dstLen]); // dealloc unused portion
	} else {
		byte* buf = buffer.allocate(len);
		memcpy(buf, data, len);
	}

}

void MemInputArchive::serialize_blob(const char*, void* data, size_t len)
{
	if (len >= SMALL_SIZE) {
		size_t srcLen; load(srcLen);
		snappy::uncompress(reinterpret_cast<const char*>(buffer.getCurrentPos()),
		                   srcLen, reinterpret_cast<char*>(data), len);
		buffer.skip(srcLen);
	} else {
		memcpy(data, buffer.getCurrentPos(), len);
		buffer.skip(len);
	}
}

////

XmlOutputArchive::XmlOutputArchive(const string& filename)
	: root("serial")
{
	root.addAttribute("openmsx_version", Version::full());
	root.addAttribute("date_time", Date::toString(time(nullptr)));
	root.addAttribute("platform", TARGET_PLATFORM);
	{
		auto f = FileOperations::openFile(filename, "wb");
		if (!f) goto error;
		int duped_fd = dup(fileno(f.get()));
		if (duped_fd == -1) goto error;
		file = gzdopen(duped_fd, "wb9");
		if (!file) {
			close(duped_fd);
			goto error;
		}
		current.push_back(&root);
		return; // success
		// on scope-exit 'File* f' is closed, and 'gzFile file'
		// uses the dup()'ed file descriptor.
	}

error:
	throw XMLException("Could not open compressed file \"" + filename + "\"");
}

XmlOutputArchive::~XmlOutputArchive()
{
	assert(current.back() == &root);
	const char* header =
	    "<?xml version=\"1.0\" ?>\n"
	    "<!DOCTYPE openmsx-serialize SYSTEM 'openmsx-serialize.dtd'>\n";
	gzwrite(file, const_cast<char*>(header), unsigned(strlen(header)));
	string dump = root.dump();
	gzwrite(file, const_cast<char*>(dump.data()), unsigned(dump.size()));
	gzclose(file);
}

void XmlOutputArchive::saveChar(char c)
{
	save(string(1, c));
}
void XmlOutputArchive::save(const string& str)
{
	assert(!current.empty());
	assert(current.back()->getData().empty());
	current.back()->setData(str);
}
void XmlOutputArchive::save(bool b)
{
	assert(!current.empty());
	assert(current.back()->getData().empty());
	current.back()->setData(b ? "true" : "false");
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

void XmlOutputArchive::attribute(const char* name, const string& str)
{
	assert(!current.empty());
	assert(!current.back()->hasAttribute(name));
	current.back()->addAttribute(name, str);
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
	assert(!current.empty());
	auto& elem = current.back()->addChild(tag);
	current.push_back(&elem);
}
void XmlOutputArchive::endTag(const char* tag)
{
	assert(!current.empty());
	assert(current.back()->getName() == tag); (void)tag;
	current.pop_back();
}

////

XmlInputArchive::XmlInputArchive(const string& filename)
	: rootElem(XMLLoader::load(filename, "openmsx-serialize.dtd"))
{
	elems.emplace_back(&rootElem, 0);
}

string_ref XmlInputArchive::loadStr()
{
	if (!elems.back().first->getChildren().empty()) {
		throw XMLException("No child tags expected for primitive type");
	}
	return elems.back().first->getData();
}
void XmlInputArchive::load(string& t)
{
	t = loadStr().str();
}
void XmlInputArchive::loadChar(char& c)
{
	std::string str;
	load(str);
	std::istringstream is(str);
	is >> c;
}
void XmlInputArchive::load(bool& b)
{
	string_ref s = loadStr();
	if ((s == "true") || (s == "1")) {
		b = true;
	} else if ((s == "false") || (s == "0")) {
		b = false;
	} else {
		throw XMLException("Bad value found for boolean: " + s);
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
template<bool IS_SIGNED> struct ConditionalNegate;
template<> struct ConditionalNegate<true> {
	template<typename T> void operator()(bool negate, T& t) {
		if (negate) t = -t; // ok to negate a signed type
	}
};
template<> struct ConditionalNegate<false> {
	template<typename T> void operator()(bool negate, T& /*t*/) {
		assert(!negate); (void)negate; // can't negate unsigned type
	}
};
template<typename T> static inline void fastAtoi(string_ref str, T& t)
{
	t = 0;
	bool neg = false;
	size_t i = 0;
	size_t l = str.size();

	static const bool IS_SIGNED = std::numeric_limits<T>::is_signed;
	if (IS_SIGNED) {
		if (l == 0) return;
		if (str[0] == '-') {
			neg = true;
			i = 1;
		}
	}
	for (/**/; i < l; ++i) {
		unsigned d = str[i] - '0';
		if (unlikely(d > 9)) {
			throw XMLException("Invalid integer: " + str);
		}
		t = 10 * t + d;
	}
	// The following stuff does the equivalent of:
	//    if (neg) t = -t;
	// Though this expression triggers a warning on VC++ when T is an
	// unsigned type. This complex template stuff avoids the warning.
	ConditionalNegate<IS_SIGNED> negateFunctor;
	negateFunctor(neg, t);
}
void XmlInputArchive::load(int& i)
{
	string_ref str = loadStr();
	fastAtoi(str, i);
}
void XmlInputArchive::load(unsigned& u)
{
	string_ref str = loadStr();
	fastAtoi(str, u);
}
void XmlInputArchive::load(unsigned long long& ull)
{
	string_ref str = loadStr();
	fastAtoi(str, ull);
}
void XmlInputArchive::load(unsigned char& b)
{
	unsigned i;
	load(i);
	b = i;
}
void XmlInputArchive::load(signed char& c)
{
	int i;
	load(i);
	c = i;
}
void XmlInputArchive::load(char& c)
{
	int i;
	load(i);
	c = i;
}

void XmlInputArchive::beginTag(const char* tag)
{
	auto* child = elems.back().first->findNextChild(
		tag, elems.back().second);
	if (!child) {
		string path;
		for (auto& e : elems) {
			path += e.first->getName() + '/';
		}
		throw XMLException(StringOp::Builder() <<
			"No child tag \"" << tag <<
			"\" found at location \"" << path << '\"');
	}
	elems.emplace_back(child, 0);
}
void XmlInputArchive::endTag(const char* tag)
{
	const auto& elem = *elems.back().first;
	if (elem.getName() != tag) {
		throw XMLException("End tag \"" + elem.getName() +
			"\" not equal to begin tag \"" + tag + "\"");
	}
	auto& elem2 = const_cast<XMLElement&>(elem);
	elem2.clearName(); // mark this elem for later beginTag() calls
	elems.pop_back();
}

void XmlInputArchive::attribute(const char* name, string& t)
{
	try {
		t = elems.back().first->getAttribute(name);
	} catch (ConfigException& ex) {
		throw XMLException(ex.getMessage());
	}
}
void XmlInputArchive::attribute(const char* name, int& i)
{
	attributeImpl(name, i);
}
void XmlInputArchive::attribute(const char* name, unsigned& u)
{
	attributeImpl(name, u);
}
bool XmlInputArchive::hasAttribute(const char* name)
{
	return elems.back().first->hasAttribute(name);
}
bool XmlInputArchive::findAttribute(const char* name, unsigned& value)
{
	return elems.back().first->findAttributeInt(name, value);
}
int XmlInputArchive::countChildren() const
{
	return int(elems.back().first->getChildren().size());
}

} // namespace openmsx
