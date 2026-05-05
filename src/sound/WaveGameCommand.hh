#ifndef WAVEGAMECOMMAND_HH
#define WAVEGAMECOMMAND_HH

#include "RecordedCommand.hh"

namespace openmsx {

class WaveGame;
class CommandController;
class StateChangeDistributor;
class Scheduler;

class WaveGameCommand final : public RecordedCommand
{
public:
	WaveGameCommand(WaveGame& waveGame,
	                CommandController& commandController,
	                StateChangeDistributor& stateChangeDistributor,
	                Scheduler& scheduler);

	void execute(std::span<const TclObject> tokens, TclObject& result,
	             EmuTime time) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
	[[nodiscard]] bool needRecord(std::span<const TclObject> tokens) const override;

private:
	WaveGame& waveGame;
};

} // namespace openmsx

#endif
