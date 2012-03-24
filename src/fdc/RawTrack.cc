// $Id$

#include "RawTrack.hh"
#include "CRC16.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include <algorithm>
#include <cstring>
#include <cassert>

using std::vector;

namespace openmsx {

const int RawTrack::SIZE;

RawTrack::RawTrack()
{
	clear();
}

void RawTrack::clear()
{
	idam.clear();
	memset(data, 0x4e, sizeof(data));
}

void RawTrack::addIdam(int idx)
{
	assert(idx < SIZE);
	assert(idam.empty() || (idx > idam.back()));
	idam.push_back(idx);
}

bool RawTrack::decodeSector(int idx, Sector& sector) const
{
	// read (and check) address mark
	// assume addr mark starts with three A1 bytes (should be
	// located right before the current 'idx' position)
	CRC16 addrCrc;
	addrCrc.init<0xA1, 0xA1, 0xA1>();

	if (read(idx++) != 0xFE) return false;
	addrCrc.update(0xFE);
	int addrIdx = idx;

	byte trackNum = read(idx++);
	byte headNum = read(idx++);
	byte secNum = read(idx++);
	byte sizeCode = read(idx++);
	addrCrc.update(trackNum);
	addrCrc.update(headNum);
	addrCrc.update(secNum);
	addrCrc.update(sizeCode);

	byte addrCrc1 = read(idx++);
	byte addrCrc2 = read(idx++);
	bool addrCrcErr = (256 * addrCrc1 + addrCrc2) != addrCrc.getValue();

	// Locate data mark, should starts within 43 bytes from current
	// position (that's what the WD2793 does).
	for (int i = 0; i < 43; ++i) {
		CRC16 dataCrc;
		int idx2 = idx + i;
		int j = 0;
		for (; j < 3; ++j) {
			if (read(idx2++) != 0xA1) break;
			dataCrc.update(0xA1);
		}
		if (j != 3) continue; // didn't find 3 x 0xA1

		byte type = read(idx2++);
		if (!((type == 0xfb) || (type == 0xf8))) continue;
		dataCrc.update(type);
		int dataIdx = idx2;

		// OK, found start of data, calculate CRC.
		unsigned sectorSize = 128 << (sizeCode & 7);
		for (unsigned j = 0; j < sectorSize; ++j) {
			byte d = read(idx2++);
			dataCrc.update(d);
		}
		byte dataCrc1 = read(idx2++);
		byte dataCrc2 = read(idx2++);
		bool dataCrcErr = (256 * dataCrc1 + dataCrc2) != dataCrc.getValue();

		// store result
		sector.addrIdx    = addrIdx;
		sector.dataIdx    = dataIdx;
		sector.track      = trackNum;
		sector.head       = headNum;
		sector.sector     = secNum;
		sector.sizeCode   = sizeCode;
		sector.deleted    = type == 0xf8;
		sector.addrCrcErr = addrCrcErr;
		sector.dataCrcErr = dataCrcErr;
		return true;
	}
	return false;
}

vector<RawTrack::Sector> RawTrack::decodeAll() const
{
	vector<Sector> result;
	for (vector<int>::const_iterator it = idam.begin();
	     it != idam.end(); ++it) {
		Sector sector;
		if (decodeSector(*it, sector)) {
			result.push_back(sector);
		}
	}
	return result;
}

static vector<int> rotateIdam(vector<int> idam, int startIdx)
{
	// find first element that is equal or bigger
	vector<int>::iterator it = lower_bound(idam.begin(), idam.end(), startIdx);
	// rotate range so that we start at that element
	if (it != idam.end()) {
		rotate(idam.begin(), it, idam.end());
	}
	return idam;
}

bool RawTrack::decodeNextSector(int startIdx, Sector& sector) const
{
	vector<int> idamCopy = rotateIdam(idam, startIdx);
	// get first valid sector
	for (vector<int>::const_iterator it = idamCopy.begin();
	     it != idamCopy.end(); ++it) {
		if (decodeSector(*it, sector)) {
			return true;
		}
	}
	return false;
}

bool RawTrack::decodeSector(byte sectorNum, Sector& sector) const
{
	for (vector<int>::const_iterator it = idam.begin();
	     it != idam.end(); ++it) {
		if (decodeSector(*it, sector) &&
		    (sector.sector == sectorNum)) {
			return true;
		}
	}
	return false;
}

void RawTrack::readBlock(int idx, unsigned size, byte* destination) const
{
	for (unsigned i = 0; i < size; ++i) {
		destination[i] = read(idx + i);
	}
}
void RawTrack::writeBlock(int idx, unsigned size, const byte* source)
{
	for (unsigned i = 0; i < size; ++i) {
		write(idx + i, source[i]);
	}
}

word RawTrack::calcCrc(int idx, int size) const
{
	CRC16 crc;
	for (int i = 0; i < size; ++i) {
		crc.update(read(idx + i));
	}
	return crc.getValue();
}


template<typename Archive>
void RawTrack::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("idam", idam);
	ar.serialize_blob("data", data, sizeof(data));
}
INSTANTIATE_SERIALIZE_METHODS(RawTrack);

} // namespace openmsx
