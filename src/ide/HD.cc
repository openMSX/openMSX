#include "HD.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FilePool.hh"
#include "DeviceConfig.hh"
#include "CliComm.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "GlobalSettings.hh"
#include "MSXException.hh"
#include "HDCommand.hh"
#include "serialize.hh"
#include "memory.hh"
#include "xrange.hh"
#include <cassert>

namespace openmsx {

using std::string;
using std::vector;

HD::HD(const DeviceConfig& config)
	: motherBoard(config.getMotherBoard())
	, name("hdX")
{
	hdInUse = motherBoard.getSharedStuff<HDInUse>("hdInUse");

	unsigned id = 0;
	while ((*hdInUse)[id]) {
		++id;
		if (id == MAX_HD) {
			throw MSXException("Too many HDs");
		}
	}
	// for exception safety, set hdInUse only at the end
	name[2] = char('a' + id);

	// For the initial hd image, savestate should only try exactly this
	// (resolved) filename. For user-specified hd images (commandline or
	// via hda command) savestate will try to re-resolve the filename.
	string original = config.getChildData("filename");
	string resolved = config.getFileContext().resolveCreate(original);
	filename = Filename(resolved);
	try {
		file = File(filename);
		filesize = file.getSize();
		tigerTree = make_unique<TigerTree>(*this, filesize,
		                                   filename.getResolved());
	} catch (FileException&) {
		// Image didn't exist yet, but postpone image creation:
		// we don't want to create images during 'testconfig'
		filesize = size_t(config.getChildDataAsInt("size")) * 1024 * 1024;
	}
	alreadyTried = false;

	(*hdInUse)[id] = true;
	hdCommand = make_unique<HDCommand>(
		motherBoard.getCommandController(),
		motherBoard.getStateChangeDistributor(),
		motherBoard.getScheduler(),
		*this,
		motherBoard.getReactor().getGlobalSettings().getPowerSetting());

	motherBoard.getMSXCliComm().update(CliComm::HARDWARE, name, "add");
}

HD::~HD()
{
	motherBoard.getMSXCliComm().update(CliComm::HARDWARE, name, "remove");

	unsigned id = name[2] - 'a';
	assert((*hdInUse)[id]);
	(*hdInUse)[id] = false;
}

void HD::openImage()
{
	if (file.is_open()) return;

	// image didn't exist yet, create new
	if (alreadyTried) {
		throw FileException("No HD image");
	}
	alreadyTried = true;
	try {
		file = File(filename, File::CREATE);
		file.truncate(filesize);
		tigerTree = make_unique<TigerTree>(*this, filesize,
		                                   filename.getResolved());
	} catch (FileException& e) {
		motherBoard.getMSXCliComm().printWarning(
			"Couldn't create HD image: " + e.getMessage());
		throw;
	}
}

void HD::switchImage(const Filename& newFilename)
{
	file = File(newFilename);
	filename = newFilename;
	filesize = file.getSize();
	tigerTree = make_unique<TigerTree>(*this, filesize,
	                                   filename.getResolved());
	motherBoard.getMSXCliComm().update(CliComm::MEDIA, getName(),
	                                   filename.getResolved());
}

size_t HD::getNbSectorsImpl() const
{
	const_cast<HD&>(*this).openImage();
	return filesize / sizeof(SectorBuffer);
}

void HD::readSectorImpl(size_t sector, SectorBuffer& buf)
{
	openImage();
	file.seek(sector * sizeof(buf));
	file.read(&buf, sizeof(buf));
}

void HD::writeSectorImpl(size_t sector, const SectorBuffer& buf)
{
	openImage();
	file.seek(sector * sizeof(buf));
	file.write(&buf, sizeof(buf));
	tigerTree->notifyChange(sector * sizeof(buf), sizeof(buf),
	                        file.getModificationDate());
}

bool HD::isWriteProtectedImpl() const
{
	const_cast<HD&>(*this).openImage();
	return file.isReadOnly();
}

Sha1Sum HD::getSha1SumImpl(FilePool& filePool)
{
	openImage();
	if (hasPatches()) {
		return SectorAccessibleDisk::getSha1SumImpl(filePool);
	}
	return filePool.getSha1Sum(file);
}

std::string HD::getTigerTreeHash()
{
	openImage();
	return tigerTree->calcHash().toString(); // calls HD::getData()
}

uint8_t* HD::getData(size_t offset, size_t size)
{
	assert(size <= 1024);
	assert((offset % sizeof(SectorBuffer)) == 0);
	assert((size   % sizeof(SectorBuffer)) == 0);

	struct Work {
		char extra; // at least one byte before 'bufs'
		// likely here are padding bytes inbetween
		SectorBuffer bufs[1024 / sizeof(SectorBuffer)];
	};
	static Work work; // not reentrant

	size_t sector = offset / sizeof(SectorBuffer);
	for (auto i : xrange(size / sizeof(SectorBuffer))) {
		// This possibly applies IPS patches.
		readSector(sector++, work.bufs[i]);
	}
	return work.bufs[0].raw;
}

bool HD::isCacheStillValid(time_t& cacheTime)
{
	time_t fileTime = file.getModificationDate();
	bool result = fileTime == cacheTime;
	cacheTime = fileTime;
	return result;
}

SectorAccessibleDisk* HD::getSectorAccessibleDisk()
{
	return this;
}

const std::string& HD::getContainerName() const
{
	return getName();
}

bool HD::diskChanged()
{
	return false; // TODO not implemented
}

int HD::insertDisk(string_ref newDisk)
{
	try {
		switchImage(Filename(newDisk.str()));
		return 0;
	} catch (MSXException&) {
		return -1;
	}
}

// version 1: initial version
// version 2: replaced 'checksum'(=sha1) with 'tthsum`
template<typename Archive>
void HD::serialize(Archive& ar, unsigned version)
{
	Filename tmp = file.is_open() ? filename : Filename();
	ar.serialize("filename", tmp);
	if (ar.isLoader()) {
		if (tmp.empty()) {
			// Lazily open file specified in config. And close if
			// it was already opened (in the constructor). The
			// latter can occur in the following scenario:
			//  - The hd image doesn't exist yet
			//  - Reverse creates savestates, these still have
			//      tmp="" (because file=nullptr)
			//  - At some later point the hd image gets created
			//     (e.g. on first access to the image)
			//  - Now reverse to some point in EmuTime before the
			//    first disk access
			//  - The loadstate re-constructs this HD object, but
			//    because the hd image does exist now, it gets
			//    opened in the constructor (file!=nullptr).
			//  - So to get in the same state as the initial
			//    savestate we again close the file. Otherwise the
			//    checksum-check code below goes wrong.
			file.close();
		} else {
			tmp.updateAfterLoadState();
			if (filename != tmp) switchImage(tmp);
			assert(file.is_open());
		}
	}

	// store/check checksum
	if (file.is_open()) {
		bool mismatch = false;

		if (ar.versionAtLeast(version, 2)) {
			// use tiger-tree-hash
			string oldTiger = ar.isLoader() ? "" : getTigerTreeHash();
			ar.serialize("tthsum", oldTiger);
			if (ar.isLoader()) {
				string newTiger = getTigerTreeHash();
				mismatch = oldTiger != newTiger;
			}
		} else {
			// use sha1
			auto& filepool = motherBoard.getReactor().getFilePool();
			Sha1Sum oldChecksum;
			if (!ar.isLoader()) {
				oldChecksum = getSha1Sum(filepool);
			}
			string oldChecksumStr = oldChecksum.empty()
					      ? ""
					      : oldChecksum.toString();
			ar.serialize("checksum", oldChecksumStr);
			oldChecksum = oldChecksumStr.empty()
				    ? Sha1Sum()
				    : Sha1Sum(oldChecksumStr);

			if (ar.isLoader()) {
				Sha1Sum newChecksum = getSha1Sum(filepool);
				mismatch = oldChecksum != newChecksum;
			}
		}

		if (ar.isLoader() && mismatch) {
			motherBoard.getMSXCliComm().printWarning(
			    "The content of the harddisk " +
			    tmp.getResolved() +
			    " has changed since the time this savestate was "
			    "created. This might result in emulation problems "
			    "or even diskcorruption. To prevent the latter, "
			    "the harddisk is now write-protected.");
			forceWriteProtect();
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(HD);

} // namespace openmsx
