#include "WaveGame.hh"

#include "DeviceConfig.hh"
#include "EmuDuration.hh"
#include "EmuTime.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "MSXCliComm.hh"
#include "MSXCPUInterface.hh"
#include "MSXException.hh"
#include "TclObject.hh"
#include "XMLElement.hh"
#include "serialize.hh"
#include "serialize_stl.hh"

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
static constexpr unsigned INPUT_BITS = 16;

// Delay before (re-)writing the BIOS work-RAM firmware byte. We must
// wait until the MSX BIOS has finished initialising its work-RAM
// (otherwise BIOS will clobber our value). One second is a comfortable
// margin on all supported machines.
static constexpr auto FIRMWARE_BYTE_DELAY = EmuDuration::sec(1);

// 1 second fade-in used by the v2.07+ "continue previous" command and
// the implicit fade applied to the bare "stop" opcode on new firmware.
static constexpr float CONTINUE_FADE_SECONDS = 1.0;

// Firmware revision values written into 0xF91E.
static constexpr byte FW_V207_PLUS = 1;

// NN.wav / NN.cfg filename for song index [0, 64).
[[nodiscard]] static std::string songFilename(unsigned idx, std::string_view ext)
{
	return strCat((idx < 10) ? "0" : "", idx, ext);
}

[[nodiscard]] static byte getFirmwareValue(const DeviceConfig& config)
{
	auto firmware = config.getChildDataAsInt("firmware", FW_V207_PLUS);
	if (firmware != one_of(0, FW_V207_PLUS)) {
		throw MSXException("Wave Game firmware value must be 0 or 1.");
	}
	return byte(firmware);
}

WaveGame::WaveGame(const DeviceConfig& config)
	: MSXDevice(config)
	, ResampledSoundDevice(
		config.getMotherBoard(), "wavegame",
		"MSX-Pico Wave Game device, plays WAV files triggered by MSX software.",
		/*numChannels=*/1, INPUT_RATE, /*stereo=*/false)
	, syncFirmwareByte(config.getMotherBoard().getScheduler())
	, motherBoard(config.getMotherBoard())
	, firmwareValue(getFirmwareValue(config))
	, command(motherBoard.getCommandController())
{
	registerSound(config);
	setAutoSamplesDir();
}

WaveGame::~WaveGame()
{
	unregisterSound();
}

void WaveGame::reset(EmuTime time)
{
	stopPlayback();
	expectQueueByte = false;
	queuedCmd = {};
	last = {};
	syncFirmwareByte.schedule(time);
}

void WaveGame::writeIO(uint16_t /*port*/, byte value, EmuTime /*time*/)
{
	// 0xD0 (v2.07+) stashes the *next* write for deferred execution. The
	// latched byte is not itself interpreted further - it just arms the
	// latch.
	if (expectQueueByte) {
		expectQueueByte = false;
		if (current.song != NO_SONG) {
			queuedCmd = value;
			return;
		}
	}
	handleCommand(value);
}

