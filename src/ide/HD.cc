// $Id$

#include "HD.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "DeviceConfig.hh"
#include "CliComm.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "GlobalSettings.hh"
#include "MSXException.hh"
#include "HDCommand.hh"
#include "serialize.hh"
#include "memory.hh"
#include <bitset>
#include <cassert>

namespace openmsx {

using std::string;
using std::vector;

static const unsigned MAX_HD = 26;
typedef std::bitset<MAX_HD> HDInUse;

HD::HD(const DeviceConfig& config)
	: motherBoard(config.getMotherBoard())
	, name("hdX")
{
	MSXMotherBoard::SharedStuff& info =
		motherBoard.getSharedStuff("hdInUse");
	if (info.counter == 0) {
		assert(info.stuff == nullptr);
		info.stuff = new HDInUse();
	}
	++info.counter;
	auto& hdInUse = *reinterpret_cast<HDInUse*>(info.stuff);

	unsigned id = 0;
	while (hdInUse[id]) {
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
		file = make_unique<File>(filename);
		filesize = file->getSize();
		file->setFilePool(motherBoard.getReactor().getFilePool());
	} catch (FileException&) {
		// Image didn't exist yet, but postpone image creation:
		// we don't want to create images during 'testconfig'
		filesize = size_t(config.getChildDataAsInt("size")) * 1024 * 1024;
	}
	alreadyTried = false;

	hdInUse[id] = true;
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
	MSXMotherBoard::SharedStuff& info =
		motherBoard.getSharedStuff("hdInUse");
	assert(info.counter);
	assert(info.stuff);
	auto& hdInUse = *reinterpret_cast<HDInUse*>(info.stuff);

	motherBoard.getMSXCliComm().update(CliComm::HARDWARE, name, "remove");
	unsigned id = name[2] - 'a';
	assert(hdInUse[id]);
	hdInUse[id] = false;

	--info.counter;
	if (info.counter == 0) {
		assert(hdInUse.none());
		delete &hdInUse;
		info.stuff = nullptr;
	}
}

const string& HD::getName() const
{
	return name;
}

const Filename& HD::getImageName() const
{
	return filename;
}

void HD::openImage()
{
	if (file.get()) return;

	// image didn't exist yet, create new
	if (alreadyTried) {
		throw FileException("No HD image");
	}
	alreadyTried = true;
	try {
		file = make_unique<File>(filename, File::CREATE);
		file->truncate(filesize);
		file->setFilePool(motherBoard.getReactor().getFilePool());
	} catch (FileException& e) {
		motherBoard.getMSXCliComm().printWarning(
			"Couldn't create HD image: " + e.getMessage());
		throw;
	}
}

void HD::switchImage(const Filename& name)
{
	file = make_unique<File>(name);
	filename = name;
	filesize = file->getSize();
	file->setFilePool(motherBoard.getReactor().getFilePool());
	motherBoard.getMSXCliComm().update(CliComm::MEDIA, getName(),
	                                   filename.getResolved());
}

size_t HD::getNbSectorsImpl() const
{
	const_cast<HD&>(*this).openImage();
	return filesize / SECTOR_SIZE;
}

void HD::readSectorImpl(size_t sector, byte* buf)
{
	openImage();
	file->seek(sector * SECTOR_SIZE);
	file->read(buf, SECTOR_SIZE);
}

void HD::writeSectorImpl(size_t sector, const byte* buf)
{
	openImage();
	file->seek(sector * SECTOR_SIZE);
	file->write(buf, SECTOR_SIZE);
}

bool HD::isWriteProtectedImpl() const
{
	const_cast<HD&>(*this).openImage();
	return file->isReadOnly();
}

Sha1Sum HD::getSha1Sum()
{
	if (hasPatches()) {
		return SectorAccessibleDisk::getSha1Sum();
	}
	return file->getSha1Sum();
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

int HD::insertDisk(const std::string& filename)
{
	try {
		switchImage(Filename(filename));
		return 0;
	} catch (MSXException&) {
		return -1;
	}
}


template<typename Archive>
void HD::serialize(Archive& ar, unsigned /*version*/)
{
	Filename tmp = file.get() ? filename : Filename();
	ar.serialize("filename", tmp);
	if (ar.isLoader()) {
		if (tmp.empty()) {
			// lazily open file specified in config
		} else {
			tmp.updateAfterLoadState();
			switchImage(tmp);
			assert(file.get());
		}
	}

	if (file.get()) {
		Sha1Sum oldChecksum;
		if (!ar.isLoader()) {
			oldChecksum = getSha1Sum();
		}
		string oldChecksumStr = oldChecksum.empty()
		                      ? ""
		                      : oldChecksum.toString();
		ar.serialize("checksum", oldChecksumStr);
		oldChecksum = oldChecksumStr.empty()
		            ? Sha1Sum()
		            : Sha1Sum(oldChecksumStr);
		if (ar.isLoader()) {
			Sha1Sum newChecksum = getSha1Sum();
			if (oldChecksum != newChecksum) {
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
}
INSTANTIATE_SERIALIZE_METHODS(HD);

} // namespace openmsx
