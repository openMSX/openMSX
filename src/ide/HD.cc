// $Id$

#include "HD.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "XMLElement.hh"
#include "MSXCliComm.hh"
#include "MSXMotherBoard.hh"
#include "MSXException.hh"
#include "HDCommand.hh"
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

		string filename = config.getFileContext().resolveCreate(
		config.getChildData("filename"));
	try {
		file.reset(new File(filename));
	} catch (FileException& e) {
		// image didn't exist yet, create new
		file.reset(new File(filename, File::CREATE));
		file->truncate(config.getChildDataAsInt("size") * 1024 * 1024);
	}

	hdInUse[id] = true;
	hdCommand.reset(new HDCommand(motherBoard.getMSXCommandController(),
	                              motherBoard.getMSXEventDistributor(),
	                              motherBoard.getScheduler(),
	                              motherBoard.getMSXCliComm(), *this));

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

const std::string& HD::getName() const
{
	return name;
}

} // namespace openmsx
