#include "Ram.hh"
#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "Base64.hh"
#include "HexDump.hh"
#include "MSXException.hh"
#include "one_of.hh"
#include "serialize.hh"
#include <zlib.h>
#include <algorithm>
#include <cstring>
#include <memory>

namespace openmsx {

Ram::Ram(const DeviceConfig& config, const std::string& name,
         static_string_view description, unsigned size_)
	: xml(*config.getXML())
	, ram(size_)
	, size(size_)
	, debuggable(std::in_place,
		config.getMotherBoard(), name, description, *this)
{
	clear();
}

Ram::Ram(const XMLElement& xml_, unsigned size_)
	: xml(xml_)
	, ram(size_)
	, size(size_)
{
	clear();
}

void Ram::clear(byte c)
{
	if (const auto* init = xml.findChild("initialContent")) {
		// get pattern (and decode)
		auto encoding = init->getAttributeValue("encoding");
		size_t done = 0;
		if (encoding == "gz-base64") {
			auto [buf, bufSize] = Base64::decode(init->getData());
			uLongf dstLen = getSize();
			if (uncompress(reinterpret_cast<Bytef*>(ram.data()), &dstLen,
			               reinterpret_cast<const Bytef*>(buf.data()), uLong(bufSize))
			     != Z_OK) {
				throw MSXException("Error while decompressing initialContent.");
			}
			done = dstLen;
		} else if (encoding == one_of("hex", "base64")) {
			auto [buf, bufSize] = (encoding == "hex")
			       ? HexDump::decode(init->getData())
			       : Base64 ::decode(init->getData());
			if (bufSize == 0) {
				throw MSXException("Zero-length initial pattern");
			}
			done = std::min(size_t(size), bufSize);
			memcpy(ram.data(), buf.data(), done);
		} else {
			throw MSXException("Unsupported encoding \"", encoding,
			                   "\" for initialContent");
		}

		// repeat pattern over whole ram
		auto left = size - done;
		while (left) {
			auto tmp = std::min(done, left);
			memcpy(&ram[done], &ram[0], tmp);
			done += tmp;
			left -= tmp;
		}
	} else {
		// no init pattern specified
		memset(ram.data(), c, size);
	}

}

const std::string& Ram::getName() const
{
	return debuggable->getName();
}

RamDebuggable::RamDebuggable(MSXMotherBoard& motherBoard_,
                             const std::string& name_,
                             static_string_view description_, Ram& ram_)
	: SimpleDebuggable(motherBoard_, name_, description_, ram_.getSize())
	, ram(ram_)
{
}

byte RamDebuggable::read(unsigned address)
{
	return ram[address];
}

void RamDebuggable::write(unsigned address, byte value)
{
	ram[address] = value;
}


void Ram::serialize(Archive auto& ar, unsigned /*version*/)
{
	ar.serialize_blob("ram", ram.data(), size);
}
INSTANTIATE_SERIALIZE_METHODS(Ram);

} // namespace openmsx
