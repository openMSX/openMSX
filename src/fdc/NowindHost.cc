#include "NowindHost.hh"
#include "DiskContainer.hh"
#include "FileOperations.hh"
#include "MSXException.hh"
#include "SectorAccessibleDisk.hh"
#include "enumerate.hh"
#include "one_of.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <fstream>
#include <memory>

using std::string;

namespace openmsx {

static constexpr unsigned SECTOR_SIZE = sizeof(SectorBuffer);

static void DBERR(const char* message, ...)
{
	(void)message;
#if 0
	va_list args;
	va_start(args, message);
	printf("nowind: ");
	vprintf(message, args);
	va_end(args);
#endif
}


NowindHost::NowindHost(const Drives& drives_)
	: drives(drives_)
	, lastTime(0)
	, state(STATE_SYNC1)
	, romdisk(255)
	, allowOtherDiskroms(false)
	, enablePhantomDrives(true)
{
}

NowindHost::~NowindHost() = default;

byte NowindHost::peek() const
{
	return isDataAvailable() ? hostToMsxFifo.front() : 0xFF;
}

// receive:  msx <- pc
byte NowindHost::read()
{
	return isDataAvailable() ? hostToMsxFifo.pop_front() : 0xFF;
}

bool NowindHost::isDataAvailable() const
{
	return !hostToMsxFifo.empty();
}


// send:  msx -> pc
void NowindHost::write(byte data, unsigned time)
{
	unsigned duration = time - lastTime;
	lastTime = time;
	if (duration >= 500) {
		// timeout (500ms), start looking for AF05
		purge();
		state = STATE_SYNC1;
	}

	switch (state) {
	case STATE_SYNC1:
		if (data == 0xAF) state = STATE_SYNC2;
		break;
	case STATE_SYNC2:
		switch (data) {
		case 0x05: state = STATE_COMMAND; recvCount = 0; break;
		case 0xAF: state = STATE_SYNC2; break;
		case 0xFF: state = STATE_SYNC1; msxReset(); break;
		default:   state = STATE_SYNC1; break;
		}
		break;
	case STATE_COMMAND:
		assert(recvCount < 9);
		cmdData[recvCount] = data;
		if (++recvCount == 9) {
			executeCommand();
		}
		break;
	case STATE_DISKREAD:
		assert(recvCount < 2);
		extraData[recvCount] = data;
		if (++recvCount == 2) {
			doDiskRead2();
		}
		break;
	case STATE_DISKWRITE:
		assert(recvCount < (transferSize + 2));
		extraData[recvCount] = data;
		if (++recvCount == (transferSize + 2)) {
			doDiskWrite2();
		}
		break;
	case STATE_DEVOPEN:
		assert(recvCount < 11);
		extraData[recvCount] = data;
		if (++recvCount == 11) {
			deviceOpen();
		}
		break;
	case STATE_IMAGE:
		assert(recvCount < 40);
		extraData[recvCount] = data;
		if (data == one_of(0, ':') ||
		    (++recvCount == 40)) {
			char* eData = reinterpret_cast<char*>(extraData);
			callImage(string(eData, recvCount));
			state = STATE_SYNC1;
		}
		break;
	case STATE_MESSAGE:
		assert(recvCount < (240 - 1));
		extraData[recvCount] = data;
		if ((data == 0) || (++recvCount == (240 - 1))) {
			extraData[recvCount] = 0;
			DBERR("%s\n", reinterpret_cast<char*>(extraData));
			state = STATE_SYNC1;
		}
		break;
	default:
		UNREACHABLE;
	}
}

void NowindHost::msxReset()
{
	for (auto& dev : devices) {
		dev.fs.reset();
	}
	DBERR("MSX reset\n");
}

SectorAccessibleDisk* NowindHost::getDisk() const
{
	byte num = cmdData[7]; // reg_a
	if (num >= drives.size()) {
		return nullptr;
	}
	return drives[num]->getSectorAccessibleDisk();
}

void NowindHost::executeCommand()
{
	assert(recvCount == 9);
	byte cmd = cmdData[8];
	switch (cmd) {
	//case 0x0D: BDOS_0DH_DiskReset();
	//case 0x0F: BDOS_0FH_OpenFile();
	//case 0x10: BDOS_10H_CloseFile();
	//case 0x11: BDOS_11H_FindFirst();
	//case 0x12: BDOS_12H_FindNext();
	//case 0x13: BDOS_13H_DeleteFile();
	//case 0x14: BDOS_14H_ReadSeq();
	//case 0x15: BDOS_15H_WriteSeq();
	//case 0x16: BDOS_16H_CreateFile();
	//case 0x17: BDOS_17H_RenameFile();
	//case 0x21: BDOS_21H_ReadRandomFile();
	//case 0x22: BDOS_22H_WriteRandomFile();
	//case 0x23: BDOS_23H_GetFileSize();
	//case 0x24: BDOS_24H_SetRandomRecordField();
	//case 0x26: BDOS_26H_WriteRandomBlock();
	//case 0x27: BDOS_27H_ReadRandomBlock();
	//case 0x28: BDOS_28H_WriteRandomFileWithZeros();
	//case 0x2A: BDOS_2AH_GetDate();
	//case 0x2B: BDOS_2BH_SetDate();
	//case 0x2C: BDOS_2CH_GetTime();
	//case 0x2D: BDOS_2DH_SetTime();
	//case 0x2E: BDOS_2EH_Verify();
	//case 0x2F: BDOS_2FH_ReadLogicalSector();
	//case 0x30: BDOS_30H_WriteLogicalSector();

	case 0x80: { // DSKIO
		auto* disk = getDisk();
		if (!disk) {
			// no such drive or no disk inserted
			// (causes a timeout on the MSX side)
			state = STATE_SYNC1;
			return;
		}
		byte reg_f = cmdData[6];
		if (reg_f & 1) { // carry flag
			diskWriteInit(*disk);
		} else {
			diskReadInit(*disk);
		}
		break;
	}

	case 0x81: DSKCHG();      state = STATE_SYNC1; break;
	//case 0x82: GETDPB();
	//case 0x83: CHOICE();
	//case 0x84: DSKFMT();
	case 0x85: DRIVES();      state = STATE_SYNC1; break;
	case 0x86: INIENV();      state = STATE_SYNC1; break;
	case 0x87: setDateMSX();  state = STATE_SYNC1; break;
	case 0x88: state = STATE_DEVOPEN; recvCount = 0; break;
	case 0x89: deviceClose(); state = STATE_SYNC1; break;
	//case 0x8A: deviceRandomIO(fcb);
	case 0x8B: deviceWrite(); state = STATE_SYNC1; break;
	case 0x8C: deviceRead();  state = STATE_SYNC1; break;
	//case 0x8D: deviceEof(fcb);
	//case 0x8E: auxIn();
	//case 0x8F: auxOut();
	case 0x90: state = STATE_MESSAGE; recvCount = 0; break;
	case 0xA0: state = STATE_IMAGE;   recvCount = 0; break;
	//case 0xFF: vramDump();
	default:
		// Unknown USB command!
		state = STATE_SYNC1;
		break;
	}
}

// send:  pc -> msx
void NowindHost::send(byte value)
{
	hostToMsxFifo.push_back(value);
}
void NowindHost::send16(word value)
{
	hostToMsxFifo.push_back(value & 255);
	hostToMsxFifo.push_back(value >> 8);
}

void NowindHost::purge()
{
	hostToMsxFifo.clear();
}

void NowindHost::sendHeader()
{
	send(0xFF); // needed because first read might fail!
	send(0xAF);
	send(0x05);
}

void NowindHost::DSKCHG()
{
	auto* disk = getDisk();
	if (!disk) {
		// no such drive or no disk inserted
		return;
	}

	sendHeader();
	byte num = cmdData[7]; // reg_a
	assert(num < drives.size());
	if (drives[num]->diskChanged()) {
		send(255); // changed
		// read first FAT sector (contains media descriptor)
		SectorBuffer sectorBuffer;
		try {
			disk->readSectors(&sectorBuffer, 1, 1);
		} catch (MSXException&) {
			// TODO read error
			sectorBuffer.raw[0] = 0;
		}
		send(sectorBuffer.raw[0]); // new mediadescriptor
	} else {
		send(0); // not changed
		// TODO shouldn't we send some (dummy) byte here?
		//      nowind-diskrom seems to read it (but doesn't use it)
	}
}

void NowindHost::DRIVES()
{
	// at least one drive (MSX-DOS1 cannot handle 0 drives)
	byte numberOfDrives = std::max<byte>(1, byte(drives.size()));

	byte reg_a = cmdData[7];
	sendHeader();
	send(getEnablePhantomDrives() ? 0x02 : 0);
	send(reg_a | (getAllowOtherDiskroms() ? 0 : 0x80));
	send(numberOfDrives);

	romdisk = 255; // no romdisk
	for (auto [i, drv] : enumerate(drives)) {
		if (drv->isRomdisk()) {
			romdisk = byte(i);
			break;
		}
	}
}

void NowindHost::INIENV()
{
	sendHeader();
	send(romdisk); // calculated in DRIVES()
}

void NowindHost::setDateMSX()
{
	auto td = time(nullptr);
	auto* tm = localtime(&td);

	sendHeader();
	send(tm->tm_mday);          // day
	send(tm->tm_mon + 1);       // month
	send16(tm->tm_year + 1900); // year
}


unsigned NowindHost::getSectorAmount() const
{
	byte reg_b = cmdData[1];
	return reg_b;
}
unsigned NowindHost::getStartSector() const
{
	byte reg_c = cmdData[0];
	byte reg_e = cmdData[2];
	byte reg_d = cmdData[3];
	unsigned startSector = reg_e + (reg_d * 256);
	if (reg_c < 0x80) {
		// FAT16 read/write sector
		startSector += reg_c << 16;
	}
	return startSector;
}
unsigned NowindHost::getStartAddress() const
{
	byte reg_l = cmdData[4];
	byte reg_h = cmdData[5];
	return reg_h * 256 + reg_l;
}
unsigned NowindHost::getCurrentAddress() const
{
	unsigned startAddress = getStartAddress();
	return startAddress + transferred;
}


void NowindHost::diskReadInit(SectorAccessibleDisk& disk)
{
	unsigned sectorAmount = getSectorAmount();
	buffer.resize(sectorAmount);
	unsigned startSector = getStartSector();
	try {
		disk.readSectors(buffer.data(), startSector, sectorAmount);
	} catch (MSXException&) {
		// read error
		state = STATE_SYNC1;
		return;
	}

	transferred = 0;
	retryCount = 0;
	doDiskRead1();
}

void NowindHost::doDiskRead1()
{
	unsigned bytesLeft = unsigned(buffer.size() * SECTOR_SIZE) - transferred;
	if (bytesLeft == 0) {
		sendHeader();
		send(0x01); // end of receive-loop
		send(0x00); // no more data
		state = STATE_SYNC1;
		return;
	}

	constexpr unsigned NUMBEROFBLOCKS = 32; // 32 * 64 bytes = 2048 bytes
	transferSize = std::min(bytesLeft, NUMBEROFBLOCKS * 64); // hardcoded in firmware

	unsigned address = getCurrentAddress();
	if (address >= 0x8000) {
		if (transferSize & 0x003F) {
			transferSectors(address, transferSize);
		} else {
			transferSectorsBackwards(address, transferSize);
		}
	} else {
		// transfer below 0x8000
		// TODO shouldn't we also test for (transferSize & 0x3F)?
		unsigned endAddress = address + transferSize;
		if (endAddress <= 0x8000) {
			transferSectorsBackwards(address, transferSize);
		} else {
			transferSize = 0x8000 - address;
			transferSectors(address, transferSize);
		}
	}

	// wait for 2 bytes
	state = STATE_DISKREAD;
	recvCount = 0;
}

void NowindHost::doDiskRead2()
{
	// diskrom sends back the last two bytes read
	assert(recvCount == 2);
	byte tail1 = extraData[0];
	byte tail2 = extraData[1];
	if ((tail1 == 0xAF) && (tail2 == 0x07)) {
		transferred += transferSize;
		retryCount = 0;

		unsigned address = getCurrentAddress();
		size_t bytesLeft = (buffer.size() * SECTOR_SIZE) - transferred;
		if ((address == 0x8000) && (bytesLeft > 0)) {
			sendHeader();
			send(0x01); // end of receive-loop
			send(0xff); // more data for page 2/3
		}

		// continue the rest of the disk read
		doDiskRead1();
	} else {
		purge();
		if (++retryCount == 10) {
			// do nothing, timeout on MSX
			// too many retries, aborting readDisk()
			state = STATE_SYNC1;
			return;
		}

		// try again, wait for two bytes
		state = STATE_DISKREAD;
		recvCount = 0;
	}
}

// sends "02" + "transfer_addr" + "amount" + "data" + "0F 07"
void NowindHost::transferSectors(unsigned transferAddress, unsigned amount)
{
	sendHeader();
	send(0x00); // don't exit command, (more) data is coming
	send16(transferAddress);
	send16(amount);

	auto* bufferPointer = &buffer[0].raw[transferred];
	for (auto i : xrange(amount)) {
		send(bufferPointer[i]);
	}
	send(0xAF);
	send(0x07); // used for validation
}

// sends "02" + "transfer_addr" + "amount" + "data" + "0F 07"
void NowindHost::transferSectorsBackwards(unsigned transferAddress, unsigned amount)
{
	sendHeader();
	send(0x02); // don't exit command, (more) data is coming
	send16(transferAddress + amount);
	send(amount / 64);

	auto* bufferPointer = &buffer[0].raw[transferred];
	for (int i = amount - 1; i >= 0; --i) {
		send(bufferPointer[i]);
	}
	send(0xAF);
	send(0x07); // used for validation
}


void NowindHost::diskWriteInit(SectorAccessibleDisk& disk)
{
	if (disk.isWriteProtected()) {
		sendHeader();
		send(1);
		send(0); // WRITEPROTECTED
		state = STATE_SYNC1;
		return;
	}

	unsigned sectorAmount = std::min(128u, getSectorAmount());
	buffer.resize(sectorAmount);
	transferred = 0;
	doDiskWrite1();
}

void NowindHost::doDiskWrite1()
{
	unsigned bytesLeft = unsigned(buffer.size() * SECTOR_SIZE) - transferred;
	if (bytesLeft == 0) {
		// All data transferred!
		auto sectorAmount = unsigned(buffer.size());
		unsigned startSector = getStartSector();
		if (auto* disk = getDisk()) {
			try {
				disk->writeSectors(&buffer[0], startSector, sectorAmount);
			} catch (MSXException&) {
				// TODO write error
			}
		}
		sendHeader();
		send(255);
		state = STATE_SYNC1;
		return;
	}

	constexpr unsigned BLOCKSIZE = 240;
	transferSize = std::min(bytesLeft, BLOCKSIZE);

	unsigned address = getCurrentAddress();
	unsigned endAddress = address + transferSize;
	if ((address ^ endAddress) & 0x8000) {
		// would cross page 1-2 boundary -> limit to page 1
		transferSize = 0x8000 - address;
	}

	sendHeader();
	send(0);          // data ahead!
	send16(address);
	send16(transferSize);
	send(0xaa);

	// wait for data
	state = STATE_DISKWRITE;
	recvCount = 0;
}

void NowindHost::doDiskWrite2()
{
	assert(recvCount == (transferSize + 2));
	auto* buf = &buffer[0].raw[transferred];
	for (auto i : xrange(transferSize)) {
		buf[i] = extraData[i + 1];
	}

	byte seq1 = extraData[0];
	byte seq2 = extraData[transferSize + 1];
	if ((seq1 == 0xaa) && (seq2 == 0xaa)) {
		// good block received
		transferred += transferSize;

		unsigned address = getCurrentAddress();
		size_t bytesLeft = (buffer.size() * SECTOR_SIZE) - transferred;
		if ((address == 0x8000) && (bytesLeft > 0)) {
			sendHeader();
			send(254); // more data for page 2/3
		}
	} else {
		// ERROR!!!
		// This situation is still not handled correctly!
		purge();
	}

	// continue the rest of the disk write
	doDiskWrite1();
}


unsigned NowindHost::getFCB() const
{
	// note: same code as getStartAddress(), merge???
	byte reg_l = cmdData[4];
	byte reg_h = cmdData[5];
	return reg_h * 256 + reg_l;
}

string NowindHost::extractName(int begin, int end) const
{
	string result;
	for (auto i : xrange(begin, end)) {
		char c = extraData[i];
		if (c == ' ') break;
		result += char(toupper(c));
	}
	return result;
}

int NowindHost::getDeviceNum() const
{
	unsigned fcb = getFCB();
	for (auto [i, dev] : enumerate(devices)) {
		if (dev.fs && dev.fcb == fcb) {
			return int(i);
		}
	}
	return -1;
}

int NowindHost::getFreeDeviceNum()
{
	if (int dev = getDeviceNum(); dev != -1) {
		// There already was a device open with this fcb address,
		// reuse that device.
		return dev;
	}
	// Search for free device.
	for (auto [i, dev] : enumerate(devices)) {
		if (!dev.fs) return int(i);
	}
	// All devices are in use. This can't happen when the MSX software
	// functions correctly. We'll simply reuse the first device. It would
	// be nicer if we reuse the oldest device, but that's harder to
	// implement, and actually it doesn't really matter.
	return 0;
}

void NowindHost::deviceOpen()
{
	state = STATE_SYNC1;

	assert(recvCount == 11);
	string filename = extractName(0, 8);
	string ext      = extractName(8, 11);
	if (!ext.empty()) {
		strAppend(filename, '.', ext);
	}

	unsigned fcb = getFCB();
	unsigned dev = getFreeDeviceNum();
	devices[dev].fs.emplace(); // takes care of deleting old fs
	devices[dev].fcb = fcb;

	sendHeader();
	byte errorCode = 0;
	byte openMode = cmdData[2]; // reg_e
	switch (openMode) {
	case 1: // read-only mode
		devices[dev].fs->open(filename.c_str(), std::ios::in  | std::ios::binary);
		errorCode = 53; // file not found
		break;
	case 2: // create new file, write-only
		devices[dev].fs->open(filename.c_str(), std::ios::out | std::ios::binary);
		errorCode = 56; // bad file name
		break;
	case 8: // append to existing file, write-only
		devices[dev].fs->open(filename.c_str(), std::ios::out | std::ios::binary | std::ios::app);
		errorCode = 53; // file not found
		break;
	case 4:
		send(58); // sequential I/O only
		return;
	default:
		send(0xFF); // TODO figure out a good error number
		return;
	}
	assert(errorCode != 0);
	if (devices[dev].fs->fail()) {
		devices[dev].fs.reset();
		send(errorCode);
		return;
	}

	unsigned readLen = 0;
	bool eof = false;
	char buf[256];
	if (openMode == 1) {
		// read-only mode, already buffer first 256 bytes
		readLen = readHelper1(dev, buf);
		assert(readLen <= 256);
		eof = readLen < 256;
	}

	send(0x00); // no error
	send16(fcb);
	send16(9 + readLen + (eof ? 1 : 0)); // number of bytes to transfer

	send(openMode);
	send(0);
	send(0);
	send(0);
	send(cmdData[3]); // reg_d
	send(0);
	send(0);
	send(0);
	send(0);

	if (openMode == 1) {
		readHelper2(readLen, buf);
	}
}

void NowindHost::deviceClose()
{
	int dev = getDeviceNum();
	if (dev == -1) return;
	devices[dev].fs.reset();
}

void NowindHost::deviceWrite()
{
	int dev = getDeviceNum();
	if (dev == -1) return;
	char data = cmdData[0]; // reg_c
	devices[dev].fs->write(&data, 1);
}

void NowindHost::deviceRead()
{
	int dev = getDeviceNum();
	if (dev == -1) return;

	char buf[256];
	unsigned readLen = readHelper1(dev, buf);
	bool eof = readLen < 256;
	send(0xAF);
	send(0x05);
	send(0x00); // dummy
	send16(getFCB() + 9);
	send16(readLen + (eof ? 1 : 0));
	readHelper2(readLen, buf);
}

unsigned NowindHost::readHelper1(unsigned dev, char* buf)
{
	assert(dev < MAX_DEVICES);
	unsigned len = 0;
	for (/**/; len < 256; ++len) {
		devices[dev].fs->read(&buf[len], 1);
		if (devices[dev].fs->eof()) break;
	}
	return len;
}

void NowindHost::readHelper2(unsigned len, const char* buf)
{
	for (auto i : xrange(len)) {
		send(buf[i]);
	}
	if (len < 256) {
		send(0x1A); // end-of-file
	}
}


// strips a string from outer double-quotes and anything outside them
// ie: 'pre("foo")bar' will result in 'foo'
static constexpr std::string_view stripquotes(std::string_view str)
{
	auto first = str.find_first_of('\"');
	if (first == string::npos) {
		// There are no quotes, return the whole string.
		return str;
	}
	auto last = str.find_last_of ('\"');
	if (first == last) {
		// Error, there's only a single double-quote char.
		return {};
	}
	// Return the part between the quotes.
	return str.substr(first + 1, last - first - 1);
}

void NowindHost::callImage(const string& filename)
{
	byte num = cmdData[7]; // reg_a
	if (num >= drives.size()) {
		// invalid drive number
		return;
	}
	if (drives[num]->insertDisk(FileOperations::expandTilde(string(stripquotes(filename))))) {
		// TODO error handling
	}
}


static constexpr std::initializer_list<enum_string<NowindHost::State>> stateInfo = {
	{ "SYNC1",     NowindHost::STATE_SYNC1     },
	{ "SYNC2",     NowindHost::STATE_SYNC2     },
	{ "COMMAND",   NowindHost::STATE_COMMAND   },
	{ "DISKREAD",  NowindHost::STATE_DISKREAD  },
	{ "DISKWRITE", NowindHost::STATE_DISKWRITE },
	{ "DEVOPEN",   NowindHost::STATE_DEVOPEN   },
	{ "IMAGE",     NowindHost::STATE_IMAGE     },
};
SERIALIZE_ENUM(NowindHost::State, stateInfo);

template<typename Archive>
void NowindHost::serialize(Archive& ar, unsigned /*version*/)
{
	// drives is serialized elsewhere

	ar.serialize("hostToMsxFifo", hostToMsxFifo,
	             "lastTime",      lastTime,
	             "state",         state,
	             "recvCount",     recvCount,
	             "cmdData",       cmdData,
	             "extraData",     extraData);

	// for backwards compatibility, serialize buffer as a vector<byte>
	size_t bufSize = buffer.size() * sizeof(SectorBuffer);
	byte* bufRaw = buffer.data()->raw.data();
	std::vector<byte> tmp(bufRaw, bufRaw + bufSize);
	ar.serialize("buffer", tmp);
	ranges::copy(tmp, bufRaw);

	ar.serialize("transfered",          transferred, // for bw compat, keep typo in serialize name
	             "retryCount",          retryCount,
	             "transferSize",        transferSize,
	             "romdisk",             romdisk,
	             "allowOtherDiskroms",  allowOtherDiskroms,
	             "enablePhantomDrives", enablePhantomDrives);

	// Note: We don't serialize 'devices'. So after a loadstate it will be
	// as-if the devices are closed again. The reason for not serializing
	// this is that it's very hard to serialize a fstream (and we anyway
	// can't restore the state of the host filesystem).
}
INSTANTIATE_SERIALIZE_METHODS(NowindHost);

} // namspace openmsx
