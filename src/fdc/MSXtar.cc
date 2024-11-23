// Note: For Mac OS X 10.3 <ctime> must be included before <utime.h>.
#include <ctime>
#ifndef _MSC_VER
#include <utime.h>
#else
#include <sys/utime.h>
#endif

#include "MSXtar.hh"

#include "SectorAccessibleDisk.hh"
#include "FileOperations.hh"
#include "foreach_file.hh"
#include "MSXException.hh"
#include "MsxChar2Unicode.hh"

#include "StringOp.hh"
#include "strCat.hh"
#include "File.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "stl.hh"
#include "xrange.hh"

#include <algorithm>
#include <bit>
#include <cstring>
#include <cassert>
#include <cctype>

using std::string;
using std::string_view;

namespace openmsx {

using FAT::Free;
using FAT::EndOfChain;
using FAT::Cluster;
using FAT::DirCluster;
using FAT::FatCluster;
using FAT::FileName;

namespace FAT {
	static constexpr unsigned FREE = 0x000;
	static constexpr unsigned FIRST_CLUSTER = 0x002;
}

namespace FAT12 {
	static constexpr unsigned BAD = 0xFF7;
	static constexpr unsigned END_OF_CHAIN = 0xFFF; // actually 0xFF8-0xFFF, signals EOF in FAT12

	static constexpr unsigned MAX_CLUSTER_COUNT = 0xFF4;

	// Functor to convert a FatCluster or DirCluster to a FAT12 cluster number
	struct ToClusterNumber {
		unsigned operator()(Free) const { return FAT::FREE; }
		unsigned operator()(EndOfChain) const { return END_OF_CHAIN; }
		unsigned operator()(Cluster cluster) const { return FAT::FIRST_CLUSTER + cluster.index; }
	};
}

namespace FAT16 {
	static constexpr unsigned BAD = 0xFFF7;
	static constexpr unsigned END_OF_CHAIN = 0xFFFF; // actually 0xFFF8-0xFFFF, signals EOF in FAT16

	static constexpr unsigned MAX_CLUSTER_COUNT = 0xFFF4;

