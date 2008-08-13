// $Id$

#include "HD.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "XMLElement.hh"
#include "CliComm.hh"
#include "MSXMotherBoard.hh"
#include "MSXException.hh"
#include "HDCommand.hh"
#include "serialize.hh"
#include <bitset>
#include <cassert>

namespace openmsx {

using std::string;
using std::vector;

static const unsigned MAX_HD = 26;
typedef std::bitset<MAX_HD> HDInUse;

HD::HD(MSXMotherBoard& motherBoard_, const XMLElement& config)
	: motherBoard(motherBoard_)
{
	MSXMotherBoard::SharedStuff& info =
		motherBoard.getSharedStuff("hdInUse");
	if (info.counter == 0) {
		assert(info.stuff == NULL);
		info.stuff = new HDInUse();
	}
	++info.counter;
	HDInUse& hdInUse = *reinterpret_cast<HDInUse*>(info.stuff);

	unsigned id = 0;
	while (hdInUse[id]) {
		++id;
		if (id == MAX_HD) {
			throw MSXException("Too many HDs");
		}
	}
	// for exception safety, set hdInUse only at the end
	name = string("hd") + char('a' + id);

	// For the initial hd image, savestate should only try exactly this
	// (resolved) filename. For user-specified hd images (commandline or
	// via hda command) savestate will try to re-resolve the filename.
	string original = config.getChildData("filename");
	string resolved = config.getFileContext().resolveCreate(original);
	filename = Filename(resolved);
	try {
		file.reset(new File(filename));
		filesize = file->getSize();
	} catch (FileException& e) {
		// Image didn't exist yet, but postpone image creation:
		// we don't want to create images during 'testconfig'
		filesize = config.getChildDataAsInt("size") * 1024 * 1024;
	}
	alreadyTried = false;

	hdInUse[id] = true;
	hdCommand.reset(new HDCommand(motherBoard.getCommandController(),
	                              motherBoard.getMSXEventDistributor(),
	                              motherBoard.getScheduler(),
	                              *this));

	motherBoard.getMSXCliComm().update(CliComm::HARDWARE, name, "add");
}

HD::~HD()
{
	MSXMotherBoard::SharedStuff& info =
		motherBoard.getSharedStuff("hdInUse");
	assert(info.counter);
	assert(info.stuff);
	HDInUse& hdInUse = *reinterpret_cast<HDInUse*>(info.stuff);

	motherBoard.getMSXCliComm().update(CliComm::HARDWARE, name, "remove");
	unsigned id = name[2] - 'a';
	assert(hdInUse[id]);
	hdInUse[id] = false;

	--info.counter;
	if (info.counter == 0) {
		assert(hdInUse.none());
		delete &hdInUse;
		info.stuff = NULL;
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
		file.reset(new File(filename, File::CREATE));
		file->truncate(filesize);
	} catch (FileException& e) {
		motherBoard.getMSXCliComm().printWarning(
			"Couldn't create HD image: " + e.getMessage());
		throw;
	}
}

void HD::switchImage(const std::string& name_)
{
	Filename name(name_, motherBoard.getCommandController());
	file.reset(new File(name));
	filename = name;
	filesize = file->getSize();
	motherBoard.getMSXCliComm().update(CliComm::MEDIA, getName(),
	                                   filename.getResolved());
}

void HD::readFromImage(unsigned offset, unsigned size, byte* buf)
{
	openImage();
	file->seek(offset);
	file->read(buf, size);
}

void HD::writeToImage(unsigned offset, unsigned size, const byte* buf)
{
	openImage();
	file->seek(offset);
	file->write(buf, size);
}

unsigned HD::getImageSize() const
{
	const_cast<HD&>(*this).openImage();
	return filesize;
}

bool HD::isImageReadOnly()
{
	openImage();
	return file->isReadOnly();
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
			switchImage(tmp.getAfterLoadState());
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(HD);

} // namespace openmsx
