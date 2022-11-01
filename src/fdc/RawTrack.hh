#ifndef RAWTRACK_HH
#define RAWTRACK_HH

#include "narrow.hh"
#include "serialize_meta.hh"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace openmsx {

class CRC16;

// This class represents a raw disk track. It contains the logical sector
// content, but also address blocks, CRC checksums, sync blocks and the data
// in the gaps between these blocks.
//
// The internal representation is based on the DMK disk image file format. See:
//    http://www.trs-80.com/wordpress/dsk-and-dmk-image-utilities/
//    (at the bottom of the page)
//
// Besides the raw track data, this format also stores the positions of the
// 'address marks' in the track (this roughly corresponds with the start of a
// sector). Of course a real disk doesn't have such a list. In a real disk this
// information is stored as 'MFM encodings with missing clock transitions'.
//
// Normal MFM encoding goes like this: An input bit of '0' is encoded as 'x0'
// with x the inverse of the previously encoded bit. A '1' input bit is encoded
// as '01'. (More in detail: a '1' encoded bit indicates a magnetic flux
// transition, a '0' bit is no flux transition). So for example the input byte
// 0xA1 (binary 10100001) is MFM encoded as '01 00 01 00 10 10 10 01'. (Note
// that decoding is simply taking every 2nd bit (the data bits), the other bits
// (the clock bits) ensure that between encoded '1' bits is always at least 1
// and at most 3 zero bits. So no too dense flux transitions and not too far
// apart to keep the stream synchronized).
//
// Now for the missing clock transitions: besides the encodings for the 256
// possible input bytes, the FDC can write two other encodings, namely:
//   01 00 01 00 10 00 10 01  (0xA1 with missing clock between bit 4 and 5)
//   01 01 00 10 00 10 01 00  (0xC2 with missing clock between bit 3 and 4)
//
// So in principle we should store each of these special 0xA1 or 0xC2 bytes.
// Instead we only store the locations of '0xA1 0xA1 0xA1 0xFE' sequences (the
// 0xA1 bytes have missing clocks, this sequence indicates the start of an
// address mark). So we don't store the location of the special 0xC2 bytes, nor
// the location of each special 0xA1 byte. These certainly do occur on a real
// disk track, but the WD2793 controller only reacts to the full sequence
// above, so for us this is good enough. (The FDC also uses these special
// encodings to re-synchronize itself with the input stream, e.g. figure out
// which bit is the start bit in a byte, but from a functional emulation point
// of view we can ignore this).
//
// Also note that it's possible to create real disks that have still completely
// different magnetic flux patterns than the 256+2 possible MFM patterns
// described above. Such disks cannot be described by this class. But for
// openMSX that's not a problem because the WD2793 or TC8566AF disk controllers
// anyway can't handle such disks (they would always interpret the flux pattern
// as one of the 256+2 MFM patterns). Other systems (like Amiga) have disk
// controllers that allow more direct access to the disk and could for example
// encode the data in a more efficient way than MFM (e.g. GCR6). That's why
// Amiga can fit more data on the same disk (even more than simply storing 10
// or 11 sectors on a track by making the gaps between the sectors smaller).

class RawTrack {
public:
	// Typical track length is 6250 bytes:
	//    250kbps, 300rpm -> 6250 bytes per rotation.
	// The IBM Disk Format Specification confirms this number.
	// Of course this is in ideal circumstances: in reality the rotation
	// speed can vary and thus the disk can be formatted with slightly more
	// or slightly less raw bytes per track. This class can also represent
	// tracks of different lengths.
	static constexpr unsigned STANDARD_SIZE = 6250;

	struct Sector
	{
		// note: initialize to avoid UMR on savestate
		int addrIdx = 0;
		int dataIdx = 0;
		uint8_t track = 0;
		uint8_t head = 0;
		uint8_t sector = 0;
		uint8_t sizeCode = 0;
		bool deleted = false;
		bool addrCrcErr = false;
		bool dataCrcErr = false;

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);
	};

	/* Construct a (cleared) track. */
	explicit RawTrack(unsigned size = STANDARD_SIZE);

	/** Clear track data. Also sets the track length. */
	void clear(unsigned size);

	/** Get track length. */
	[[nodiscard]] unsigned getLength() const { return unsigned(data.size()); }

	void addIdam(unsigned idx);

	// In the methods below, 'index' is allowed to be 'out-of-bounds',
	// it will wrap like in a circular buffer.

	[[nodiscard]] uint8_t read(int idx) const { return data[wrapIndex(idx)]; }
	void write(int idx, uint8_t val, bool setIdam = false);
	[[nodiscard]] int wrapIndex(int idx) const {
		// operator% is not a modulo but a remainder operation (makes a
		// difference for negative inputs). Hence the extra test.
		int size = narrow<int>(data.size());
		int tmp = idx % size;
		return (tmp >= 0) ? tmp : (tmp + size);
	}

	[[nodiscard]] std::span<      uint8_t> getRawBuffer()       { return data; }
	[[nodiscard]] std::span<const uint8_t> getRawBuffer() const { return data; }
	[[nodiscard]] const auto& getIdamBuffer() const { return idam; }

	/** Get info on all sectors in this track. */
	[[nodiscard]] std::vector<Sector> decodeAll() const;

	/** Get the next sector (starting from a certain index). */
	[[nodiscard]] std::optional<Sector> decodeNextSector(unsigned startIdx) const;

	/** Get a sector with a specific number.
	  * Note that if a sector with the same number occurs multiple times,
	  * this method will always return the same (the first) sector. So
	  * don't use it in the implementation of FDC / DiskDrive code.
	  */
	[[nodiscard]] std::optional<Sector> decodeSector(uint8_t sectorNum) const;

	/** Like memcpy() but copy from/to circular buffer. */
	void readBlock (int idx, std::span<uint8_t> destination) const;
	void writeBlock(int idx, std::span<const uint8_t> source);

	/** Convenience method to calculate CRC for part of this track. */
	[[nodiscard]] uint16_t calcCrc(int idx, int size) const;
	void updateCrc(CRC16& crc, int idx, int size) const;

	void applyWd2793ReadTrackQuirk();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] std::optional<Sector> decodeSectorImpl(int idx) const;

private:
	// Index into 'data'-array to positions where an address mark
	// starts (it points to the 'FE' byte in the 'A1 A1 A1 FE ..'
	// sequence.
	std::vector<unsigned> idam;

	// MFM-decoded raw data, this does NOT include the missing clock
	// transitions that can occur in the encodings of the 'A1' and
	// 'C2' bytes.
	std::vector<uint8_t> data;
};
SERIALIZE_CLASS_VERSION(RawTrack, 2);

} // namespace openmsx

#endif