	// Functor to convert a FatCluster or DirCluster to a FAT16 cluster number
	struct ToClusterNumber {
		unsigned operator()(Free) const { return FAT::FREE; }
		unsigned operator()(EndOfChain) const { return END_OF_CHAIN; }
		unsigned operator()(Cluster cluster) const { return FAT::FIRST_CLUSTER + cluster.index; }
	};
}

static constexpr unsigned SECTOR_SIZE = sizeof(SectorBuffer);
static constexpr unsigned DIR_ENTRIES_PER_SECTOR = SECTOR_SIZE / sizeof(MSXDirEntry);

static constexpr uint8_t EBPB_SIGNATURE = 0x29;  // Extended BIOS Parameter Block signature

// This particular combination of flags indicates that this dir entry is used
// to store a long Unicode file name.
// For details, read http://home.teleport.com/~brainy/lfn.htm
static constexpr MSXDirEntry::AttribValue T_MSX_LFN(0x0F); // LFN entry (long files names)

/** Transforms a cluster number towards the first sector of this cluster
  * The calculation uses info read fom the boot sector
  */
unsigned MSXtar::clusterToSector(Cluster cluster) const
{
	return dataStart + sectorsPerCluster * cluster.index;
}

/** Transforms a sector number towards it containing cluster
  * The calculation uses info read fom the boot sector
  */
Cluster MSXtar::sectorToCluster(unsigned sector) const
{
	// check lower bound since unsigned can't represent negative numbers
	// don't check upper bound since it is used in calculations like getNextSector
	assert(sector >= dataStart);
	return Cluster{(sector - dataStart) / sectorsPerCluster};
}


/** Initialize object variables by reading info from the boot sector
  */
void MSXtar::parseBootSector(const MSXBootSector& boot)
{
	fatCount          = boot.nrFats;
	sectorsPerFat     = boot.sectorsFat;
	sectorsPerCluster = boot.spCluster;

	unsigned nrSectors = boot.nrSectors;
	if (nrSectors == 0 && boot.params.extended.extendedBootSignature == EBPB_SIGNATURE) {
		nrSectors = boot.params.extended.nrSectors;
	}

	if (boot.bpSector != SECTOR_SIZE) {
		throw MSXException("Illegal sector size: ", boot.bpSector);
	}
	if (boot.resvSectors == 0) {
		throw MSXException("Illegal number of reserved sectors: ", boot.resvSectors);
	}
	if (fatCount == 0) {
		throw MSXException("Illegal number of FATs: ", fatCount);
	}
	if (sectorsPerFat == 0 || sectorsPerFat > 0x100) {
		throw MSXException("Illegal number of sectors per FAT: ", sectorsPerFat);
	}
	if (boot.dirEntries == 0 || boot.dirEntries % DIR_ENTRIES_PER_SECTOR != 0) {
		throw MSXException("Illegal number of root directory entries: ", boot.dirEntries);
	}
	if (!std::has_single_bit(sectorsPerCluster)) {
		throw MSXException("Illegal number of sectors per cluster: ", sectorsPerCluster);
	}

	unsigned nbRootDirSectors = boot.dirEntries / DIR_ENTRIES_PER_SECTOR;
	fatStart = boot.resvSectors;
	rootDirStart = fatStart + fatCount * sectorsPerFat;
	chrootSector = rootDirStart;
	dataStart = rootDirStart + nbRootDirSectors;

	// Whether to use FAT16 is strangely implicit, it must be derived from the
	// cluster count being higher than 0xFF4. Let's round that up to 0x1000,
	// since FAT12 uses 1.5 bytes per cluster its maximum size is 12 sectors,
	// whereas FAT16 uses 2 bytes per cluster so its minimum size is 16 sectors.
	// Therefore we can pick the correct FAT type with sectorsPerFat >= 14.
	constexpr unsigned fat16Threshold = (0x1000 * 3 / 2 + 0x1000 * 2) / 2 / SECTOR_SIZE;
	fat16 = sectorsPerFat >= fat16Threshold;

	if (dataStart + sectorsPerCluster > nrSectors) {
		throw MSXException("Illegal number of sectors: ", nrSectors);
	}

	clusterCount = std::min((nrSectors - dataStart) / sectorsPerCluster,
	                        fat16 ? FAT16::MAX_CLUSTER_COUNT : FAT12::MAX_CLUSTER_COUNT);

	// Some (invalid) disk images have a too small FAT to be able to address
	// all clusters of the image. OpenMSX SVN revisions pre-11326 even
	// created such invalid images for some disk sizes!!
	unsigned fatCapacity = (2 * SECTOR_SIZE * sectorsPerFat) / 3 - FAT::FIRST_CLUSTER;
	clusterCount = std::min(clusterCount, fatCapacity);
}

void MSXtar::writeLogicalSector(unsigned sector, const SectorBuffer& buf)
{
	assert(!fatBuffer.empty());
	unsigned fatSector = sector - fatStart;
	if (fatSector < sectorsPerFat) {
		// we have a cache and this is a sector of the 1st FAT
		//   --> update cache
		fatBuffer[fatSector] = buf;
		fatCacheDirty = true;
	} else {
		disk.writeSector(sector, buf);
	}
}

void MSXtar::readLogicalSector(unsigned sector, SectorBuffer& buf)
{
	assert(!fatBuffer.empty());
	unsigned fatSector = sector - fatStart;
	if (fatSector < sectorsPerFat) {
		// we have a cache and this is a sector of the 1st FAT
		//   --> read from cache
		buf = fatBuffer[fatSector];
	} else {
		disk.readSector(sector, buf);
	}
}

MSXtar::MSXtar(SectorAccessibleDisk& sectorDisk, const MsxChar2Unicode& msxChars_)
	: disk(sectorDisk)
	, msxChars(msxChars_)
{
	if (disk.getNbSectors() == 0) {
		throw MSXException("No disk inserted.");
	}
	try {
		SectorBuffer buf;
		disk.readSector(0, buf);
		parseBootSector(buf.bootSector);
	} catch (MSXException& e) {
		throw MSXException("Bad disk image: ", e.getMessage());
	}

	// cache complete FAT
	fatCacheDirty = false;
	fatBuffer.resize(sectorsPerFat);
	disk.readSectors(std::span{fatBuffer}, fatStart);
}

// Not used when NRVO is used (but NRVO optimization is not (yet) mandated)
MSXtar::MSXtar(MSXtar&& other) noexcept
	: disk(other.disk)
	, fatBuffer(std::move(other.fatBuffer))
	, msxChars(other.msxChars)
	, findFirstFreeClusterStart(other.findFirstFreeClusterStart)
	, clusterCount(other.clusterCount)
	, fatCount(other.fatCount)
	, sectorsPerCluster(other.sectorsPerCluster)
	, sectorsPerFat(other.sectorsPerFat)
	, fatStart(other.fatStart)
	, rootDirStart(other.rootDirStart)
	, dataStart(other.dataStart)
	, chrootSector(other.chrootSector)
	, fatCacheDirty(other.fatCacheDirty)
{
	other.fatCacheDirty = false;
}

MSXtar::~MSXtar()
{
	if (!fatCacheDirty) return;

	for (auto fat : xrange(fatCount)) {
		for (auto i : xrange(sectorsPerFat)) {
			try {
				disk.writeSector(i + fatStart + fat * sectorsPerFat, fatBuffer[i]);
			} catch (MSXException&) {
				// nothing
			}
		}
	}
}

// Get the next cluster number from the FAT chain
FatCluster MSXtar::readFAT(Cluster cluster) const
{
	assert(!fatBuffer.empty()); // FAT must already be cached
	assert(cluster.index < clusterCount);

	std::span<const uint8_t> data{fatBuffer[0].raw.data(), sectorsPerFat * size_t(SECTOR_SIZE)};

	unsigned index = FAT::FIRST_CLUSTER + cluster.index;
	unsigned value = [&] {
		if (fat16) {
			auto p = subspan<2>(data, index * 2);
			return p[0] | p[1] << 8;
		} else {
			auto p = subspan<2>(data, (index * 3) / 2);
			return (index & 1)
			     ? (p[0] >> 4) + (p[1] << 4)
			     : p[0] + ((p[1] & 0x0F) << 8);
		}
	}();

	// Be tolerant when reading:
	// * FREE is returned as FREE.
	// * Anything else but a valid cluster number is returned as END_OF_CHAIN.
	if (value == FAT::FREE) {
		return Free{};
	} else if (value >= FAT::FIRST_CLUSTER && value < FAT::FIRST_CLUSTER + clusterCount) {
		return Cluster{value - FAT::FIRST_CLUSTER};
	} else {
		return EndOfChain{};
	}
}

// Write an entry to the FAT
void MSXtar::writeFAT(Cluster cluster, FatCluster value)
{
	assert(!fatBuffer.empty()); // FAT must already be cached
	assert(cluster.index < clusterCount);

	// Be strict when writing:
	// * Anything but FREE, END_OF_CHAIN or a valid cluster number is rejected.
	assert(!std::holds_alternative<Cluster>(value) || std::get<Cluster>(value).index < clusterCount);

	std::span data{fatBuffer[0].raw.data(), sectorsPerFat * size_t(SECTOR_SIZE)};

	unsigned index = FAT::FIRST_CLUSTER + cluster.index;

	if (std::holds_alternative<Free>(value) && cluster < findFirstFreeClusterStart) {
		// update where findFirstFreeCluster() will start scanning
		findFirstFreeClusterStart = cluster;
	}

	if (fat16) {
		unsigned fatValue = std::visit(FAT16::ToClusterNumber{}, value);
		auto p = subspan<2>(data, index * 2);
		p[0] = narrow_cast<uint8_t>(fatValue);
		p[1] = narrow_cast<uint8_t>(fatValue >> 8);
	} else {
		unsigned fatValue = std::visit(FAT12::ToClusterNumber{}, value);
		auto p = subspan<2>(data, (index * 3) / 2);
		if (index & 1) {
			p[0] = narrow_cast<uint8_t>((p[0] & 0x0F) + (fatValue << 4));
			p[1] = narrow_cast<uint8_t>(fatValue >> 4);
		} else {
			p[0] = narrow_cast<uint8_t>(fatValue);
			p[1] = narrow_cast<uint8_t>((p[1] & 0xF0) + ((fatValue >> 8) & 0x0F));
		}
	}
	fatCacheDirty = true;
}

// Find the next cluster number marked as free in the FAT
// @throws When no more free clusters
Cluster MSXtar::findFirstFreeCluster()
{
	for (auto cluster : xrange(findFirstFreeClusterStart.index, clusterCount)) {
		if (readFAT({cluster}) == FatCluster(Free{})) {
			findFirstFreeClusterStart = {cluster};
			return findFirstFreeClusterStart;
		}
	}
	throw MSXException("Disk full.");
}

unsigned MSXtar::countFreeClusters() const
{
	return narrow<unsigned>(ranges::count_if(xrange(findFirstFreeClusterStart.index, clusterCount),
		[&](unsigned cluster) { return readFAT({cluster}) == FatCluster(Free{}); }));
}

// Get the next sector from a file or (root/sub)directory
// If no next sector then 0 is returned
unsigned MSXtar::getNextSector(unsigned sector) const
{
	assert(sector >= rootDirStart);
	if (sector < dataStart) {
		// sector is part of the root directory
		return (sector == dataStart - 1) ? 0 : sector + 1;
	}
	Cluster currCluster = sectorToCluster(sector);
	if (currCluster == sectorToCluster(sector + 1)) {
		// next sector of cluster
		return sector + 1;
	} else {
		// first sector in next cluster
		FatCluster nextCluster = readFAT(currCluster);
		return std::visit(overloaded{
			[](Free) { return 0u; /* Invalid entry in FAT chain. */ },
			[](EndOfChain) { return 0u; },
			[this](Cluster cluster) { return clusterToSector(cluster); }
		}, nextCluster);
	}
}

// Get start cluster from a directory entry.
DirCluster MSXtar::getStartCluster(const MSXDirEntry& entry) const
{
	// Be tolerant when reading:
	// * Anything but a valid cluster number is returned as FREE.
	unsigned cluster = entry.startCluster;
	if (cluster >= FAT::FIRST_CLUSTER && cluster < FAT::FIRST_CLUSTER + clusterCount) {
		return Cluster{cluster - FAT::FIRST_CLUSTER};
	} else {
		return Free{};
	}
}

// Set start cluster on a directory entry.
void MSXtar::setStartCluster(MSXDirEntry& entry, DirCluster cluster) const
{
	// Be strict when writing:
	// * Anything but FREE or a valid cluster number is rejected.
	assert(!std::holds_alternative<Cluster>(cluster) || std::get<Cluster>(cluster).index < clusterCount);
	if (fat16) {
		entry.startCluster = narrow<uint16_t>(std::visit(FAT16::ToClusterNumber{}, cluster));
	} else {
		entry.startCluster = narrow<uint16_t>(std::visit(FAT12::ToClusterNumber{}, cluster));
	}
}

// If there are no more free entries in a subdirectory, the subdir is
// expanded with an extra cluster. This function gets the free cluster,
// clears it and updates the fat for the subdir
// returns: the first sector in the newly appended cluster
// @throws When disk is full
unsigned MSXtar::appendClusterToSubdir(unsigned sector)
{
	Cluster nextCl = findFirstFreeCluster();
	unsigned nextSector = clusterToSector(nextCl);

	// clear this cluster
	SectorBuffer buf;
	ranges::fill(buf.raw, 0);
	for (auto i : xrange(sectorsPerCluster)) {
		writeLogicalSector(i + nextSector, buf);
	}

	Cluster curCl = sectorToCluster(sector);
	assert(readFAT(curCl) == FatCluster(EndOfChain{}));
	writeFAT(curCl, nextCl);
	writeFAT(nextCl, EndOfChain{});
	return nextSector;
}


// Returns the index of a free (or with deleted file) entry
//  In:  The current dir sector
//  Out: index number, if no index is found then -1 is returned
unsigned MSXtar::findUsableIndexInSector(unsigned sector)
{
	SectorBuffer buf;
	readLogicalSector(sector, buf);

	// find a not used (0x00) or delete entry (0xE5)
	for (auto i : xrange(DIR_ENTRIES_PER_SECTOR)) {
		if (buf.dirEntry[i].filename[0] == one_of(0x00, char(0xE5))) {
			return i;
		}
	}
	return unsigned(-1);
}

// This function returns the sector and dirIndex for a new directory entry
// if needed the involved subdirectory is expanded by an extra cluster
// returns: a DirEntry containing sector and index
// @throws When either root dir is full or disk is full
MSXtar::DirEntry MSXtar::addEntryToDir(unsigned sector)
{
	// this routine adds the msx name to a directory sector, if needed (and
	// possible) the directory is extened with an extra cluster
	DirEntry result;
	result.sector = sector;

	assert(sector >= rootDirStart);
	if (sector < dataStart) {
		// add to the root directory
		for (/* */ ; result.sector < dataStart; result.sector++) {
			result.index = findUsableIndexInSector(result.sector);
			if (result.index != unsigned(-1)) {
				return result;
			}
		}
		throw MSXException("Root directory full.");

	} else {
		// add to a subdir
		while (true) {
			result.index = findUsableIndexInSector(result.sector);
			if (result.index != unsigned(-1)) {
				return result;
			}
			unsigned nextSector = getNextSector(result.sector);
			if (nextSector == 0) {
				nextSector = appendClusterToSubdir(result.sector);
			}
			result.sector = nextSector;
		}
	}
}

// filters out unsupported characters and upper-cases
static char toFileNameChar(char a)
{
	if ((a >= 0x00 && a < 0x20) || a == one_of(
		' ', '"', '*', '+', ',', '.', '/', ':', ';', '<', '=', '>', '?', '[', '\\', ']', '|', 0x7F, 0xFF)) {
		a = '_';
	}
	return narrow<char>(toupper(a));
}

// Transform a long hostname in a 8.3 uppercase filename as used in the
// dirEntries on an MSX
FileName MSXtar::hostToMSXFileName(string_view hostName) const
{
	std::vector<uint8_t> hostMSXName = msxChars.utf8ToMsx(hostName, '_');
	std::string_view hostMSXNameView(std::bit_cast<char*>(hostMSXName.data()), hostMSXName.size());
	auto [hostDir, hostFile] = StringOp::splitOnLast(hostMSXNameView, '/');

	// handle special case '.' and '..' first
	FileName result;
	result.fill(' ');
	if (hostFile == one_of(".", "..")) {
		ranges::copy(hostFile, result);
		return result;
	}

	auto [file, ext] = StringOp::splitOnLast(hostFile, '.');
	if (file.empty()) std::swap(file, ext);

	StringOp::trimRight(file, ' ');
	StringOp::trimRight(ext,  ' ');

	// truncate to 8.3 characters, put in uppercase and create '_' if needed
	string fileS(file.substr(0, 8));
	string extS (ext .substr(0, 3));
	transform_in_place(fileS, toFileNameChar);
	transform_in_place(extS,  toFileNameChar);

	// add correct number of spaces
	ranges::copy(fileS, subspan<8>(result, 0));
	ranges::copy(extS,  subspan<3>(result, 8));
	return result;
}

// This function creates a new MSX subdir with given date 'd' and time 't'
// in the subdir pointed at by 'sector'. In the newly
// created subdir the entries for '.' and '..' are created
// returns: the first sector of the new subdir
// @throws in case no directory could be created
unsigned MSXtar::addSubdir(
	const FileName& msxName, uint16_t t, uint16_t d, unsigned sector)
{
	// returns the sector for the first cluster of this subdir
	DirEntry result = addEntryToDir(sector);

	// load the sector
	SectorBuffer buf;
	readLogicalSector(result.sector, buf);

	auto& dirEntry = buf.dirEntry[result.index];
	ranges::copy(msxName, dirEntry.filename);
	dirEntry.attrib = MSXDirEntry::Attrib::DIRECTORY;
	dirEntry.time = t;
	dirEntry.date = d;

	// dirEntry.filesize = fsize;
	Cluster curCl = findFirstFreeCluster();
	setStartCluster(dirEntry, curCl);
	writeFAT(curCl, EndOfChain{});

	// save the sector again
	writeLogicalSector(result.sector, buf);

	// clear this cluster
	unsigned logicalSector = clusterToSector(curCl);
	ranges::fill(buf.raw, 0);
	for (auto i : xrange(sectorsPerCluster)) {
		writeLogicalSector(i + logicalSector, buf);
	}

	// now add the '.' and '..' entries!!
	memset(&buf.dirEntry[0], 0, sizeof(MSXDirEntry));
	ranges::fill(buf.dirEntry[0].filename, ' ');
	buf.dirEntry[0].filename[0] = '.';
	buf.dirEntry[0].attrib = MSXDirEntry::Attrib::DIRECTORY;
	buf.dirEntry[0].time = t;
	buf.dirEntry[0].date = d;
	setStartCluster(buf.dirEntry[0], curCl);

	memset(&buf.dirEntry[1], 0, sizeof(MSXDirEntry));
	ranges::fill(buf.dirEntry[1].filename, ' ');
	buf.dirEntry[1].filename[0] = '.';
	buf.dirEntry[1].filename[1] = '.';
	buf.dirEntry[1].attrib = MSXDirEntry::Attrib::DIRECTORY;
	buf.dirEntry[1].time = t;
	buf.dirEntry[1].date = d;
	if (sector == rootDirStart) {
		setStartCluster(buf.dirEntry[1], Free{});
	} else {
		setStartCluster(buf.dirEntry[1], sectorToCluster(sector));
	}

	// and save this in the first sector of the new subdir
	writeLogicalSector(logicalSector, buf);

	return logicalSector;
}

// Get the time/date from a host file in MSX format
static DiskImageUtils::FatTimeDate getTimeDate(zstring_view filename)
{
	if (auto st = FileOperations::getStat(filename)) {
		// Some info indicates that st.st_mtime could be useless on win32 with vfat.
		// On Android 'st_mtime' is 'unsigned long' instead of 'time_t'
		// (like on linux), so we require a reinterpret_cast. That cast
		// is fine (but redundant) on linux.
		return DiskImageUtils::toTimeDate(reinterpret_cast<time_t&>(st->st_mtime));
	} else {
		// stat failed
		return {0, 0};
	}
}

// Add an MSXsubdir with the time properties from the HOST-OS subdir
// @throws when subdir could not be created
unsigned MSXtar::addSubdirToDSK(zstring_view hostName, const FileName& msxName,
                                unsigned sector)
{
	auto [time, date] = getTimeDate(hostName);
	return addSubdir(msxName, time, date, sector);
}

// This file alters the filecontent of a given file
// It only changes the file content (and the filesize in the msxDirEntry)
// It doesn't changes timestamps nor filename, filetype etc.
// @throws when something goes wrong
void MSXtar::alterFileInDSK(MSXDirEntry& msxDirEntry, const string& hostName)
{
	// get host file size
	auto st = FileOperations::getStat(hostName);
	if (!st) {
		throw MSXException("Error reading host file: ", hostName);
	}
	auto hostSize = narrow<unsigned>(st->st_size);
	auto remaining = hostSize;

	// open host file for reading
	File file(hostName, "rb");

	// copy host file to image
	DirCluster prevCl = Free{};
	FatCluster curCl = std::visit(overloaded{
		[](Free) -> FatCluster { return EndOfChain{}; },
		[](Cluster cluster) -> FatCluster { return cluster; }
	}, getStartCluster(msxDirEntry));

	while (remaining) {
		Cluster cluster;
		// allocate new cluster if needed
		try {
			cluster = std::visit(overloaded{
				[](Free) -> Cluster { throw MSXException("Invalid entry in FAT chain."); },
				[&](EndOfChain) {
					Cluster newCl = findFirstFreeCluster();
					std::visit(overloaded{
						[&](Free) { setStartCluster(msxDirEntry, newCl); },
						[&](Cluster cluster_) { writeFAT(cluster_, newCl); }
					}, prevCl);
					writeFAT(newCl, EndOfChain{});
					return newCl;
				},
				[](Cluster cluster_) { return cluster_; }
			}, curCl);
		} catch (MSXException&) {
			// no more free clusters or invalid entry in FAT chain
			break;
		}

		// fill cluster
		unsigned logicalSector = clusterToSector(cluster);
		for (unsigned j = 0; (j < sectorsPerCluster) && remaining; ++j) {
			SectorBuffer buf;
			unsigned chunkSize = std::min(SECTOR_SIZE, remaining);
			file.read(subspan(buf.raw, 0, chunkSize));
			ranges::fill(subspan(buf.raw, chunkSize), 0);
			writeLogicalSector(logicalSector + j, buf);
			remaining -= chunkSize;
		}

		// advance to next cluster
		prevCl = cluster;
		curCl = readFAT(cluster);
	}

	// terminate FAT chain
	std::visit(overloaded{
		[&](Free free) { setStartCluster(msxDirEntry, free); },
		[&](Cluster cluster) { writeFAT(cluster, EndOfChain{}); }
	}, prevCl);

	// free rest of FAT chain
	freeFatChain(curCl);

	// write (possibly truncated) file size
	msxDirEntry.size = hostSize - remaining;

	if (remaining) {
		throw MSXException("Disk full, ", hostName, " truncated.");
	}
}

void MSXtar::freeFatChain(FAT::FatCluster startCluster)
{
	FatCluster curCl = startCluster;
	while (std::holds_alternative<Cluster>(curCl)) {
		Cluster cluster = std::get<Cluster>(curCl);
		FatCluster nextCl = readFAT(cluster);
		writeFAT(cluster, Free{});
		curCl = nextCl;
	}
}

std::string MSXtar::deleteEntry(const FAT::FileName& msxName, unsigned rootSector)
{
	SectorBuffer buf;
	DirEntry entry = findEntryInDir(msxName, rootSector, buf);
	if (entry.sector == 0) {
		// not found
		return "entry not found";
	}
	deleteEntry(buf.dirEntry[entry.index]);
	writeLogicalSector(entry.sector, buf);
	return "";
}

void MSXtar::deleteEntry(MSXDirEntry& msxDirEntry)
{
	DirCluster startCluster = getStartCluster(msxDirEntry);

	if (msxDirEntry.attrib & MSXDirEntry::Attrib::DIRECTORY) {
		// If we're deleting a directory then also (recursively)
		// delete the files/directories in this directory.
		if (const auto& msxName = msxDirEntry.filename;
		    ranges::equal(msxName, std::string_view(".          ")) ||
		    ranges::equal(msxName, std::string_view("..         "))) {
			// But skip the "." and ".." entries.
			return;
		}
		std::visit(overloaded{
			[](Free) { /* Points to root, ignore. */ },
			[&](Cluster cluster) { deleteDirectory(clusterToSector(cluster)); }
		}, startCluster);
	}

	// At this point we have a regular file or an empty subdirectory.
	// Delete it by marking the first filename char as 0xE5.
	msxDirEntry.filename[0] = char(0xE5);

	// Free the FAT chain
	std::visit(overloaded{
		[](Free) { /* Points to root, ignore. */ },
		[&](Cluster cluster) { freeFatChain(cluster); }
	}, startCluster);

	// Changed sector will be written by parent function
}

void MSXtar::deleteDirectory(unsigned sector)
{
	for (/* */ ; sector != 0; sector = getNextSector(sector)) {
		SectorBuffer buf;
		readLogicalSector(sector, buf);
		for (auto& dirEntry : buf.dirEntry) {
			if (dirEntry.filename[0] == char(0x00)) {
				return;
			}
			if (dirEntry.filename[0] == one_of(char(0xe5), '.') || dirEntry.attrib == T_MSX_LFN) {
				continue;
			}
			deleteEntry(dirEntry);
		}
		writeLogicalSector(sector, buf);
	}
}

std::string MSXtar::renameItem(std::string_view currentName, std::string_view newName)
{
	SectorBuffer buf;

	FileName newMsxName = hostToMSXFileName(newName);
	if (auto newEntry = findEntryInDir(newMsxName, chrootSector, buf);
	    newEntry.sector != 0) {
		return "another entry with new name already exists";
	}

	FileName oldMsxName = hostToMSXFileName(currentName);
	auto oldEntry = findEntryInDir(oldMsxName, chrootSector, buf);
	if (oldEntry.sector == 0) {
		return "entry not found";
	}

	buf.dirEntry[oldEntry.index].filename = newMsxName;
	writeLogicalSector(oldEntry.sector, buf);
	return "";
}

// Find the dir entry for 'name' in subdir starting at the given 'sector'
// with given 'index'
// returns: a DirEntry with sector and index filled in
//          sector is 0 if no match was found
MSXtar::DirEntry MSXtar::findEntryInDir(
	const FileName& msxName, unsigned sector, SectorBuffer& buf)
{
	DirEntry result;
	result.sector = sector;
	result.index = 0; // avoid warning (only some gcc versions complain)
	while (result.sector) {
		// read sector and scan 16 entries
		readLogicalSector(result.sector, buf);
		for (result.index = 0; result.index < DIR_ENTRIES_PER_SECTOR; ++result.index) {
			if (ranges::equal(buf.dirEntry[result.index].filename, msxName)) {
				return result;
			}
		}
		// try next sector
		result.sector = getNextSector(result.sector);
	}
	return result;
}

// Add file to the MSX disk in the subdir pointed to by 'sector'
// @throws when file could not be added
string MSXtar::addFileToDSK(const string& fullHostName, unsigned rootSector, Add add)
{
	auto [directory, hostName] = StringOp::splitOnLast(fullHostName, "/\\");
	FileName msxName = hostToMSXFileName(hostName);

	// first find out if the filename already exists in current dir
	SectorBuffer dummy;
	if (DirEntry fullMsxDirEntry = findEntryInDir(msxName, rootSector, dummy);
	    fullMsxDirEntry.sector != 0) {
		if (add == Add::PRESERVE) {
			return strCat("Warning: preserving entry ", hostName, '\n');
		} else {
			// first delete entry
			deleteEntry(dummy.dirEntry[fullMsxDirEntry.index]);
			writeLogicalSector(fullMsxDirEntry.sector, dummy);
		}
	}

	SectorBuffer buf;
	DirEntry entry = addEntryToDir(rootSector);
	readLogicalSector(entry.sector, buf);
	auto& dirEntry = buf.dirEntry[entry.index];
	memset(&dirEntry, 0, sizeof(dirEntry));
	ranges::copy(msxName, dirEntry.filename);
	dirEntry.attrib = MSXDirEntry::Attrib::REGULAR;

	// compute time/date stamps
	auto [time, date] = getTimeDate(fullHostName);
	dirEntry.time = time;
	dirEntry.date = date;

	try {
		alterFileInDSK(dirEntry, fullHostName);
	} catch (MSXException&) {
		// still write directory entry
		writeLogicalSector(entry.sector, buf);
		throw;
	}
	writeLogicalSector(entry.sector, buf);
	return {};
}

std::string MSXtar::addOrCreateSubdir(zstring_view hostDirName, unsigned sector, Add add)
{
	FileName msxFileName = hostToMSXFileName(hostDirName);
	auto printableFilename = msxToHostFileName(msxFileName);
	SectorBuffer buf;
	if (DirEntry entry = findEntryInDir(msxFileName, sector, buf);
	    entry.sector != 0) {
		// entry already exists ..
		auto& msxDirEntry = buf.dirEntry[entry.index];
		if (msxDirEntry.attrib & MSXDirEntry::Attrib::DIRECTORY) {
			// .. and is a directory
			DirCluster nextCluster = getStartCluster(msxDirEntry);
			return std::visit(overloaded{
				[&](Free) { return strCat("Directory ", printableFilename, " goes to root.\n"); },
				[&](Cluster cluster) { return recurseDirFill(hostDirName, clusterToSector(cluster), add); }
			}, nextCluster);
		}
		// .. but is NOT a directory
		if (add == Add::PRESERVE) {
			return strCat("MSX file ", printableFilename, " is not a directory.\n");
		} else {
			// first delete existing file
			deleteEntry(msxDirEntry);
			writeLogicalSector(entry.sector, buf);
		}
	}
	// add new directory
	unsigned nextSector = addSubdirToDSK(hostDirName, msxFileName, sector);
	return recurseDirFill(hostDirName, nextSector, add);
}

// Transfer directory and all its subdirectories to the MSX disk image
// @throws when an error occurs
string MSXtar::recurseDirFill(string_view dirName, unsigned sector, Add add)
{
	string messages;

	auto fileAction = [&](const string& path) {
		// add new file
		messages += addFileToDSK(path, sector, add);
	};
	auto dirAction = [&](const string& path) {
		// add new directory (+ recurse)
		messages += addOrCreateSubdir(path, sector, add);
	};
	foreach_file_and_directory(std::string(dirName), fileAction, dirAction);

	return messages;
}


string MSXtar::msxToHostFileName(const FileName& msxName) const
{
	string result;
	for (unsigned i = 0; i < 8 && msxName[i] != ' '; ++i) {
		result += char(tolower(msxName[i]));
	}
	if (msxName[8] != ' ') {
		result += '.';
		for (unsigned i = 8; i < 11 && msxName[i] != ' '; ++i) {
			result += char(tolower(msxName[i]));
		}
	}
	std::span<const uint8_t> resultSpan(std::bit_cast<const uint8_t*>(result.data()), result.size());
	return msxChars.msxToUtf8(resultSpan, '_');
}


// Set the entries from dirEntry to the timestamp of resultFile
static void changeTime(zstring_view resultFile, const MSXDirEntry& dirEntry)
{
	unsigned t = dirEntry.time;
	unsigned d = dirEntry.date;
	struct tm mTim;
	struct utimbuf uTim;
	mTim.tm_sec   = narrow<int>((t & 0x001f) << 1);
	mTim.tm_min   = narrow<int>((t & 0x07e0) >> 5);
	mTim.tm_hour  = narrow<int>((t & 0xf800) >> 11);
	mTim.tm_mday  = narrow<int>( (d & 0x001f) >> 0);
	mTim.tm_mon   = narrow<int>(((d & 0x01e0) >> 5) - 1);
	mTim.tm_year  = narrow<int>(((d & 0xfe00) >> 9) + 80);
	mTim.tm_isdst = -1;
	uTim.actime  = mktime(&mTim);
	uTim.modtime = mktime(&mTim);
	utime(resultFile.c_str(), &uTim);
}

TclObject MSXtar::dirRaw()
{
	TclObject result;
	for (unsigned sector = chrootSector; sector != 0; sector = getNextSector(sector)) {
		SectorBuffer buf;
		readLogicalSector(sector, buf);
		for (auto& dirEntry : buf.dirEntry) {
			if (dirEntry.filename[0] == char(0x00)) {
				return result;
			}
			if (dirEntry.filename[0] == char(0xe5) || dirEntry.attrib == T_MSX_LFN) {
				continue;
			}

			auto filename = msxToHostFileName(dirEntry.filename);
			time_t time = DiskImageUtils::fromTimeDate(DiskImageUtils::FatTimeDate{dirEntry.time, dirEntry.date});
			result.addListElement(makeTclList(filename, dirEntry.attrib.value, narrow<uint32_t>(time), dirEntry.size));
		}
	}
	return result;
}

std::string MSXtar::dir()
{
	std::string result;
	auto list = dirRaw();
	auto num = list.size();
	for (unsigned i = 0; i < num; ++i) {
		auto entry = list.getListIndexUnchecked(i);
		auto filename = std::string(entry.getListIndexUnchecked(0).getString());
		auto attrib = DiskImageUtils::formatAttrib(MSXDirEntry::AttribValue(uint8_t(entry.getListIndexUnchecked(1).getOptionalInt().value_or(0))));
		//time_t time = entry.getListIndexUnchecked(2).getOptionalInt().value_or(0); // ignored
		auto size = entry.getListIndexUnchecked(3).getOptionalInt().value_or(0);

		filename.resize(13, ' '); // filename first (in condensed form for human readability)
		strAppend(result, filename, attrib, "  ", size, '\n');
	}
	return result;
}

// routines to update the global vars: chrootSector
void MSXtar::chdir(string_view newRootDir)
{
	chroot(newRootDir, false);
}

void MSXtar::mkdir(string_view newRootDir)
{
	unsigned tmpMSXchrootSector = chrootSector;
	chroot(newRootDir, true);
	chrootSector = tmpMSXchrootSector;
}

void MSXtar::chroot(string_view newRootDir, bool createDir)
{
	if (newRootDir.starts_with('/') || newRootDir.starts_with('\\')) {
		// absolute path, reset chrootSector
		chrootSector = rootDirStart;
		StringOp::trimLeft(newRootDir, "/\\");
	}

	while (!newRootDir.empty()) {
		auto [firstPart, lastPart] = StringOp::splitOnFirst(newRootDir, "/\\");
		newRootDir = lastPart;
		StringOp::trimLeft(newRootDir, "/\\");

		// find 'firstPart' directory or create it if requested
		SectorBuffer buf;
		FileName msxName = hostToMSXFileName(firstPart);
		DirEntry entry = findEntryInDir(msxName, chrootSector, buf);
		if (entry.sector == 0) {
			if (!createDir) {
				throw MSXException("Subdirectory ", firstPart,
				                   " not found.");
			}
			// creat new subdir
			time_t now;
			time(&now);
			auto [t, d] = DiskImageUtils::toTimeDate(now);
			chrootSector = addSubdir(msxName, t, d, chrootSector);
		} else {
			const auto& dirEntry = buf.dirEntry[entry.index];
			if (!(dirEntry.attrib & MSXDirEntry::Attrib::DIRECTORY)) {
				throw MSXException(firstPart, " is not a directory.");
			}
			DirCluster chrootCluster = getStartCluster(dirEntry);
			chrootSector = std::visit(overloaded{
				[this](Free) { return rootDirStart; },
				[this](Cluster cluster) { return clusterToSector(cluster); }
			}, chrootCluster);
		}
	}
}

void MSXtar::fileExtract(const string& resultFile, const MSXDirEntry& dirEntry)
{
	unsigned size = dirEntry.size;
	unsigned sector = std::visit(overloaded{
		[](Free) { return 0u; },
		[this](Cluster cluster) { return clusterToSector(cluster); }
	}, getStartCluster(dirEntry));

	File file(resultFile, "wb");
	while (size && sector) {
		SectorBuffer buf;
		readLogicalSector(sector, buf);
		unsigned saveSize = std::min(size, SECTOR_SIZE);
		file.write(subspan(buf.raw, 0, saveSize));
		size -= saveSize;
		sector = getNextSector(sector);
	}
	// now change the access time
	changeTime(resultFile, dirEntry);
}

// extracts a single item (file or directory) from the msx image to the host OS
string MSXtar::singleItemExtract(string_view dirName, string_view itemName,
                                 unsigned sector)
{
	// first find out if the filename exists in current dir
	SectorBuffer buf;
	FileName msxName = hostToMSXFileName(itemName);
	DirEntry entry = findEntryInDir(msxName, sector, buf);
	if (entry.sector == 0) {
		return strCat(itemName, " not found!\n");
	}

	const auto& msxDirEntry = buf.dirEntry[entry.index];
	// create full name for local filesystem
	string fullName = strCat(dirName, '/', msxToHostFileName(msxDirEntry.filename));

	// ...and extract
	if  (msxDirEntry.attrib & MSXDirEntry::Attrib::DIRECTORY) {
		// recursive extract this subdir
		FileOperations::mkdirp(fullName);
		DirCluster nextCluster = getStartCluster(msxDirEntry);
		std::visit(overloaded{
			[](Free) { /* Points to root, ignore. */},
			[&](Cluster cluster) { recurseDirExtract(fullName, clusterToSector(cluster)); }
		}, nextCluster);
	} else {
		// it is a file
		fileExtract(fullName, msxDirEntry);
	}
	return {};
}


// extracts the contents of the directory (at sector) and all its subdirs to the host OS
void MSXtar::recurseDirExtract(string_view dirName, unsigned sector)
{
	for (/* */ ; sector != 0; sector = getNextSector(sector)) {
		SectorBuffer buf;
		readLogicalSector(sector, buf);
		for (auto& dirEntry : buf.dirEntry) {
			if (dirEntry.filename[0] == char(0x00)) {
				return;
			}
			if (dirEntry.filename[0] == one_of(char(0xe5), '.') || dirEntry.attrib == T_MSX_LFN) {
				continue;
			}
			string filename = msxToHostFileName(dirEntry.filename);
			string fullName = filename;
			if (!dirName.empty()) {
				fullName = strCat(dirName, '/', filename);
			}
			if (dirEntry.attrib & MSXDirEntry::Attrib::DIRECTORY) {
				FileOperations::mkdirp(fullName);
				// now change the access time
				changeTime(fullName, dirEntry);
				std::visit(overloaded{
					[](Free) { /* Points to root, ignore. */ },
					[&](Cluster cluster) { recurseDirExtract(fullName, clusterToSector(cluster)); }
				}, getStartCluster(dirEntry));
			} else {
				fileExtract(fullName, dirEntry);
			}
		}
	}
}

std::string MSXtar::addItem(const std::string& hostItemName, Add add)
{
	if (auto stat = FileOperations::getStat(hostItemName)) {
		if (FileOperations::isRegularFile(*stat)) {
			return addFileToDSK(hostItemName, chrootSector, add);
		} else if (FileOperations::isDirectory(hostItemName)) {
			return addOrCreateSubdir(hostItemName, chrootSector, add);
		}
	}
	return "";
}

string MSXtar::addDir(string_view rootDirName, Add add)
{
	return recurseDirFill(rootDirName, chrootSector, add);
}

string MSXtar::addFile(const string& filename, Add add)
{
	return addFileToDSK(filename, chrootSector, add);
}

string MSXtar::getItemFromDir(string_view rootDirName, string_view itemName)
{
	return singleItemExtract(rootDirName, itemName, chrootSector);
}

void MSXtar::getDir(string_view rootDirName)
{
	recurseDirExtract(rootDirName, chrootSector);
}

std::string MSXtar::deleteItem(std::string_view itemName)
{
	FileName msxName = hostToMSXFileName(itemName);
	return deleteEntry(msxName, chrootSector);
}

std::string MSXtar::convertToMsxName(std::string_view name) const
{
	FileName msxName = hostToMSXFileName(name);
	return msxToHostFileName(msxName);
}

MSXtar::FreeSpaceResult MSXtar::getFreeSpace() const
{
	return {countFreeClusters(), sectorsPerCluster * SECTOR_SIZE};
}

} // namespace openmsx
