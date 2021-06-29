#include "DiskFactory.hh"
#include "Reactor.hh"
#include "File.hh"
#include "FileContext.hh"
#include "DSKDiskImage.hh"
#include "XSADiskImage.hh"
#include "DMKDiskImage.hh"
#include "RamDSKDiskImage.hh"
#include "DirAsDSK.hh"
#include "DiskPartition.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include <memory>

using std::string;

namespace openmsx {

DiskFactory::DiskFactory(Reactor& reactor_)
	: reactor(reactor_)
	, syncDirAsDSKSetting(
		reactor.getCommandController(), "DirAsDSKmode",
		"type of synchronisation between host directory and dir-as-dsk diskimage",
		DirAsDSK::SYNC_FULL, EnumSetting<DirAsDSK::SyncMode>::Map{
			{"read_only", DirAsDSK::SYNC_READONLY},
			{"full",      DirAsDSK::SYNC_FULL}})
	, bootSectorSetting(
		reactor.getCommandController(), "bootsector",
		"boot sector type for dir-as-dsk",
		DirAsDSK::BOOTSECTOR_DOS2, EnumSetting<DirAsDSK::BootSectorType>::Map{
			{"DOS1", DirAsDSK::BOOTSECTOR_DOS1},
			{"DOS2", DirAsDSK::BOOTSECTOR_DOS2}})
{
}

std::unique_ptr<Disk> DiskFactory::createDisk(
	const string& diskImage, DiskChanger& diskChanger)
{
	if (diskImage == "ramdsk") {
		return std::make_unique<RamDSKDiskImage>();
	}

	Filename filename(diskImage, userFileContext());
	try {
		// First try DirAsDSK
		return std::make_unique<DirAsDSK>(
			diskChanger,
			reactor.getCliComm(),
			filename,
			syncDirAsDSKSetting.getEnum(),
			bootSectorSetting.getEnum());
	} catch (MSXException&) {
		// DirAsDSK didn't work, no problem
	}
	try {
		auto file = std::make_shared<File>(filename, File::PRE_CACHE);
		try {
			// first try XSA
			return std::make_unique<XSADiskImage>(filename, *file);
		} catch (MSXException&) {
			// XSA didn't work, still no problem
		}
		try {
			// next try dmk
			file->seek(0);
			return std::make_unique<DMKDiskImage>(filename, file);
		} catch (MSXException& /*e*/) {
			// DMK didn't work, still no problem
		}
		// next try normal DSK
		return std::make_unique<DSKDiskImage>(filename, std::move(file));

	} catch (MSXException& e) {
		// File could not be opened or (very rare) something is wrong
		// with the DSK image. Try to interpret the filename as
		//    <filename>:<partition-number>
		// Try this last because the ':' character could be
		// part of the filename itself. So only try this if
		// the name could not be interpreted as a valid
		// filename.
		auto pos = diskImage.find_last_of(':');
		if (pos == string::npos) {
			// does not contain ':', throw previous exception
			throw;
		}
		std::shared_ptr<SectorAccessibleDisk> wholeDisk;
		try {
			Filename filename2(diskImage.substr(0, pos));
			wholeDisk = std::make_shared<DSKDiskImage>(filename2);
		} catch (MSXException&) {
			// If this fails we still prefer to show the
			// previous error message, because it's most
			// likely more descriptive.
			throw e;
		}
		unsigned num = [&] {
			auto n = StringOp::stringToBase<10, unsigned>(std::string_view(diskImage).substr(pos + 1));
			if (!n) {
				// not a valid partion number, throw previous exception
				throw e;
			}
			return *n;
		}();
		SectorAccessibleDisk& disk = *wholeDisk;
		return std::make_unique<DiskPartition>(disk, num, std::move(wholeDisk));
	}
}

} // namespace openmsx
