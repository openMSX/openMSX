// $Id$

#include "DiskFactory.hh"
#include "CommandController.hh"
#include "DSKDiskImage.hh"
#include "XSADiskImage.hh"
#include "RamDSKDiskImage.hh"
#include "DirAsDSK.hh"
#include "DiskPartition.hh"
#include "GlobalSettings.hh"
#include "EnumSetting.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "shared_ptr.hh"

using std::string;

namespace openmsx {

DiskFactory::DiskFactory(CommandController& controller_)
	: controller(controller_)
{
}

Disk* DiskFactory::createDisk(const string& diskImage)
{
	if (diskImage == "ramdsk") {
		return new RamDSKDiskImage();
	} else {
		Filename filename(diskImage, controller);
		try {
			// first try XSA
			return new XSADiskImage(filename);
		} catch (MSXException&) {
		try {
			// First try the fake disk, because a DSK will always
			// succeed if diskImage can be resolved
			// It is simply stat'ed, so even a directory name
			// can be resolved and will be accepted as dsk name
			// try to create fake DSK from a dir on host OS
			return new DirAsDSK(
				controller.getCliComm(),
				filename,
				controller.getGlobalSettings().getSyncDirAsDSKSetting().getValue(),
				controller.getGlobalSettings().getBootSectorSetting().getValue()
				);
		} catch (MSXException&) {
		try {
			// then try normal DSK
			return new DSKDiskImage(filename);
		} catch (MSXException& e1) {
			// Finally try to interpret the filename as
			//    <filename>:<partition-number>
			// Try this last because the ':' character could be
			// part of the filename itself. So only try this if
			// the name could not be interpreted as a valid
			// filename.
			string::size_type pos = diskImage.find_last_of(':');
			if (pos == string::npos) {
				// does not contain ':', throw previous exception
				throw;
			}
			shared_ptr<SectorAccessibleDisk> wholeDisk;
			try {
				Filename file(diskImage.substr(0, pos));
				wholeDisk.reset(new DSKDiskImage(file));
			} catch (MSXException&) {
				// If this fails we still prefer to show the
				// previous error message, because it's most
				// likely more descriptive.
				throw e1;
			}
			unsigned num = StringOp::stringToUint(
				diskImage.substr(pos + 1));
			return new DiskPartition(*wholeDisk, num, wholeDisk);
		}}}
	}
}

} // namespace openmsx
