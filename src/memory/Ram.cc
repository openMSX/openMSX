#include "Ram.hh"

#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "MSXException.hh"

#include "Base64.hh"
#include "HexDump.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "serialize.hh"

#include <zlib.h>

#include <algorithm>
#include <bit>

namespace openmsx {

Ram::Ram(const DeviceConfig& config, const std::string& name,
         static_string_view description, size_t size, bool* debugWrite)
	: xml(*config.getXML())
	, ram(size)
	, debuggable(std::in_place,
		config.getMotherBoard(), name, description, *this, debugWrite ? debugWrite : &dummyDebugWrite)
{
	clear();
}

Ram::Ram(const XMLElement& xml_, size_t size)
	: xml(xml_)
	, ram(size)
{
	clear();
}

void Ram::clear(uint8_t c)
{
	if (const auto* init = xml.findChild("initialContent")) {
		// get pattern (and decode)
		auto encoding = init->getAttributeValue("encoding");
		size_t done = 0;
		if (encoding == "gz-base64") {
			auto buf = Base64::decode(init->getData());
			auto dstLen = narrow<uLongf>(size());
			if (uncompress(std::bit_cast<Bytef*>(ram.data()), &dstLen,
			               std::bit_cast<const Bytef*>(buf.data()), uLong(buf.size()))
			     != Z_OK) {
				throw MSXException("Error while decompressing initialContent.");
			}
			done = dstLen;
		} else if (encoding == one_of("hex", "base64")) {
			auto buf = (encoding == "hex")
			       ? HexDump::decode(init->getData())
			       : Base64 ::decode(init->getData());
			if (buf.empty()) {
				throw MSXException("Zero-length initial pattern");
			}
			done = std::min(size(), buf.size());
			copy_to_range(buf.first(done), ram);
		} else {
			throw MSXException("Unsupported encoding \"", encoding,
			                   "\" for initialContent");
		}

		// repeat pattern over whole ram
		auto left = size() - done;
		while (left) {
			auto tmp = std::min(done, left);
			copy_to_range(std::span{ram.data(), tmp}, subspan(ram, done));
			done += tmp;
			left -= tmp;
		}
	} else {
		// no init pattern specified
		std::ranges::fill(*this, c);
	}
}

const std::string& Ram::getName() const
{
	assert(debuggable);
	return debuggable->getName();
}

RamDebuggable::RamDebuggable(MSXMotherBoard& motherBoard_,
                             const std::string& name_,
                             static_string_view description_, Ram& ram_, bool* debugWrite_)
	: SimpleDebuggable(motherBoard_, name_, description_, narrow<unsigned>(ram_.size()))
	, ram(ram_)
	, debugWrite(debugWrite_)
{
}

uint8_t RamDebuggable::read(unsigned address)
{
	return ram[address];
}

void RamDebuggable::readBlock(unsigned start, std::span<uint8_t> output)
{
	copy_to_range(std::span{ram}.subspan(start, output.size()), output);
}

void RamDebuggable::write(unsigned address, uint8_t value)
{
	ram[address] = value;
	*debugWrite = true; // for TrackedRam
}


template<typename Archive>
void Ram::serialize(Archive& ar, unsigned /*version*/)
{
	// ar.serialize_blob("ram", std::span{*this}); // TODO error with clang-15/libc++
	ar.serialize_blob("ram", std::span{begin(), end()});
}
INSTANTIATE_SERIALIZE_METHODS(Ram);

} // namespace openmsx
