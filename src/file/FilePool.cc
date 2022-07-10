#include "FilePool.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "Display.hh"
#include "EventDistributor.hh"
#include "CliComm.hh"
#include "Reactor.hh"
#include "outer.hh"
#include "xrange.hh"
#include <memory>

namespace openmsx {

[[nodiscard]] static TclObject initialFilePoolSettingValue()
{
	TclObject result;

	for (const auto& p : systemFileContext().getPaths()) {
		result.addListElement(
			makeTclDict("-path", tmpStrCat(p, "/systemroms"),
			            "-types", "system_rom"),
			makeTclDict("-path", tmpStrCat(p, "/software"),
			            "-types", "rom disk tape"));
	}
	return result;
}

FilePool::FilePool(CommandController& controller, Reactor& reactor_)
	: core(FileOperations::getUserDataDir() + "/.filecache",
	       [&] { return getDirectories(); },
	       [&](std::string_view message) { reportProgress(message); })
	, filePoolSetting(
		controller, "__filepool",
		"This is an internal setting. Don't change this directly, "
		"instead use the 'filepool' command.",
		initialFilePoolSettingValue().getString())
	, reactor(reactor_)
	, sha1SumCommand(controller)
{
	filePoolSetting.attach(*this);
	reactor.getEventDistributor().registerEventListener(EventType::QUIT, *this);
}

FilePool::~FilePool()
{
	reactor.getEventDistributor().unregisterEventListener(EventType::QUIT, *this);
	filePoolSetting.detach(*this);
}

File FilePool::getFile(FileType fileType, const Sha1Sum& sha1sum)
{
	return core.getFile(fileType, sha1sum);
}

Sha1Sum FilePool::getSha1Sum(File& file)
{
	return core.getSha1Sum(file);
}

[[nodiscard]] static FileType parseTypes(Interpreter& interp, const TclObject& list)
{
	auto result = FileType::NONE;
	for (auto i : xrange(list.getListLength(interp))) {
		std::string_view elem = list.getListIndex(interp, i).getString();
		if (elem == "system_rom") {
			result |= FileType::SYSTEM_ROM;
		} else if (elem == "rom") {
			result |= FileType::ROM;
		} else if (elem == "disk") {
			result |= FileType::DISK;
		} else if (elem == "tape") {
			result |= FileType::TAPE;
		} else {
			throw CommandException("Unknown type: ", elem);
		}
	}
	return result;
}

FilePoolCore::Directories FilePool::getDirectories() const
{
	FilePoolCore::Directories result;
	try {
		auto& interp = filePoolSetting.getInterpreter();
		const TclObject& all = filePoolSetting.getValue();
		for (auto i : xrange(all.getListLength(interp))) {
			FilePoolCore::Dir dir;
			bool hasPath = false;
			dir.types = FileType::NONE;
			TclObject line = all.getListIndex(interp, i);
			unsigned numItems = line.getListLength(interp);
			if (numItems & 1) {
				throw CommandException(
					"Expected a list with an even number "
					"of elements, but got ", line.getString());
			}
			for (unsigned j = 0; j < numItems; j += 2) {
				std::string_view name  = line.getListIndex(interp, j + 0).getString();
				TclObject value = line.getListIndex(interp, j + 1);
				if (name == "-path") {
					dir.path = value.getString();
					hasPath = true;
				} else if (name == "-types") {
					dir.types = parseTypes(interp, value);
				} else {
					throw CommandException("Unknown item: ", name);
				}
			}
			if (!hasPath) {
				throw CommandException(
					"Missing -path item: ", line.getString());
			}
			if (dir.types == FileType::NONE) {
				throw CommandException(
					"Missing -types item: ", line.getString());
			}
			result.push_back(std::move(dir));
		}
	} catch (CommandException& e) {
		reactor.getCliComm().printWarning(
			"Error while parsing '__filepool' setting", e.getMessage());
	}
	return result;
}

void FilePool::update(const Setting& setting) noexcept
{
	assert(&setting == &filePoolSetting); (void)setting;
	(void)getDirectories(); // check for syntax errors
}

void FilePool::reportProgress(std::string_view message)
{
	if (quit) core.abort();
	reactor.getCliComm().printProgress(message);
	reactor.getDisplay().repaint();
}

int FilePool::signalEvent(const Event& event) noexcept
{
	(void)event; // avoid warning for non-assert compiles
	assert(getType(event) == EventType::QUIT);
	quit = true;
	return 0;
}


// class Sha1SumCommand

FilePool::Sha1SumCommand::Sha1SumCommand(
		CommandController& commandController_)
	: Command(commandController_, "sha1sum")
{
}

void FilePool::Sha1SumCommand::execute(std::span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 2, "filename");
	File file(FileOperations::expandTilde(std::string(tokens[1].getString())));
	auto& filePool = OUTER(FilePool, sha1SumCommand);
	result = filePool.getSha1Sum(file).toString();
}

std::string FilePool::Sha1SumCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return "Calculate sha1 value for the given file. If the file is "
	       "(g)zipped the sha1 is calculated on the unzipped version.";
}

void FilePool::Sha1SumCommand::tabCompletion(std::vector<std::string>& tokens) const
{
	completeFileName(tokens, userFileContext());
}

} // namespace openmsx
