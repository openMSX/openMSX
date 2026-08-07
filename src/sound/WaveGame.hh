#ifndef WAVEGAME_HH
#define WAVEGAME_HH

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "ResampledSoundDevice.hh"
#include "Schedulable.hh"
#include "WavData.hh"
#include "Filename.hh"

#include "outer.hh"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace openmsx {

// Emulation of the MSX Pico "Wave Game" audio cartridge.
//
// The real cartridge plays 48 kHz mono or stereo 16-bit PCM WAV files from an
// SD card in response to byte commands written to I/O port 0x92. Songs are
// named NN.wav (two-digit decimal, 00..63), with optional NN.cfg files
// giving loop and start offsets in sample counts. A 'multi.wav' +
// 'multi.cfg' pair is consulted as a fallback for song slots not backed
// by an individual NN.wav. A firmware byte at BIOS work-RAM address
// 0xF91E lets games probe the cartridge revision
// (0 = v1.49 .. v2.06, 1 = v2.07+).
//
// Port 0x92 protocol, as documented by the Pico firmware README:
//   00000000       stop music (instant on v1.49-2.06; 1s fade on v2.07+)
//   0000xxxx       fade out over xxxx seconds
//   01xxxxxx       play song xxxxxx once
//   10xxxxxx       play song xxxxxx in a loop
//   11xxxxxx       (v1.49-2.06) toggle pause, low bits ignored
//   11000000       (v2.07+)     toggle pause
//   11010000       (v2.07+)     store next command, execute after current
//                                song ends
//   11100000       (v2.07+)     continue previous song with 1s fade-in
//   11110000       (v2.07+)     play pause.wav once
//
// Detail specs from https://www.msxpico.com/downloads/WaveGameReadme.txt are:
//
// Every xx.wav file can have an optional companion xx.cfg file, which should
// contain the lines:
// 
// Loop offset (used with command 01xxxxxx)
// Optional start offset (used with command 01xxxxxx and 10xxxxxx)
// 
// When a song does not exist as xx.wav, multi.wav (with multiple songs in
// sequence) is accessed. Companion file multi.cfg is required to determine the
// start offsets for each song, and should contain the lines:
// 
// Loop offset
// Start offset for song 1
// Start offset for song 2
// Start offset for song 3
// Start offset for song 4
// Start offset for song 5
// Start offset for song 6
// Start offset for song 7
// etc
// 
// Notes:
// 
// - The .wav files need to be in the same directory as the .rom or .dsk file
// - Tests can be done in BASIC by selecting the directory with the .wav files,
//   and then exiting the MSX Pico menu by pressing <ESC>
// - When a file can't be found, the song will just not play
// - A song does not need to be stopped before a new one is started. It is
//   actually best to avoid stopping of music.
// - When a song is started, the previous song number and position are stored, so
//   these are used to continue the previous song.
// - All loop and start offsets are in decimal number of samples from the start
//   of the song.
// - When the .cfg file is missing, or the offset is missing in the .cfg file, an
//   offset of 0 is assumed.
// - Use WAV files stored as 48kHz mono, 16 bits.
// - Commands 11110000 and 11100000 are mainly used to enter/exit pause. This
//   keeps it compatible with command 11xxxxxx of older firmware versions.
// - WaveGame and Nextor can never work at the same time.

class WaveGame final : public MSXDevice
                     , public ResampledSoundDevice
{
public:
	explicit WaveGame(const DeviceConfig& config);
	~WaveGame() override;

	// MSXDevice
	void reset(EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	struct Song {
		size_t wavIdx      = 0;   // index into `wavs`
		size_t startSample = 0;   // first sample of this song in the WAV
		size_t endSample   = 0;   // one past last sample (exclusive)
		size_t loopSample  = 0;   // absolute position within the WAV to loop back to
	};

	// SoundDevice
	void generateChannels(std::span<float*> bufs, unsigned num) override;
	[[nodiscard]] float getAmplificationFactorImpl() const override;

	// Used by Cmd.
	void setSamplesDir(const Filename& dir);
	bool setSamplesDirByMediaSlot(const std::string& slot);
	void setAutoSamplesDir();

	bool assignDirFromMediaProviderIfValid(MediaProvider& mediaProvider, const std::string& mediaName);

	void clear();
	void loadSongs();
	void resetSongs();

	void handleCommand(byte value);
	void playSong(unsigned idx, bool loop);
	void continuePreviousSong();
	void stopPlayback();
	void startFadeOut(float seconds);

	void writeFirmwareByte(EmuTime time);

private:
	struct SyncFirmwareByte final : Schedulable {
		explicit SyncFirmwareByte(Scheduler& s) : Schedulable(s) {}
		void schedule(EmuTime time);
		void executeUntil(EmuTime time) override {
			auto& wg = OUTER(WaveGame, syncFirmwareByte);
			wg.writeFirmwareByte(time);
		}
	} syncFirmwareByte;

	static constexpr unsigned NUM_SONGS = 64; // 6-bit song index
	static constexpr unsigned NO_SONG = unsigned(-1);
	// Reserved index used internally for the pause.wav slot.
	static constexpr unsigned PAUSE_SONG = NUM_SONGS;

	MSXMotherBoard& motherBoard;
	Filename currentDir;
	const byte firmwareValue; // written to BIOS work RAM 0xF91E

	std::vector<WavData> wavs;        // backing WAV buffers
	std::array<std::optional<Song>, NUM_SONGS + 1> songs{}; // (last index is for PAUSE_SONG)

	// Playback state.
	struct PlayState {
		size_t   pos     = 0;       // absolute position inside the song's WAV
		bool     looping = false;
		unsigned song    = NO_SONG; // song index (or NO_SONG)
		template<typename Archive>
		void serialize(Archive& ar, unsigned version);
	};
	PlayState current;
	// Remembered state for the "continue previous" command.
	PlayState last;

	bool     paused  = false;

	// Fade. `fadeStep == 0` means no fade in progress.
	float fadeGain = 1.0f;
	float fadeStep = 0.0f;

	// Queue. 0xD0 sets `expectQueueByte`; the next write fills `queuedCmd`.
	// When the current song ends naturally, the queued byte is executed
	// and the queue cleared.
	bool expectQueueByte = false;
	std::optional<byte> queuedCmd;

	bool autoSamplesDir = true;
	std::string currentMediaProviderName;
	std::string currentMediaProviderTarget;

	struct Cmd final : Command {
		explicit Cmd(CommandController& commandController);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} command;
};

} // namespace openmsx

#endif
