#include "DiskChanger.hh"
#include "DiskFactory.hh"
#include "DummyDisk.hh"
#include "RamDSKDiskImage.hh"
#include "DirAsDSK.hh"
#include "CommandController.hh"
#include "StateChangeDistributor.hh"
#include "Scheduler.hh"
#include "FilePool.hh"
#include "File.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "DiskManipulator.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include "CommandException.hh"
#include "CliComm.hh"
#include "TclObject.hh"
#include "EmuTime.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "serialize_constr.hh"
#include "strCat.hh"
#include "view.hh"
#include <array>
#include <functional>
#include <memory>
#include <utility>

using std::string;

namespace openmsx {

DiskChanger::DiskChanger(MSXMotherBoard& board,
                         string driveName_,
                         bool createCmd,
                         bool doubleSidedDrive_,
                         std::function<void()> preChangeCallback_)
	: reactor(board.getReactor())
	, controller(board.getCommandController())
	, stateChangeDistributor(&board.getStateChangeDistributor())
	, scheduler(&board.getScheduler())
	, preChangeCallback(std::move(preChangeCallback_))
	, driveName(std::move(driveName_))
	, doubleSidedDrive(doubleSidedDrive_)
{
	init(tmpStrCat(board.getMachineID(), "::"), createCmd);
}

DiskChanger::DiskChanger(Reactor& reactor_, string driveName_)
	: reactor(reactor_)
	, controller(reactor.getCommandController())
	, stateChangeDistributor(nullptr)
	, scheduler(nullptr)
	, driveName(std::move(driveName_))
	, doubleSidedDrive(true) // irrelevant, but needs a value
{
	init({}, true);
}

void DiskChanger::init(std::string_view prefix, bool createCmd)
{
	if (createCmd) createCommand();
	ejectDisk();
	auto& manipulator = reactor.getDiskManipulator();
	manipulator.registerDrive(*this, prefix);
	if (stateChangeDistributor) {
		stateChangeDistributor->registerListener(*this);
	}
}

void DiskChanger::createCommand()
{
	if (diskCommand) return;
	diskCommand.emplace(controller, *this);
}

DiskChanger::~DiskChanger()
{
	if (stateChangeDistributor) {
		stateChangeDistributor->unregisterListener(*this);
	}
	auto& manipulator = reactor.getDiskManipulator();
	manipulator.unregisterDrive(*this);
}

const DiskName& DiskChanger::getDiskName() const
{
	return disk->getName();
}

bool DiskChanger::diskChanged()
{
	bool ret = diskChangedFlag || disk->hasChanged();
	diskChangedFlag = false;
	return ret;
}

SectorAccessibleDisk* DiskChanger::getSectorAccessibleDisk()
{
	if (dynamic_cast<DummyDisk*>(disk.get())) {
		return nullptr;
	}
	return dynamic_cast<SectorAccessibleDisk*>(disk.get());
}

std::string_view DiskChanger::getContainerName() const
{
	return getDriveName();
}

void DiskChanger::sendChangeDiskEvent(std::span<const TclObject> args)
{
	// note: might throw MSXException
	if (stateChangeDistributor) {
		stateChangeDistributor->distributeNew<MSXCommandEvent>(
			scheduler->getCurrentTime(), args);
	} else {
		execute(args);
	}
}

void DiskChanger::signalStateChange(const StateChange& event)
{
	const auto* commandEvent = dynamic_cast<const MSXCommandEvent*>(&event);
	if (!commandEvent) return;

	execute(commandEvent->getTokens());
}

void DiskChanger::execute(std::span<const TclObject> tokens)
{
	if (tokens[0] == getDriveName()) {
		if (tokens[1] == "eject") {
			ejectDisk();
		} else {
			insertDisk(tokens); // might throw
		}
	}
}

void DiskChanger::stopReplay(EmuTime::param /*time*/) noexcept
{
	// nothing
}

int DiskChanger::insertDisk(const std::string& filename)
{
	std::array args = {TclObject("dummy"), TclObject(filename)};
	try {
		insertDisk(args);
		return 0;
	} catch (MSXException&) {
		return -1;
	}
}

void DiskChanger::insertDisk(std::span<const TclObject> args)
{
	string diskImage = FileOperations::getConventionalPath(string(args[1].getString()));
	auto& diskFactory = reactor.getDiskFactory();
	std::unique_ptr<Disk> newDisk(diskFactory.createDisk(diskImage, *this));
	for (const auto& arg : view::drop(args, 2)) {
		newDisk->applyPatch(Filename(
			arg.getString(), userFileContext()));
	}

	// no errors, only now replace original disk
	changeDisk(std::move(newDisk));
}

void DiskChanger::ejectDisk()
{
	changeDisk(std::make_unique<DummyDisk>());
}

void DiskChanger::changeDisk(std::unique_ptr<Disk> newDisk)
{
	if (preChangeCallback) preChangeCallback();
	disk = std::move(newDisk);
	diskChangedFlag = true;
	controller.getCliComm().update(CliComm::MEDIA, getDriveName(),
	                               getDiskName().getResolved());
}


// class DiskCommand

DiskCommand::DiskCommand(CommandController& commandController_,
                         DiskChanger& diskChanger_)
	: Command(commandController_, diskChanger_.driveName)
	, diskChanger(diskChanger_)
{
}

void DiskCommand::execute(std::span<const TclObject> tokens, TclObject& result)
{
	if (tokens.size() == 1) {
		result.addListElement(tmpStrCat(diskChanger.getDriveName(), ':'),
		                      diskChanger.getDiskName().getResolved());

		TclObject options;
		if (dynamic_cast<DummyDisk*>(diskChanger.disk.get())) {
			options.addListElement("empty");
		} else if (dynamic_cast<DirAsDSK*>(diskChanger.disk.get())) {
			options.addListElement("dirasdisk");
		} else if (dynamic_cast<RamDSKDiskImage*>(diskChanger.disk.get())) {
			options.addListElement("ramdsk");
		}
		if (diskChanger.disk->isWriteProtected()) {
			options.addListElement("readonly");
		}
		if (options.getListLength(getInterpreter()) != 0) {
			result.addListElement(options);
		}

	} else if (tokens[1] == "ramdsk") {
		std::array args = {TclObject(diskChanger.getDriveName()), tokens[1]};
		diskChanger.sendChangeDiskEvent(args);
	} else if (tokens[1] == "-ramdsk") {
		std::array args = {TclObject(diskChanger.getDriveName()), TclObject("ramdsk")};
		diskChanger.sendChangeDiskEvent(args);
		result = "Warning: use of '-ramdsk' is deprecated, instead use the 'ramdsk' subcommand";
	} else if (tokens[1] == "-eject") {
		std::array args = {TclObject(diskChanger.getDriveName()), TclObject("eject")};
		diskChanger.sendChangeDiskEvent(args);
		result = "Warning: use of '-eject' is deprecated, instead use the 'eject' subcommand";
	} else if (tokens[1] == "eject") {
		std::array args = {TclObject(diskChanger.getDriveName()), TclObject("eject")};
		diskChanger.sendChangeDiskEvent(args);
	} else {
		int firstFileToken = 1;
		if (tokens[1] == "insert") {
			if (tokens.size() > 2) {
				firstFileToken = 2; // skip this subcommand as filearg
			} else {
				throw CommandException("Missing argument to insert subcommand");
			}
		}
		try {
			std::vector<TclObject> args = { TclObject(diskChanger.getDriveName()) };
			for (size_t i = firstFileToken; i < tokens.size(); ++i) { // 'i' changes in loop
				std::string_view option = tokens[i].getString();
				if (option == "-ips") {
					if (++i == tokens.size()) {
						throw MSXException(
							"Missing argument for option \"", option, '\"');
					}
					args.emplace_back(tokens[i]);
				} else {
					// backwards compatibility
					args.emplace_back(option);
				}
			}
			diskChanger.sendChangeDiskEvent(args);
		} catch (FileException& e) {
			throw CommandException(std::move(e).getMessage());
		}
	}
}

string DiskCommand::help(std::span<const TclObject> /*tokens*/) const
{
	const string& driveName = diskChanger.getDriveName();
	return strCat(
		driveName, " eject             : remove disk from virtual drive\n",
		driveName, " ramdsk            : create a virtual disk in RAM\n",
		driveName, " insert <filename> : change the disk file\n",
		driveName, " <filename>        : change the disk file\n",
		driveName, "                   : show which disk image is in drive\n"
		"The following options are supported when inserting a disk image:\n"
		"-ips <filename> : apply the given IPS patch to the disk image");
}

void DiskCommand::tabCompletion(std::vector<string>& tokens) const
{
	if (tokens.size() >= 2) {
		using namespace std::literals;
		static constexpr std::array extra = {
			"eject"sv, "ramdsk"sv, "insert"sv,
		};
		completeFileName(tokens, userFileContext(), extra);
	}
}

bool DiskCommand::needRecord(std::span<const TclObject> tokens) const
{
	return tokens.size() > 1;
}

static string calcSha1(SectorAccessibleDisk* disk, FilePool& filePool)
{
	return disk ? disk->getSha1Sum(filePool).toString() : string{};
}

// version 1:  initial version
// version 2:  replaced Filename with DiskName
template<typename Archive>
void DiskChanger::serialize(Archive& ar, unsigned version)
{
	DiskName diskname = disk->getName();
	if (ar.versionBelow(version, 2)) {
		// there was no DiskName yet, just a plain Filename
		Filename filename;
		ar.serialize("disk", filename);
		if (filename.getOriginal() == "ramdisk") {
			diskname = DiskName(Filename(), "ramdisk");
		} else {
			diskname = DiskName(filename, {});
		}
	} else {
		ar.serialize("disk", diskname);
	}

	std::vector<Filename> patches;
	if constexpr (!Archive::IS_LOADER) {
		patches = disk->getPatches();
	}
	ar.serialize("patches", patches);

	auto& filePool = reactor.getFilePool();
	string oldChecksum;
	if constexpr (!Archive::IS_LOADER) {
		oldChecksum = calcSha1(getSectorAccessibleDisk(), filePool);
	}
	ar.serialize("checksum", oldChecksum);

	if constexpr (Archive::IS_LOADER) {
		diskname.updateAfterLoadState();
		string name = diskname.getResolved(); // TODO use Filename
		if (!name.empty()) {
			// Only when the original file doesn't exist on this
			// system, try to search by sha1sum. This means we
			// prefer the original file over a file with a matching
			// sha1sum (the original file may have changed). An
			// alternative is to prefer the exact sha1sum match.
			// I'm not sure which alternative is better.
			if (!FileOperations::exists(name)) {
				assert(!oldChecksum.empty());
				auto file = filePool.getFile(
					FileType::DISK, Sha1Sum(oldChecksum));
				if (file.is_open()) {
					name = file.getURL();
				}
			}
			std::vector<TclObject> args =
				{ TclObject("dummy"), TclObject(name) };
			for (auto& p : patches) {
				p.updateAfterLoadState();
				args.emplace_back(p.getResolved()); // TODO
			}

			try {
				insertDisk(args);
			} catch (MSXException& e) {
				throw MSXException(
					"Couldn't reinsert disk in drive ",
					getDriveName(), ": ", e.getMessage());
				// Alternative: Print warning and continue
				//   without diskimage. Is this better?
			}
		}

		string newChecksum = calcSha1(getSectorAccessibleDisk(), filePool);
		if (oldChecksum != newChecksum) {
			controller.getCliComm().printWarning(
				"The content of the diskimage ",
				diskname.getResolved(),
				" has changed since the time this savestate was "
				"created. This might result in emulation problems "
				"or even diskcorruption. To prevent the latter, "
				"the disk is now write-protected (eject and "
				"reinsert the disk if you want to override this).");
			disk->forceWriteProtect();
		}
	}

	// This should only be restored after disk is inserted
	ar.serialize("diskChanged", diskChangedFlag);
}

// extra (local) constructor arguments for polymorphic de-serialization
template<> struct SerializeConstructorArgs<DiskChanger>
{
	using type = std::tuple<std::string>;

	template<typename Archive>
	void save(Archive& ar, const DiskChanger& changer)
	{
		ar.serialize("driveName", changer.getDriveName());
	}

	template<typename Archive> type load(Archive& ar, unsigned /*version*/)
	{
		string driveName;
		ar.serialize("driveName", driveName);
		return {driveName};
	}
};

INSTANTIATE_SERIALIZE_METHODS(DiskChanger);
REGISTER_POLYMORPHIC_CLASS_1(DiskContainer, DiskChanger, "DiskChanger",
                             std::reference_wrapper<MSXMotherBoard>);

} // namespace openmsx