void WaveGame::handleCommand(byte value)
{
	const bool v207plus = firmwareValue >= FW_V207_PLUS;

	// 00000000 - stop music
	if (value == 0) {
		if (v207plus) {
			startFadeOut(CONTINUE_FADE_SECONDS);
		} else {
			stopPlayback();
		}
		return;
	}
	// 0000xxxx - fade out in xxxx seconds
	if ((value & 0b1111'0000) == 0) {
		startFadeOut(float(value & 0x0F));
		return;
	}
	// 01xxxxxx - play song xxxxxx once
	if ((value & 0b1100'0000) == 0b0100'0000) {
		playSong(value & 0x3F, /*loop=*/false);
		return;
	}
	// 10xxxxxx - play song xxxxxx in a loop
	if ((value & 0b1100'0000) == 0b1000'0000) {
		playSong(value & 0x3F, /*loop=*/true);
		return;
	}
	// 11xxxxxx - firmware-specific
	if (!v207plus && (value & 0b1100'0000) == 0b1100'0000) {
		// v1.49 .. v2.06: any 11xxxxxx toggles pause
		paused = !paused;
		return;
	}
	switch (value) {
	case 0b1100'0000: // toggle pause
		paused = !paused;
		break;
	case 0b1101'0000: // queue next command
		expectQueueByte = true;
		break;
	case 0b1110'0000: // continue previous song with 1s fade-in
		continuePreviousSong();
		break;
	case 0b1111'0000: // play pause.wav once
		playSong(PAUSE_SONG, false);
		break;
	default:
		// Unknown 11xxxxxx opcode on new firmware: ignore, just as the
		// real Pico does.
		break;
	}
}

void WaveGame::SyncFirmwareByte::schedule(EmuTime time)
{
	removeSyncPoints();
	setSyncPoint(time + FIRMWARE_BYTE_DELAY);
}

void WaveGame::writeFirmwareByte(EmuTime time)
{
	// BIOS work RAM is in page 3, which is normally mapped to main RAM.
	// If that happens not to be the case at this instant, the write is
	// silently absorbed by whatever device is mapped there, which is
	// also what a real Pico cart would experience.
	getCPUInterface().writeMem(FIRMWARE_BYTE_ADDR, firmwareValue, time);
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
	
	if (current.song == NO_SONG || paused) {
		bufs[0] = nullptr;
		return;
	}
	assert(songs[current.song]);

	auto& song = songs[current.song];
	auto* wav = &wavs[song->wavIdx];
	for (unsigned i = 0; i < num; ++i) {

		auto handleEndOfSong = [&] {
			stopPlayback();
			if (queuedCmd) {
				handleCommand(*std::exchange(queuedCmd, {}));
			}
			if (current.song == NO_SONG || paused) {
				// If we're not playing anymore, fill the buf with zeroes.
				while (i < num) bufs[0][i++] = 0.0f;
				return true; // finished
			}
			// update our pointers
			song = songs[current.song];
			wav = &wavs[song->wavIdx];
			return false;
		};

		if (current.pos >= song->endSample) {
			if (current.looping) {
				current.pos = song->loopSample;
			} else {
				// Reached natural end of this song.
				if (handleEndOfSong()) return;
			}
		}
		auto sample = float(wav->getSample(current.pos++)) * fadeGain;
		bufs[0][i] = sample;

		if (fadeStep != 0.0f) {
			fadeGain += fadeStep;
			if (fadeStep < 0.0f && fadeGain <= 0.0f) {
				// Fade-out complete.
				if (handleEndOfSong()) return;
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
	stopPlayback();
	last = {};
	wavs.clear();
	std::ranges::fill(songs, Song{});
}

void WaveGame::playSong(unsigned idx, bool loop)
{
	assert(idx != NO_SONG);

	// Remember last playback.
	// Only regular songs are eligible for "continue previous" - the
	// pause.wav jingle is explicitly excluded.
	// When the same song is started as the current song, it is not
	// remembered as previous song.
	if (current.song != one_of(NO_SONG, PAUSE_SONG, idx)) {
		assert(songs[current.song]);
		last = current;
	}

	// if we're in auto dir mode, check for possible media change
	if (autoSamplesDir) {
		auto needRefresh = [&] {
			if (auto* mediaProvider = motherBoard.findMediaProvider(currentMediaProviderName)) {
				// still exists, does it still have the same target?
				TclObject result;
				mediaProvider->getMediaInfo(result);
				if (auto target = result.getOptionalDictValue(TclObject("target"))) {
					if (target->getString() == currentMediaProviderTarget) return false;
				}
			}
			return true;
		}();
		if (needRefresh) setAutoSamplesDir();
	}

	if (!songs[idx]) {
		// Real cart is silent for missing slots.
		stopPlayback();
		return;
	}
	current = { .pos     = songs[idx]->startSample,
		    .looping = loop,
		    .song    = idx};
	paused = false;
	fadeGain = 1.0f;
	fadeStep = 0.0f;
}

void WaveGame::continuePreviousSong()
{
	if (last.song == PAUSE_SONG) return;
	assert(songs[last.song]);
	// note that if last.song == NO_SONG, current.song will now also be NO_SONG
	current = last;
	paused = false;
	fadeGain = 0.0f;
	fadeStep = 1.0f / float(CONTINUE_FADE_SECONDS * INPUT_RATE);
}

void WaveGame::stopPlayback()
{
	current = {};
	paused = false;
	fadeGain = 1.0f;
	fadeStep = 0.0f;
}

void WaveGame::startFadeOut(float seconds)
{
	if (current.song == NO_SONG) return;
	// Preserve current playback state; the fade runs in generateChannels.
	fadeStep = -1.0f / float(seconds * INPUT_RATE);
}

// Parse a per-song NN.cfg. Format:
//   line 1: loop offset     (sample count)
//   line 2: start offset    (sample count, optional)
//  Returns pair with loopOffset and startOffset, respectively
static std::pair<size_t, size_t> parseCfg(const std::string& path)
{
	std::ifstream ifs(path);
	if (!ifs) return {0, 0};
	long long tmp = 0;
	if (!(ifs >> tmp)) return {0, 0};
	auto loopOff = size_t(std::max(tmp, 0ll));
	size_t startOff = 0;
	if (ifs >> tmp) {
		startOff = size_t(std::max(tmp, 0ll));
	}
	return {loopOff, startOff};
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
	loopOff = size_t(std::max(tmp, 0ll));
	while (ifs >> tmp) {
		starts.push_back((tmp < 0) ? 0 : size_t(tmp));
	}
	return true;
}

static bool isValidWaveGameWav(const WavData& wav)
{
	return wav.getFreq() == INPUT_RATE &&
	       wav.getBits() == INPUT_BITS &&
	       wav.getSize() > 0;
}

static void printInvalidWavWarning(MSXCliComm& cli, std::string_view filename, const WavData& wav)
{
	cli.printWarning(
		"Wave Game: ", filename, " is ",
		wav.getFreq(), " Hz, ", wav.getBits(), "-bit, ",
		wav.getSize(), " samples in size; expected ",
		INPUT_RATE, " Hz, 16-bit, size > 0, skipping.");
}

void WaveGame::loadSongs()
{
	resetSongs();

	auto& cli = motherBoard.getMSXCliComm();

	// Try multi.wav + multi.cfg so its indices can fill gaps between
	// individual NN.wav files.
	std::optional<size_t> multiIdx;
	size_t multiLoop = 0;
	std::vector<size_t> multiStarts;
	{
		auto mwav = FileOperations::join(currentDir.getResolved(), "multi.wav");
		if (FileOperations::isRegularFile(mwav)) {
			try {
				WavData w(mwav);
				if (!isValidWaveGameWav(w)) {
					printInvalidWavWarning(cli, "multi.wav", w);
				} else {
					auto mcfg = FileOperations::join(currentDir.getResolved(), "multi.cfg");
					parseMultiCfg(mcfg, multiLoop, multiStarts);
					multiIdx = wavs.size();
					wavs.push_back(std::move(w));
				}
			} catch (const MSXException& e) {
				cli.printWarning(
					"Wave Game: multi.wav load failed: ",
					e.getMessage());
			}
		}
	}

	// Individual NN.wav and pause.wav (optional; v2.07+ plays it on 0xF0).
	for (unsigned i = 0; i < NUM_SONGS + 1; ++i) {
		auto filename = i == PAUSE_SONG ? "pause.wav" : songFilename(i, ".wav");
		auto path = FileOperations::join(currentDir.getResolved(), filename);
		if (!FileOperations::isRegularFile(path)) continue;
		try {
			WavData w(path);
			if (!isValidWaveGameWav(w)) {
				printInvalidWavWarning(cli, filename, w);
				continue;
			}
			const auto sz = w.getSize();

			size_t loopOff = 0;
			size_t startOff = 0;
			if (i != PAUSE_SONG) {
				auto cfgPath = FileOperations::join(currentDir.getResolved(), songFilename(i, ".cfg"));
				std::tie(loopOff, startOff) = parseCfg(cfgPath);
			}
			loopOff  = std::min(loopOff,  sz - 1);
			startOff = std::min(startOff, sz - 1);

			wavs.push_back(std::move(w));
			songs[i] = Song{
				.wavIdx      = wavs.size() - 1,
				.startSample = startOff,
				.endSample   = sz,
				.loopSample  = loopOff,
			};
		} catch (const MSXException& e) {
			cli.printWarning(
				"Wave Game: ", filename,
				" load failed: ", e.getMessage());
		}
	}

	// Fill remaining slots from multi.wav using multi.cfg offsets.
	if (multiIdx) {
		const auto sz = wavs[*multiIdx].getSize();
		for (unsigned i = 0; i < NUM_SONGS; ++i) {
			if (songs[i]) continue;
			if (i >= multiStarts.size()) break;
			size_t start = std::min(multiStarts[i], sz - 1); // TODO: warn?
			if (start >= sz) continue; // TODO: warn?
			songs[i] = Song{
				.wavIdx      = *multiIdx,
				.startSample = start,
				.endSample   = sz,
				.loopSample  = std::min(multiLoop, sz),
			};
		}
	}
}

bool WaveGame::assignDirFromMediaProviderIfValid(/*const*/MediaProvider& mediaProvider, const std::string& mediaName) // getMedia info is non-const...
{
	TclObject result;
	mediaProvider.getMediaInfo(result);
	if (auto target = result.getOptionalDictValue(TclObject("target"))) {
		auto targetString = target->getString();
		if (targetString.empty()) return false;
		auto dirName = FileOperations::getDirName(target->getString());
		if (FileOperations::isRegularFile(FileOperations::join(dirName, "01.wav")) ||
		    FileOperations::isRegularFile(FileOperations::join(dirName, "multi.wav"))) {
			currentDir = Filename(dirName);
			currentMediaProviderName = mediaName;
			currentMediaProviderTarget = targetString;
			loadSongs();
			return true;
		}
	}
	return false;
}

void WaveGame::setAutoSamplesDir()
{
	clear();
	autoSamplesDir = true;
	currentMediaProviderName.clear();

	for (const auto& mediaProvider: motherBoard.getMediaProviders()) {
		if (assignDirFromMediaProviderIfValid(*mediaProvider.provider, std::string(mediaProvider.name))) return;
	}
}

bool WaveGame::setSamplesDirByMediaSlot(const std::string& slot)
{
	autoSamplesDir = false;
	if (auto* mediaProvider = motherBoard.findMediaProvider(slot)) {
		// this is a known media provider!
		if (assignDirFromMediaProviderIfValid(*mediaProvider, slot)) return true;
	}
	return false;
}

void WaveGame::setSamplesDir(const Filename& dir)
{
	autoSamplesDir = false;
	currentMediaProviderName.clear();
	currentDir = dir;
	loadSongs();
}

void WaveGame::clear()
{
	autoSamplesDir = false;
	currentDir = {};
	resetSongs();
}


WaveGame::Cmd::Cmd(CommandController& commandController_)
	: Command(commandController_, "wavegame")
{
}

void WaveGame::Cmd::execute(
	std::span<const TclObject> tokens, TclObject& result)
{
	auto& waveGame = OUTER(WaveGame, command);
	if (tokens.size() == 1) {
		result = waveGame.autoSamplesDir ? "auto" : std::string_view{waveGame.currentDir.getResolved()};

	} else if (tokens.size() == 2) {
		if (tokens[1] == "auto") {
			waveGame.setAutoSamplesDir();
		} else {
			// user specified an explicit media provider or directory
			std::string name(tokens[1].getString());
			// media provider?
			if (!waveGame.setSamplesDirByMediaSlot(name)) {
				// assume it's a filename
				try {
					auto dir = Filename(name, userFileContext());
					if (!FileOperations::isDirectory(dir.getResolved())) {
						throw CommandException(
							"Wave Game expects a directory, not a file: ", name);
					}
					waveGame.setSamplesDir(dir);
				} catch (MSXException& e) {
					throw CommandException(std::move(e).getMessage());
				}
			}
		}
	} else {
		throw SyntaxError();
	}
}

std::string WaveGame::Cmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "wavegame             : show current samples directory (or 'auto')\n"
	       "wavegame auto        : automatically find samples directory with NN.wav / NN.cfg files based on inserted media\n"
	       "wavegame <mediaslot> : use the samples directory with NN.wav / NN.cfg files based on inserted media of the given slot ([carta|cartb|diska|...])\n"
	       "wavegame <path>      : set samples directory with NN.wav / NN.cfg files\n";
}

void WaveGame::Cmd::tabCompletion(std::vector<std::string>& tokens) const
{
	if (tokens.size() == 2) {
		auto& waveGame = OUTER(WaveGame, command);
		auto extra = to_vector(std::views::transform(waveGame.motherBoard.getMediaProviders(), &MediaProviderInfo::name));
		extra.push_back("auto"sv);
		completeFileName(tokens, userFileContext(), extra);
	}
}

template<typename Archive>
void WaveGame::PlayState::serialize(Archive& ar, unsigned /*version*/)
{
    ar.serialize("pos",     pos,
		 "looping", looping,
		 "song",    song);
}

template<typename Archive>
void WaveGame::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("currentDir", currentDir);
	ar.serialize("autoSamplesDir", autoSamplesDir);
	if constexpr (Archive::IS_LOADER) {
		if (autoSamplesDir) {
			setAutoSamplesDir();
		} else {
			currentDir.updateAfterLoadState();
			if (!currentDir.empty()) {
				loadSongs();
			}
		}
	}
	ar.serialize("current",         current,
	             "paused",          paused,
	             "fadeGain",        fadeGain,
	             "fadeStep",        fadeStep,
	             "last",            last,
	             "expectQueueByte", expectQueueByte,
	             "queuedCmd",       queuedCmd);
}
INSTANTIATE_SERIALIZE_METHODS(WaveGame);
REGISTER_MSXDEVICE(WaveGame, "WaveGame");

} // namespace openmsx
