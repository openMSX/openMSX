#include "Ram.hh"
#include "DeviceConfig.hh"
#include "SimpleDebuggable.hh"
#include "XMLElement.hh"
#include "Base64.hh"
#include "HexDump.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include "memory.hh"
#include <algorithm>
#include <cstring>
#include <zlib.h>

using std::string;

namespace openmsx {

class RamDebuggable final : public SimpleDebuggable
{
public:
	RamDebuggable(MSXMotherBoard& motherBoard, const string& name,
	              const string& description, Ram& ram);
	byte read(unsigned address) override;
	void write(unsigned address, byte value) override;
private:
	Ram& ram;
};


Ram::Ram(const DeviceConfig& config, const string& name,
         const string& description, unsigned size_)
	: xml(*config.getXML())
	, ram(size_)
	, size(size_)
	, debuggable(make_unique<RamDebuggable>(
		config.getMotherBoard(), name, description, *this))
{
	clear();
}

Ram::Ram(const DeviceConfig& config, unsigned size_)
	: xml(*config.getXML())
	, ram(size_)
	, size(size_)
{
	clear();
}

Ram::~Ram()
{
}

void Ram::clear(byte c)
{
	if (const XMLElement* init = xml.findChild("initialContent")) {
		// get pattern (and decode)
		const string& encoding = init->getAttribute("encoding");
		size_t done = 0;
		if (encoding == "gz-base64") {
			auto p = Base64::decode(init->getData());
			uLongf dstLen = getSize();
			if (uncompress(reinterpret_cast<Bytef*>(ram.data()), &dstLen,
			               reinterpret_cast<const Bytef*>(p.first.data()), uLong(p.second))
			     != Z_OK) {
				throw MSXException("Error while decompressing initialContent.");
			}
			done = dstLen;
		} else if ((encoding == "hex") || (encoding == "base64")) {
			auto p = (encoding == "hex")
			       ? HexDump::decode(init->getData())
			       : Base64 ::decode(init->getData());
			done = std::min(size_t(size), p.second);
			memcpy(ram.data(), p.first.data(), done);
		} else {
			throw MSXException("Unsupported encoding \"" + encoding + "\" for initialContent");
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

const string& Ram::getName() const
{
	return debuggable->getName();
}

RamDebuggable::RamDebuggable(MSXMotherBoard& motherBoard_,
                             const string& name_,
                             const string& description_, Ram& ram_)
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


template<typename Archive>
void Ram::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize_blob("ram", ram.data(), size);
}
INSTANTIATE_SERIALIZE_METHODS(Ram);

} // namespace openmsx
