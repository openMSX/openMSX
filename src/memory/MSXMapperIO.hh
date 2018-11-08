#ifndef MSXMAPPERIO_HH
#define MSXMAPPERIO_HH

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "SimpleDebuggable.hh"
#include <vector>

namespace openmsx {

struct MSXMemoryMapperInterface
{
	virtual byte readIO(word port, EmuTime::param time) = 0;
	virtual byte peekIO(word port, EmuTime::param time) const = 0;
	virtual void writeIO(word port, byte value, EmuTime::param time) = 0;
	virtual byte getSelectedSegment(byte page) const = 0;
protected:
	~MSXMemoryMapperInterface() = default;
};


class MSXMapperIO final : public MSXDevice
{
public:
	explicit MSXMapperIO(const DeviceConfig& config);

	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	void registerMapper(MSXMemoryMapperInterface* mapper);
	void unregisterMapper(MSXMemoryMapperInterface* mapper);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		byte read(unsigned address) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} debuggable;

	std::vector<MSXMemoryMapperInterface*> mappers;

	/**
	 * OR-mask that limits which bits can be read back.
	 * This is set using the MapperReadBackBits tag in the machine config.
	 */
	byte mask;
};
SERIALIZE_CLASS_VERSION(MSXMapperIO, 2);


class MSXMapperIOClient : public MSXMemoryMapperInterface
{
public:
	MSXMapperIOClient(MSXMotherBoard& motherBoard_)
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
