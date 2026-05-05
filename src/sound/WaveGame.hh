#ifndef WAVEGAME_HH
#define WAVEGAME_HH

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "ResampledSoundDevice.hh"
#include "Schedulable.hh"
#include "WavData.hh"

#include "outer.hh"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace openmsx {

class WaveGameCommand;

// Emulation of the MSX Pico "Wave Game" audio cartridge.
//
// The real cartridge plays 48 kHz mono 16-bit PCM WAV files from an SD
// card in response to byte commands written to I/O port 0x92. Songs are
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
class WaveGame final : public MSXDevice
                     , public ResampledSoundDevice
                     , public MediaProvider
{
public:
	static constexpr unsigned NUM_SONGS = 64; // 6-bit song index
	static constexpr unsigned NO_SONG = unsigned(-1);
	// Reserved index used internally for the pause.wav slot.
	static constexpr unsigned PAUSE_WAV_SONG = NUM_SONGS;

	explicit WaveGame(const DeviceConfig& config);
	~WaveGame() override;

	// MSXDevice
	void reset(EmuTime time) override;
	void powerUp(EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	// MediaProvider
	void getMediaInfo(TclObject& result) override;
	void setMedia(const TclObject& info, EmuTime time) override;

	// Used by WaveGameCommand.
	void setSamplesDir(const std::string& dir);
	void eject();
	[[nodiscard]] const std::string& getSamplesDir() const { return currentDir; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	struct Song {
		size_t wavIdx     = 0;    // index into `wavs`
		size_t startSample = 0;   // first sample of this song in the WAV
		size_t endSample   = 0;   // one past last sample (exclusive)
		size_t loopSample  = 0;   // absolute position within the WAV to loop back to
		bool   hasLoop     = false;
		bool   valid       = false;
	};

	// SoundDevice
	void generateChannels(std::span<float*> bufs, unsigned num) override;
	[[nodiscard]] float getAmplificationFactorImpl() const override;

	void loadSongsFrom(const std::string& dir);
	void resetSongs();

	void handleCommand(byte value);
	void playSong(unsigned idx, bool loop);
	void playPauseWav();
	void continuePreviousSong();
	void stopPlayback();
	void startFadeOut(double seconds);
	void rememberLastPlayback();

	[[nodiscard]] const Song* currentSong() const;

	void writeFirmwareByte(EmuTime time);
	void scheduleFirmwareByte(EmuTime time);

	struct SyncFirmwareByte final : Schedulable {
		friend class WaveGame;
		explicit SyncFirmwareByte(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime time) override {
			auto& wg = OUTER(WaveGame, syncFirmwareByte);
			wg.writeFirmwareByte(time);
		}
	} syncFirmwareByte;

	MSXMotherBoard& motherBoard;
	std::string currentDir; // "" == ejected
	const byte firmwareValue; // written to BIOS work RAM 0xF91E

	std::vector<WavData> wavs;        // backing WAV buffers
	std::array<Song, NUM_SONGS> songs{};

	// pause.wav is stored outside the regular song table; see
	// PAUSE_WAV_SONG.
	Song pauseWavSong{};

	// Playback state.
	unsigned current = NO_SONG;   // song index (or PAUSE_WAV_SONG, or NO_SONG)
	size_t   playPos = 0;         // absolute position inside the song's WAV
	bool     looping = false;
	bool     paused  = false;

	// Fade. `fadeStep == 0` means no fade in progress.
	float fadeGain = 1.0f;
	float fadeStep = 0.0f;

	// Queue. 0xD0 sets `expectQueueByte`; the next write fills `queuedCmd`
	// and sets `hasQueued`. When the current song ends naturally, the
	// queued byte is executed and the queue cleared.
	bool expectQueueByte = false;
	bool hasQueued       = false;
	byte queuedCmd       = 0;

	// Remembered state for the "continue previous" command.
	unsigned lastSong    = NO_SONG;
	size_t   lastPos     = 0;
	bool     lastLooping = false;

	std::unique_ptr<WaveGameCommand> command;
};

} // namespace openmsx

#endif
