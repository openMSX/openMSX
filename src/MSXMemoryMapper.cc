// $Id$

#include "MSXMemoryMapper.hh"
#include "MSXMapperIO.hh"
#include "MSXMapperIOTurboR.hh"
#include "MSXMapperIOPhilips.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "CPU.hh"

// Inlined methods first, to make sure they are actually inlined:

inline int MSXMemoryMapper::calcAddress(word address)
{
	// TODO: Keep pageAddr per mapper and apply mask on reg write.
	return (pageAddr[address>>14] & sizeMask) | (address & 0x3FFF);
}

// Constructor and destructor:

MSXMemoryMapper::MSXMemoryMapper(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXMemoryMapper object");

	initIO();

	slowDrainOnReset = deviceConfig->getParameterAsBool("slow_drain_on_reset");
	int kSize = deviceConfig->getParameterAsInt("size");
	if (kSize % 16 != 0) {
		PRT_ERROR("Mapper size is not a multiple of 16K: " << kSize);
	}
	int blocks = kSize/16;
	size = 16384 * blocks;
	sizeMask = size - 1; // Both convenient and correct!
	if (!(buffer = new byte[size]))
		PRT_ERROR("Couldn't allocate memory for " << getName());
	//Isn't completely true, but let's suppose that ram will
	//always contain all zero if started
	memset(buffer, 0, size);

	device->registerMapper(blocks);
}

MSXMemoryMapper::~MSXMemoryMapper()
{
	PRT_DEBUG("Destructing an MSXMemoryMapper object");
	delete [] buffer; // C++ can handle null-pointers
}

void MSXMemoryMapper::reset(const EmuTime &time)
{
	MSXDevice::reset(time);
	resetIO();
	if (!slowDrainOnReset) {
		PRT_DEBUG("Clearing ram of " << getName());
		memset(buffer, 0, size);
	}
}

byte MSXMemoryMapper::readMem(word address, const EmuTime &time)
{
	return buffer[calcAddress(address)];
}

void MSXMemoryMapper::writeMem(word address, byte value, const EmuTime &time)
{
	buffer[calcAddress(address)] = value;
}

byte* MSXMemoryMapper::getReadCacheLine(word start)
{
	return &buffer[calcAddress(start)];
}

byte* MSXMemoryMapper::getWriteCacheLine(word start)
{
	return &buffer[calcAddress(start)];
}



void MSXMemoryMapper::initIO()
{
	if (device == NULL) {
		// Create specified IO behaviour
		MSXConfig::Config* config = MSXConfig::instance()->getConfigById("MapperIO");
		std::string type = config->getParameter("type");
		if (type == "TurboR") {
			device = new MSXMapperIOTurboR();
		} else if (type == "Philips") {
			device = new MSXMapperIOPhilips();
		} else {
			PRT_ERROR("Unknown mapper type");
		}

		// Register I/O ports FC..FF
		// TODO: Register MSXMapperIO instead?
		MSXMotherBoard::instance()->register_IO_In (0xFC,this);
		MSXMotherBoard::instance()->register_IO_In (0xFD,this);
		MSXMotherBoard::instance()->register_IO_In (0xFE,this);
		MSXMotherBoard::instance()->register_IO_In (0xFF,this);
		MSXMotherBoard::instance()->register_IO_Out(0xFC,this);
		MSXMotherBoard::instance()->register_IO_Out(0xFD,this);
		MSXMotherBoard::instance()->register_IO_Out(0xFE,this);
		MSXMotherBoard::instance()->register_IO_Out(0xFF,this);

		resetIO();
	}
}
MSXMapperIO *MSXMemoryMapper::device = NULL;

void MSXMemoryMapper::resetIO()
{
	//TODO mapper is initialized like this by BIOS,
	// but in what state is it after reset?
	// Zeroed is most likely.
	// To find out for real, insert an external memory mapper on an MSX1.
	for (int i = 0; i < 4; i++) pageAddr[i] = 0;
}

byte MSXMemoryMapper::readIO(byte port, const EmuTime &time)
{
	assert(0xfc <= port);
	return device->convert(pageAddr[port-0xfc] >> 14);
}

void MSXMemoryMapper::writeIO(byte port, byte value, const EmuTime &time)
{
	assert (0xfc <= port);
	pageAddr[port-0xfc] = value << 14;
	MSXCPU::instance()->invalidateCache(0x4000*(port-0xfc), 0x4000/CPU::CACHE_LINE_SIZE);
}

int MSXMemoryMapper::pageAddr[4];

