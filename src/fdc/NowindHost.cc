#include "NowindHost.hh"
#include "DiskChanger.hh"
#include "SectorAccessibleDisk.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include <fstream>
#include <algorithm>
#include <ctype.h>
#include <time.h>

using std::string;
using std::fstream;
using std::ios;

namespace openmsx {

NowindHost::NowindHost(DiskChanger& changer_)
	: changer(changer_)
	, lastTime(EmuTime::zero)
{
}

NowindHost::~NowindHost()
{
}

// receive:  msx <- pc
byte NowindHost::read()
{
	if (hostToMsxFifo.empty()) {
		return 0xff;
	}
	byte result = hostToMsxFifo.front();
	hostToMsxFifo.pop_front();
	return result;
}

// send:  msx -> pc
void NowindHost::write(byte data, EmuTime::param time)
{
	EmuDuration dur = time - lastTime;
	lastTime = time;
	static const EmuDuration TIMEOUT = EmuDuration(0.5);
	if (dur >= TIMEOUT) {
		// timeout, start looking for AF05
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
		if ((data == 0) || (data == ':') ||
		    (++recvCount == 40)) {
			char* data = reinterpret_cast<char*>(extraData);
			callImage(string(data, recvCount));
			state = STATE_SYNC1;
		}
		break;
	default:
		assert(false);
	}
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
		byte reg_a = cmdData[7];
		if (reg_a != 0) {
			// invalid drive number
			state = STATE_SYNC1;
			return;
		}
		SectorAccessibleDisk* disk = changer.getSectorAccessibleDisk();
		if (!disk) {
			// no disk inserted (causes a timeout)
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
	case 0xA0: state = STATE_IMAGE; recvCount = 0; break;
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
	byte reg_a = cmdData[7];
	if (reg_a != 0) {
		// invalid drive number
		return;
	}
	SectorAccessibleDisk* disk = changer.getSectorAccessibleDisk();
	if (!disk) {
		// no disk inserted (causes a timeout)
		return;
	}

	sendHeader();
	if (changer.diskChanged()) {
		send(255); // changed
		byte sectorBuffer[512];
		try {
			disk->readSector(1, sectorBuffer); // read first FAT
		} catch (MSXException& e) {
			// TODO
			sectorBuffer[0] = 0;
		}
		send(sectorBuffer[0]); // new mediadescriptor
	} else {
		send(0); // not changed
		// TODO shouldn't we send some (dummy) byte here?
		//      nowind-diskrom seems to read it (but doesn't use it)
	}
}

void NowindHost::DRIVES()
{
	byte numberOfDrives = 1; // at least one drive (MSXDOS1 cannot handle 0 drives)
	bool bootWithShift = false;
	bool bootWithCtrl = false;

	byte reg_a = cmdData[7];
	sendHeader();
	send(bootWithCtrl ? 0 : 0x02);
	send(reg_a | (bootWithShift ? 0x80 : 0));
	send(numberOfDrives);
}

void NowindHost::INIENV()
{
	sendHeader();
	send(255); // no romdisk
}

void NowindHost::setDateMSX()
{
	time_t td = time(NULL);
	struct tm* tm = localtime(&td);

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
	unsigned startAdress = getStartAddress();
	return startAdress + transfered;
}


void NowindHost::diskReadInit(SectorAccessibleDisk& disk)
{
	unsigned sectorAmount = getSectorAmount();
	buffer.resize(sectorAmount * 512);
	unsigned startSector = getStartSector();
	try {
		for (unsigned i = 0; i < sectorAmount; ++i) {
			disk.readSector(startSector + i, &buffer[512 * i]);
		}
	} catch (MSXException& e) {
		// TODO
	}

	transfered = 0;
	retryCount = 0;
	doDiskRead1();
}

void NowindHost::doDiskRead1()
{
	unsigned bytesLeft = buffer.size() - transfered;
	if (bytesLeft == 0) {
		sendHeader();
		send(0x01); // end of receive-loop
		send(0x00); // no more data
		state = STATE_SYNC1;
		return;
	}

	static const unsigned NUMBEROFBLOCKS = 32; // 32 * 64 bytes = 2048 bytes
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
		transfered += transferSize;
		retryCount = 0;

		unsigned address = getCurrentAddress();
		unsigned bytesLeft = buffer.size() - transfered;
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

	const byte* bufferPointer = &buffer[transfered];
	for (unsigned i = 0; i < amount; ++i) {
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

	const byte* bufferPointer = &buffer[transfered];
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
	buffer.resize(sectorAmount * 512);
	transfered = 0;
	doDiskWrite1();
}

void NowindHost::doDiskWrite1()
{
	unsigned bytesLeft = buffer.size() - transfered;
	if (bytesLeft == 0) {
		// All data transferred!
		unsigned sectorAmount = buffer.size() / 512;
		unsigned startSector = getStartSector();
		if (SectorAccessibleDisk* disk = changer.getSectorAccessibleDisk()) {
			try {
				for (unsigned i = 0; i < sectorAmount; ++i) {
					disk->writeSector(startSector + i, &buffer[512 * i]);
				}
			} catch (MSXException& e) {
				// TODO
			}
		}
		sendHeader();
		send(255);
		state = STATE_SYNC1;
		return;
	}

	static const unsigned BLOCKSIZE = 240;
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
	for (unsigned i = 0; i < transferSize; ++i) {
		buffer[i + transfered] = extraData[i + 1];
	}

	byte seq1 = extraData[0];
	byte seq2 = extraData[transferSize + 1];
	if ((seq1 == 0xaa) && (seq2 == 0xaa)) {
		// good block received
		transfered += transferSize;

		unsigned address = getCurrentAddress();
		unsigned bytesLeft = buffer.size() - transfered;
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
	for (int i = begin; i < end; ++i) {
		char c = extraData[i];
		if (c == ' ') break;
		result += toupper(c);
	}
	return result;
}

void NowindHost::deviceOpen()
{
	state = STATE_SYNC1;

	assert(recvCount == 11);
	string filename = extractName(0, 8);
	string ext      = extractName(8, 11);
	if (!ext.empty()) {
		filename += '.';
		filename += ext;
	}

	byte reg_d = cmdData[3];
	byte dev = reg_d & 3; // masking is really needed
	unsigned fcb = getFCB();
	devices[dev].fs.reset(new fstream()); // takes care of deleting old fs
	devices[dev].fcb = fcb;

	sendHeader();
	byte errorCode = 0;
	byte openMode = cmdData[2]; // reg_e
	switch (openMode) {
	case 1: // read-only mode
		devices[dev].fs->open(filename.c_str(), ios::in  | ios::binary);
		errorCode = 53; // file not found
		break;
	case 2: // create new file, write-only
		devices[dev].fs->open(filename.c_str(), ios::out | ios::binary);
		errorCode = 56; // bad file name
		break;
	case 8: // append to existing file, write-only
		devices[dev].fs->open(filename.c_str(), ios::out | ios::binary | ios::app);
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
	char buffer[256];
	if (openMode == 1) {
		// read-only mode, already buffer first 256 bytes
		readLen = readHelper1(dev, buffer);
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
	send(reg_d);
	send(0);
	send(0);
	send(0);
	send(0);

	if (openMode == 1) {
		readHelper2(readLen, buffer);
	}
}

int NowindHost::getDeviceNum() const
{
	unsigned fcb = getFCB();
	for (int dev = 0; dev < 4; ++dev) {
		if (devices[dev].fs.get() &&
		    devices[dev].fcb == fcb) {
			return dev;
		}
	}
	return -1;
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

	char buffer[256];
	unsigned readLen = readHelper1(dev, buffer);
	bool eof = readLen < 256;
	send(0xAF);
	send(0x05);
	send(0x00); // dummy
	send16(getFCB() + 9);
	send16(readLen + (eof ? 1 : 0));
	readHelper2(readLen, buffer);
}

unsigned NowindHost::readHelper1(unsigned dev, char* buffer)
{
	assert(dev < 4);
	unsigned len = 0;
	for (/**/; len < 256; ++len) {
		devices[dev].fs->read(&buffer[len], 1);
		if (devices[dev].fs->eof()) break;
	}
	return len;
}

void NowindHost::readHelper2(unsigned len, const char* buffer)
{
	for (unsigned i = 0; i < len; ++i) {
		send(buffer[i]);
	}
	if (len < 256) {
		send(0x1A); // end-of-file
	}
}


static string stripquotes(const string& str)
{
	string::size_type first = str.find_first_of('\"') + 1;
	string::size_type last  = str.find_last_of ('\"');
	if (first == string::npos) first = 0;
	if (last  == string::npos) last  = str.size();
	return str.substr(first, last - first);
}

void NowindHost::callImage(const string& filename)
{
	byte reg_a = cmdData[7];
	if (reg_a != 0) {
		// invalid drive number
		return;
	}
	try {
		changer.insertDisk(stripquotes(filename));
	} catch (MSXException& e) {
		// TODO
	}
}


static enum_string<NowindHost::State> stateInfo[] = {
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
	ar.serialize("hostToMsxFifo", hostToMsxFifo);
	ar.serialize("lastTime", lastTime);
	ar.serialize("state", state);
	ar.serialize("recvCount", recvCount);
	ar.serialize("cmdData", cmdData);
	ar.serialize("extraData", extraData);
	ar.serialize("buffer", buffer);
	ar.serialize("transfered", transfered);
	ar.serialize("retryCount", retryCount);
	ar.serialize("transferSize", transferSize);

	// Note: We don't serialize 'devices'. So after a loadstate it will be
	// as-if the devices are closed again. The reason for not serializing
	// this is that it's very hard to serialize a fstream (and we anyway
	// can't restore the state of the host filesystem).
}
INSTANTIATE_SERIALIZE_METHODS(NowindHost);

} // namspace openmsx
