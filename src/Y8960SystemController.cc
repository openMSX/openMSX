#include "Y8960SystemController.hh"

#include "MSXMotherBoard.hh"
#include "serialize.hh"

namespace openmsx {

static constexpr uint8_t ID = 0x80;           // 40h: enable key
static constexpr uint8_t BOARD_ID = 0x02;     // 41h: Y8960 cartridge
static constexpr uint8_t FUNCTION_ROM = 0x01; // 42h: Config ROM reader/writer

Y8960SystemController::Y8960SystemController(const DeviceConfig& config)
	: MSXDevice(config)
	, MSXSwitchedDevice(getMotherBoard(), ID)
{
	reset(EmuTime::dummy());
}

void Y8960SystemController::reset(EmuTime /*time*/)
{
	boardSelected = false;
	romFunction = false;
}

uint8_t Y8960SystemController::readSwitchedIO(uint16_t port, EmuTime time)
{
	return peekSwitchedIO(port, time);
}

uint8_t Y8960SystemController::peekSwitchedIO(uint16_t port, EmuTime /*time*/) const
{
	switch (port & 0x0F) {
	case 0x00:
		return uint8_t(~ID);
	case 0x01:
		return boardSelected ? uint8_t(~BOARD_ID) : 0xFF;
	case 0x02:
		return (boardSelected && romFunction) ? uint8_t(~FUNCTION_ROM) : 0xFF;
	default:
		return 0xFF;
	}
}

void Y8960SystemController::writeSwitchedIO(uint16_t port, uint8_t value, EmuTime /*time*/)
{
	switch (port & 0x0F) {
	case 0x01:
		boardSelected = value == BOARD_ID;
		if (!boardSelected) romFunction = false;
		break;
	case 0x02:
		if (boardSelected) romFunction = value == FUNCTION_ROM;
		break;
	default:
		break;
	}
}

template<typename Archive>
void Y8960SystemController::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("boardSelected", boardSelected,
	             "romFunction",   romFunction);
}
INSTANTIATE_SERIALIZE_METHODS(Y8960SystemController);
REGISTER_MSXDEVICE(Y8960SystemController, "Y8960-SYSTEM-CONTROLLER");

} // namespace openmsx
