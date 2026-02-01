#ifndef FILEPOOL_HH
#define FILEPOOL_HH

#include "Command.hh"
#include "EventListener.hh"
#include "FilePoolCore.hh"
#include "Observer.hh"
#include "StringSetting.hh"

#include <optional>
#include <string_view>

namespace openmsx {

class CommandController;
class Reactor;
class Sha1SumCommand;

class FilePool final : private Observer<Setting>, private EventListener
{
public:
	using Result = FilePoolCore::Result;

	FilePool(CommandController& controller, Reactor& reactor);
	~FilePool();

	/** Search file with the given sha1sum.
	 * If found it returns the (already opened) file,
	 * if not found it returns nullptr.
	 */
	[[nodiscard]] Result getFile(FileType fileType, const Sha1Sum& sha1sum);

	/** Calculate sha1sum for the given File object.
	 * If possible the result is retrieved from cache, avoiding the
	 * relatively expensive calculation.
	 */
	[[nodiscard]] Sha1Sum getSha1Sum(File& file, std::string_view filename);
	[[nodiscard]] std::optional<Sha1Sum> getSha1Sum(zstring_view filename);

	[[nodiscard]] FilePoolCore::Directories getDirectories() const;

private:
	void reportProgress(std::string_view message, float fraction);

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

	// EventListener
	bool signalEvent(const Event& event) override;

private:
	FilePoolCore core;
	StringSetting filePoolSetting;
	Reactor& reactor;

	class Sha1SumCommand final : public Command {
	public:
		explicit Sha1SumCommand(CommandController& commandController);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} sha1SumCommand;

	bool quit = false;
};

} // namespace openmsx

#endif
