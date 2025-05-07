#ifndef MSXMAPPERIO_HH
#define MSXMAPPERIO_HH

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "SimpleDebuggable.hh"

#include <array>
#include <cstdint>
#include <vector>

namespace openmsx {

struct MSXMemoryMapperInterface
{
	[[nodiscard]] virtual byte readIO(word port, EmuTime::param time) = 0;
	[[nodiscard]] virtual byte peekIO(word port, EmuTime::param time) const = 0;
	virtual void writeIO(word port, byte value, EmuTime::param time) = 0;
	[[nodiscard]] virtual byte getSelectedSegment(byte page) const = 0;
protected:
	~MSXMemoryMapperInterface() = default;
};


class MSXMapperIO final : public MSXDevice
{
public:
	enum class Mode : uint8_t { INTERNAL, EXTERNAL };

public:
	explicit MSXMapperIO(const DeviceConfig& config);

	void setMode(Mode mode, byte mask, byte baseValue);

	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	void registerMapper(MSXMemoryMapperInterface* mapper);
	void unregisterMapper(MSXMemoryMapperInterface* mapper);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] byte read(unsigned address) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
		void readBlock(unsigned start, std::span<byte> output) override;
	} debuggable;

	std::vector<MSXMemoryMapperInterface*> mappers;

	std::array<byte, 4> registers = {}; // (copy of) the mapper register state
	byte mask; // bitmask: 1-bit -> take mapper register, 0-bit -> take baseValue
	byte baseValue = 0xff;
	Mode mode = Mode::EXTERNAL; // use the internal or the external mapper state
};
SERIALIZE_CLASS_VERSION(MSXMapperIO, 3);


class MSXMapperIOClient : public MSXMemoryMapperInterface
{
public:
	MSXMapperIOClient(const MSXMapperIOClient&) = delete;
	MSXMapperIOClient(MSXMapperIOClient&&) = delete;
	MSXMapperIOClient& operator=(const MSXMapperIOClient&) = delete;
	MSXMapperIOClient& operator=(MSXMapperIOClient&&) = delete;

protected:
	explicit MSXMapperIOClient(MSXMotherBoard& motherBoard_)
		: motherBoard(motherBoard_)
	{
		auto& mapperIO = motherBoard.createMapperIO();
		mapperIO.registerMapper(this);
	}

	~MSXMapperIOClient()
	{
		auto& mapperIO = motherBoard.getMapperIO();
		mapperIO.unregisterMapper(this);
		motherBoard.destroyMapperIO();
	}

private:
	MSXMotherBoard& motherBoard;
};

} // namespace openmsx

#endif
