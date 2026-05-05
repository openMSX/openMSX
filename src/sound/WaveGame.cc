#include "WaveGame.hh"

#include "WaveGameCommand.hh"

#include "DeviceConfig.hh"
#include "EmuDuration.hh"
#include "EmuTime.hh"
#include "FileOperations.hh"
#include "MSXCliComm.hh"
#include "MSXCPUInterface.hh"
#include "MSXException.hh"
#include "TclObject.hh"
#include "XMLElement.hh"
#include "serialize.hh"
#include "serialize_stl.hh"

#include "narrow.hh"
#include "strCat.hh"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <optional>

namespace openmsx {

static constexpr uint16_t FIRMWARE_BYTE_ADDR = 0xF91E;

// The Pico hardware runs at 48 kHz mono 16-bit PCM. Non-conforming
// files are rejected at load.
static constexpr unsigned INPUT_RATE = 48'000;
static constexpr unsigned INPUT_CHANNELS = 1;
static constexpr unsigned INPUT_BITS = 16;

// Delay before (re-)writing the BIOS work-RAM firmware byte. We must
// wait until the MSX BIOS has finished initialising its work-RAM
// (otherwise BIOS will clobber our value). One second is a comfortable
// margin on all supported machines.
static constexpr auto FIRMWARE_BYTE_DELAY = EmuDuration::sec(1);

// 1 second fade-in used by the v2.07+ "continue previous" command and
// the implicit fade applied to the bare "stop" opcode on new firmware.
static constexpr double CONTINUE_FADE_SECONDS = 1.0;

// Firmware revision values written into 0xF91E.
static constexpr byte FW_V207_PLUS = 1;

static constexpr static_string_view DESCRIPTION =
	"MSX Pico Wave Game audio cartridge, plays WAV files on I/O port 0x92.";

static constexpr std::string_view MEDIA_PROVIDER_NAME = "wavegame";

// NN.wav / NN.cfg filename for song index [0, 64).
[[nodiscard]] static std::string songFilename(unsigned idx, std::string_view ext)
{
	return strCat((idx < 10) ? "0" : "", idx, ext);
}

[[nodiscard]] static byte getFirmwareValue(const DeviceConfig& config)
{
	auto firmware = config.getChildDataAsInt("firmware", FW_V207_PLUS);
	if (firmware != 0 && firmware != FW_V207_PLUS) {
		throw MSXException("Wave Game firmware value must be 0 or 1.");
	}
	return byte(firmware);
}

WaveGame::WaveGame(const DeviceConfig& config)
	: MSXDevice(config)
	, ResampledSoundDevice(
		config.getMotherBoard(), MEDIA_PROVIDER_NAME, DESCRIPTION,
		/*numChannels=*/1, INPUT_RATE, /*stereo=*/false)
	, syncFirmwareByte(config.getMotherBoard().getScheduler())
	, motherBoard(config.getMotherBoard())
	, firmwareValue(getFirmwareValue(config))
{
	registerSound(config);

	motherBoard.registerMediaProvider(MEDIA_PROVIDER_NAME, *this);
	motherBoard.getMSXCliComm().update(
		CliComm::UpdateType::HARDWARE, MEDIA_PROVIDER_NAME, "add");

	command = std::make_unique<WaveGameCommand>(
		*this,
		motherBoard.getCommandController(),
		motherBoard.getStateChangeDistributor(),
		motherBoard.getScheduler());
}

WaveGame::~WaveGame()
{
	syncFirmwareByte.removeSyncPoints();
	motherBoard.unregisterMediaProvider(*this);
	motherBoard.getMSXCliComm().update(
		CliComm::UpdateType::HARDWARE, MEDIA_PROVIDER_NAME, "remove");
	unregisterSound();
}

void WaveGame::reset(EmuTime time)
{
	stopPlayback();
	expectQueueByte = false;
	hasQueued = false;
	queuedCmd = 0;
	paused = false;
	lastSong = NO_SONG;
	lastPos = 0;
	lastLooping = false;
	scheduleFirmwareByte(time);
}

void WaveGame::powerUp(EmuTime time)
{
	reset(time);
}

void WaveGame::writeIO(uint16_t /*port*/, byte value, EmuTime /*time*/)
{
	// 0xD0 (v2.07+) stashes the *next* write for deferred execution. The
	// latched byte is not itself interpreted further - it just arms the
	// latch.
	if (expectQueueByte) {
		expectQueueByte = false;
		if (current == NO_SONG) {
			handleCommand(value);
		} else {
			queuedCmd = value;
			hasQueued = true;
		}
		return;
	}
	handleCommand(value);
}

void WaveGame::handleCommand(byte value)
{
	const bool v207plus = firmwareValue >= FW_V207_PLUS;

	// 00000000 - stop music
	if (value == 0x00) {
		if (v207plus) {
			startFadeOut(CONTINUE_FADE_SECONDS);
		} else {
			stopPlayback();
		}
		return;
	}
	// 0000xxxx - fade out in xxxx seconds
	if ((value & 0xF0) == 0x00) {
		startFadeOut(double(value & 0x0F));
		return;
	}
	// 01xxxxxx - play song xxxxxx once
	if ((value & 0xC0) == 0x40) {
		rememberLastPlayback();
		playSong(value & 0x3F, /*loop=*/false);
		return;
	}
	// 10xxxxxx - play song xxxxxx in a loop
	if ((value & 0xC0) == 0x80) {
		rememberLastPlayback();
		playSong(value & 0x3F, /*loop=*/true);
		return;
	}
	// 11xxxxxx - firmware-specific
	if (!v207plus) {
		// v1.49 .. v2.06: any 11xxxxxx toggles pause
		paused = !paused;
		return;
	}
	switch (value) {
	case 0xC0: // toggle pause
		paused = !paused;
		break;
	case 0xD0: // queue next command
		expectQueueByte = true;
		break;
	case 0xE0: // continue previous song with 1s fade-in
		continuePreviousSong();
		break;
	case 0xF0: // play pause.wav once
		playPauseWav();
		break;
	default:
		// Unknown 11xxxxxx opcode on new firmware: ignore, just as the
		// real Pico does.
		break;
	}
}

void WaveGame::scheduleFirmwareByte(EmuTime time)
{
	syncFirmwareByte.removeSyncPoints();
	syncFirmwareByte.setSyncPoint(time + FIRMWARE_BYTE_DELAY);
}

void WaveGame::writeFirmwareByte(EmuTime time)
{
	// BIOS work RAM is in page 3, which is normally mapped to main RAM.
	// If that happens not to be the case at this instant, the write is
	// silently absorbed by whatever device is mapped there, which is
	// also what a real Pico cart would experience.
	getCPUInterface().writeMem(FIRMWARE_BYTE_ADDR, firmwareValue, time);
}

const WaveGame::Song* WaveGame::currentSong() const
{
	if (current == PAUSE_WAV_SONG) {
		return pauseWavSong.valid ? &pauseWavSong : nullptr;
	}
	if (current < songs.size() && songs[current].valid) {
		return &songs[current];
	}
	return nullptr;
}

float WaveGame::getAmplificationFactorImpl() const
{
	// The default amp factor (1/32768) scales an int16 sample to [-1, 1].
	// The PSG uses amp = 1.0 on already-normalized floats, which is why at
	// matching XML volumes the PSG is noticeably louder. Pre-multiplying
	// by 3 gives WAV playback comparable headroom without forcing users
	// to crank master + device sliders all the way up; the slider range
	// then stays useful (full-scale music stops clipping below ~70%).
	return 3.0f / 32768.0f;
}

void WaveGame::generateChannels(std::span<float*> bufs, unsigned num)
{
	assert(bufs.size() == 1);
	const auto* song = currentSong();
	if (!song || paused) {
		bufs[0] = nullptr;
		return;
	}
	const auto& wav = wavs[song->wavIdx];
	for (unsigned i = 0; i < num; ++i) {
		if (playPos >= song->endSample) {
			if (looping && song->hasLoop) {
				playPos = song->loopSample;
			} else {
				// Reached natural end of this song.
				current = NO_SONG;
				fadeGain = 1.0f;
				fadeStep = 0.0f;
				if (hasQueued) {
					byte cmd = queuedCmd;
					hasQueued = false;
					queuedCmd = 0;
					handleCommand(cmd);
				}
				// The queued command (if any) will start from the next
				// buffer. This introduces at most one buffer of silence,
				// which is below audible threshold.
				while (i < num) bufs[0][i++] = 0.0f;
				return;
			}
		}
		auto sample = narrow<float>(wav.getSample(playPos++)) * fadeGain;
		bufs[0][i] = sample;

		if (fadeStep != 0.0f) {
			fadeGain += fadeStep;
			if (fadeStep < 0.0f && fadeGain <= 0.0f) {
				// Fade-out complete.
				fadeGain = 1.0f;
				fadeStep = 0.0f;
				current = NO_SONG;
				// Also check the queue here - some titles fade-then-queue.
				if (hasQueued) {
					byte cmd = queuedCmd;
					hasQueued = false;
					queuedCmd = 0;
					handleCommand(cmd);
				}
				while (++i < num) bufs[0][i] = 0.0f;
				return;
			}
			if (fadeStep > 0.0f && fadeGain >= 1.0f) {
				// Fade-in complete.
				fadeGain = 1.0f;
				fadeStep = 0.0f;
			}
		}
	}
}

void WaveGame::resetSongs()
{
	wavs.clear();
	for (auto& s : songs) s = Song{};
	pauseWavSong = Song{};
}

void WaveGame::rememberLastPlayback()
{
	// Only regular songs are eligible for "continue previous" - the
	// pause.wav jingle is explicitly excluded.
	if (current < songs.size() && songs[current].valid) {
		lastSong = current;
		lastPos = playPos;
		lastLooping = looping;
	}
}

void WaveGame::playSong(unsigned idx, bool loop)
{
	if (idx >= songs.size() || !songs[idx].valid) {
		// Real cart is silent for missing slots.
		current = NO_SONG;
		playPos = 0;
		looping = false;
		fadeGain = 1.0f;
		fadeStep = 0.0f;
		return;
	}
	current = idx;
	playPos = songs[idx].startSample;
	looping = loop;
	paused = false;
	fadeGain = 1.0f;
	fadeStep = 0.0f;
}

void WaveGame::playPauseWav()
{
	if (!pauseWavSong.valid) return;
	rememberLastPlayback();
	current = PAUSE_WAV_SONG;
	playPos = pauseWavSong.startSample;
	looping = false;
	paused = false;
	fadeGain = 1.0f;
	fadeStep = 0.0f;
}

void WaveGame::continuePreviousSong()
{
	if (lastSong >= songs.size() || !songs[lastSong].valid) return;
	current = lastSong;
	playPos = lastPos;
	looping = lastLooping;
	paused = false;
	fadeGain = 0.0f;
	fadeStep = 1.0f / float(CONTINUE_FADE_SECONDS * INPUT_RATE);
}

void WaveGame::stopPlayback()
{
	current = NO_SONG;
	playPos = 0;
	looping = false;
	paused = false;
	fadeGain = 1.0f;
	fadeStep = 0.0f;
}

void WaveGame::startFadeOut(double seconds)
{
	if (seconds <= 0.0 || current == NO_SONG) {
		stopPlayback();
		return;
	}
	// Preserve current playback state; the fade runs in generateChannels.
	fadeGain = std::max(fadeGain, 0.0f);
	fadeStep = -1.0f / float(seconds * INPUT_RATE);
}

// Parse a per-song NN.cfg. Format:
//   line 1: loop offset     (sample count)
//   line 2: start offset    (sample count, optional)
// Returns true iff at least the loop offset was read. `startOff` is
// left at 0 if only one line is present.
[[nodiscard]] static bool parseCfg(const std::string& path,
                                   size_t& loopOff, size_t& startOff)
{
	std::ifstream ifs(path);
	if (!ifs) return false;
	long long tmp = 0;
	if (!(ifs >> tmp)) return false;
	loopOff = (tmp < 0) ? 0 : size_t(tmp);
	if (ifs >> tmp) {
		startOff = (tmp < 0) ? 0 : size_t(tmp);
	} else {
		startOff = 0;
	}
	return true;
}

// Parse multi.cfg. Format:
//   line 1: loop offset (sample count, absolute within multi.wav)
//   line 2+: start offset for song 0, 1, 2, ...
// Absent or unreadable files leave `loopOff` at 0 and `starts` empty,
// which matches the Pico's behaviour for a multi.wav without cfg.
static bool parseMultiCfg(const std::string& path,
                          size_t& loopOff,
                          std::vector<size_t>& starts)
{
	std::ifstream ifs(path);
	if (!ifs) return false;
	long long tmp = 0;
	if (!(ifs >> tmp)) return false;
	loopOff = (tmp < 0) ? 0 : size_t(tmp);
	while (ifs >> tmp) {
		starts.push_back((tmp < 0) ? 0 : size_t(tmp));
	}
	return true;
}

static bool isValidWaveGameWav(const WavData& wav)
{
	return (wav.getFreq() == INPUT_RATE) &&
	       (wav.getChannels() == INPUT_CHANNELS) &&
	       (wav.getBits() == INPUT_BITS);
}

static void printInvalidWavWarning(MSXCliComm& cli, std::string_view filename, const WavData& wav)
{
	cli.printWarning(
		"Wave Game: ", filename, " is ",
		wav.getFreq(), " Hz, ", wav.getChannels(), " channel(s), ",
		wav.getBits(), "-bit; expected ",
		INPUT_RATE, " Hz, mono, 16-bit, skipping.");
}

void WaveGame::loadSongsFrom(const std::string& dir)
{
	stopPlayback();
	resetSongs();

	auto& cli = motherBoard.getMSXCliComm();

	// pause.wav (optional; v2.07+ plays it on 0xF0).
	{
		auto path = FileOperations::join(dir, "pause.wav");
		if (FileOperations::isRegularFile(path)) {
			try {
				WavData w(path);
				if (!isValidWaveGameWav(w)) {
					printInvalidWavWarning(cli, "pause.wav", w);
				} else {
					size_t sz = w.getSize();
					wavs.push_back(std::move(w));
					pauseWavSong = Song{
						.wavIdx      = wavs.size() - 1,
						.startSample = 0,
						.endSample   = sz,
						.loopSample  = 0,
						.hasLoop     = false,
						.valid       = true,
					};
				}
			} catch (const MSXException& e) {
				cli.printWarning(
					"Wave Game: pause.wav load failed: ",
					e.getMessage());
			}
		}
	}

	// Try multi.wav + multi.cfg so its indices can fill gaps between
	// individual NN.wav files.
	std::optional<size_t> multiIdx;
	size_t multiLoop = 0;
	std::vector<size_t> multiStarts;
	{
		auto mwav = FileOperations::join(dir, "multi.wav");
		if (FileOperations::isRegularFile(mwav)) {
			try {
				WavData w(mwav);
				if (!isValidWaveGameWav(w)) {
					printInvalidWavWarning(cli, "multi.wav", w);
				} else {
					auto mcfg = FileOperations::join(dir, "multi.cfg");
					parseMultiCfg(mcfg, multiLoop, multiStarts);
					wavs.push_back(std::move(w));
					multiIdx = wavs.size() - 1;
				}
			} catch (const MSXException& e) {
				cli.printWarning(
					"Wave Game: multi.wav load failed: ",
					e.getMessage());
			}
		}
	}

	// Individual NN.wav.
	for (unsigned i = 0; i < NUM_SONGS; ++i) {
		auto path = FileOperations::join(dir, songFilename(i, ".wav"));
		if (!FileOperations::isRegularFile(path)) continue;
		try {
			WavData w(path);
			auto filename = songFilename(i, ".wav");
			if (!isValidWaveGameWav(w)) {
				printInvalidWavWarning(cli, filename, w);
				continue;
			}
			size_t sz = w.getSize();

			auto cfgPath = FileOperations::join(dir, songFilename(i, ".cfg"));
			size_t loopOff = 0;
			size_t startOff = 0;
			bool hasLoop = parseCfg(cfgPath, loopOff, startOff);
			loopOff  = std::min(loopOff,  sz);
			startOff = std::min(startOff, sz);

			wavs.push_back(std::move(w));
			songs[i] = Song{
				.wavIdx      = wavs.size() - 1,
				.startSample = startOff,
				.endSample   = sz,
				.loopSample  = loopOff,
				.hasLoop     = hasLoop,
				.valid       = true,
			};
		} catch (const MSXException& e) {
			cli.printWarning(
				"Wave Game: ", songFilename(i, ".wav"),
				" load failed: ", e.getMessage());
		}
	}

	// Fill remaining slots from multi.wav using multi.cfg offsets.
	if (multiIdx) {
		const size_t mSize = wavs[*multiIdx].getSize();
		for (unsigned i = 0; i < NUM_SONGS; ++i) {
			if (songs[i].valid) continue;
			if (i >= multiStarts.size()) break;
			size_t start = std::min(multiStarts[i], mSize);
			size_t end   = (i + 1 < multiStarts.size())
			             ? std::min(multiStarts[i + 1], mSize)
			             : mSize;
			if (start >= end) continue;
			songs[i] = Song{
				.wavIdx      = *multiIdx,
				.startSample = start,
				.endSample   = end,
				.loopSample  = std::min(multiLoop, mSize),
				.hasLoop     = multiLoop != 0,
				.valid       = true,
			};
		}
	}
}

void WaveGame::getMediaInfo(TclObject& result)
{
	result.addDictKeyValues("target", currentDir,
	                        "type", "directory");
}

void WaveGame::setMedia(const TclObject& info, EmuTime /*time*/)
{
	auto target = info.getOptionalDictValue(TclObject("target"));
	if (!target) return;
	auto trgt = target->getString();
	if (trgt.empty()) {
		eject();
	} else {
		setSamplesDir(std::string(trgt));
	}
}

void WaveGame::setSamplesDir(const std::string& dir)
{
	currentDir = dir;
	loadSongsFrom(dir);
}

void WaveGame::eject()
{
	currentDir.clear();
	stopPlayback();
	resetSongs();
}

template<typename Archive>
void WaveGame::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("currentDir", currentDir);
	if constexpr (Archive::IS_LOADER) {
		if (!currentDir.empty()) {
			loadSongsFrom(currentDir);
		}
	}
	ar.serialize("current",         current,
	             "playPos",         playPos,
	             "looping",         looping,
	             "paused",          paused,
	             "fadeGain",        fadeGain,
	             "fadeStep",        fadeStep,
	             "expectQueueByte", expectQueueByte,
	             "hasQueued",       hasQueued,
	             "queuedCmd",       queuedCmd,
	             "lastSong",        lastSong,
	             "lastPos",         lastPos,
	             "lastLooping",     lastLooping);
}
INSTANTIATE_SERIALIZE_METHODS(WaveGame);
REGISTER_MSXDEVICE(WaveGame, "WaveGame");

} // namespace openmsx
