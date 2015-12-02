#ifndef MIDISESSIOnALSA_HH
#define MIDISESSIOnALSA_HH

#include <alsa/asoundlib.h>
#include <memory>


namespace openmsx {

class CliComm;
class EventDistributor;
class Scheduler;
class PluggingController;


/** Lists ALSA MIDI ports we can connect to.
  */
class MidiSessionALSA final
{
public:
	// TODO: Check which args we actually need.
	static void registerAll(
			EventDistributor& eventDistributor, Scheduler& scheduler,
			PluggingController& controller, CliComm& cliComm);

	~MidiSessionALSA();

private:
	static std::unique_ptr<MidiSessionALSA> instance;

	MidiSessionALSA(PluggingController& controller, CliComm& cliComm);

	snd_seq_t *seq;
};

} // namespace openmsx

#endif // MIDISESSIOnALSA_HH
