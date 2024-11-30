#include "MSXMixer.hh"

#include "Mixer.hh"
#include "SoundDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXCommandController.hh"
#include "TclObject.hh"
#include "ThrottleManager.hh"
#include "GlobalSettings.hh"
#include "IntegerSetting.hh"
#include "StringSetting.hh"
#include "BooleanSetting.hh"
#include "CommandException.hh"
#include "AviRecorder.hh"
#include "Filename.hh"
#include "FileOperations.hh"
#include "MSXCliComm.hh"

#include "stl.hh"
#include "aligned.hh"
#include "enumerate.hh"
#include "inplace_buffer.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "outer.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include "view.hh"

#include <cassert>
#include <cmath>
#include <memory>
#include <tuple>

namespace openmsx {

MSXMixer::MSXMixer(Mixer& mixer_, MSXMotherBoard& motherBoard_,
                   GlobalSettings& globalSettings)
	: Schedulable(motherBoard_.getScheduler())
	, mixer(mixer_)
	, motherBoard(motherBoard_)
	, commandController(motherBoard.getMSXCommandController())
	, masterVolume(mixer.getMasterVolume())
	, speedManager(globalSettings.getSpeedManager())
	, throttleManager(globalSettings.getThrottleManager())
	, prevTime(getCurrentTime(), 44100)
	, soundDeviceInfo(commandController.getMachineInfoCommand())
{
	reschedule2();

	masterVolume.attach(*this);
	speedManager.attach(*this);
	throttleManager.attach(*this);
}

MSXMixer::~MSXMixer()
{
	if (recorder) {
		recorder->stop();
	}
	assert(infos.empty());

	throttleManager.detach(*this);
	speedManager.detach(*this);
	masterVolume.detach(*this);

	mute(); // calls Mixer::unregisterMixer()
}

MSXMixer::SoundDeviceInfo::SoundDeviceInfo(unsigned numChannels)
	: channelSettings(numChannels)
{
}

void MSXMixer::registerSound(SoundDevice& device, float volume,
                             int balance, unsigned numChannels)
{
	// TODO read volume/balance(mode) from config file
	const std::string& name = device.getName();
	SoundDeviceInfo info(numChannels);
	info.device = &device;
	info.defaultVolume = volume;
	info.volumeSetting = std::make_unique<IntegerSetting>(
		commandController, tmpStrCat(name, "_volume"),
		"the volume of this sound chip", 75, 0, 100);
	info.balanceSetting = std::make_unique<IntegerSetting>(
		commandController, tmpStrCat(name, "_balance"),
		"the balance of this sound chip", balance, -100, 100);

	info.volumeSetting->attach(*this);
	info.balanceSetting->attach(*this);

	for (auto&& [i, channelSettings] : enumerate(info.channelSettings)) {
		auto ch_name = tmpStrCat(name, "_ch", i + 1);

		channelSettings.record = std::make_unique<StringSetting>(
			commandController, tmpStrCat(ch_name, "_record"),
			"filename to record this channel to",
			std::string_view{}, Setting::Save::NO);
		channelSettings.record->attach(*this);

		channelSettings.mute = std::make_unique<BooleanSetting>(
			commandController, tmpStrCat(ch_name, "_mute"),
			"sets mute-status of individual sound channels",
			false, Setting::Save::NO);
		channelSettings.mute->attach(*this);
	}

	device.setOutputRate(getSampleRate(), speedManager.getSpeed());
	auto& i = infos.emplace_back(std::move(info));
	updateVolumeParams(i);

	commandController.getCliComm().update(CliComm::UpdateType::SOUND_DEVICE, device.getName(), "add");
}

void MSXMixer::unregisterSound(SoundDevice& device)
{
	auto it = rfind_unguarded(infos, &device, &SoundDeviceInfo::device);
	it->volumeSetting->detach(*this);
	it->balanceSetting->detach(*this);
	for (auto& s : it->channelSettings) {
		s.record->detach(*this);
		s.mute->detach(*this);
	}
	move_pop_back(infos, it);
	commandController.getCliComm().update(CliComm::UpdateType::SOUND_DEVICE, device.getName(), "remove");
}

void MSXMixer::setSynchronousMode(bool synchronous)
{
	// TODO ATM synchronous is not used anymore
	if (synchronous) {
		++synchronousCounter;
		if (synchronousCounter == 1) {
			setMixerParams(fragmentSize, hostSampleRate);
		}
	} else {
		assert(synchronousCounter > 0);
		--synchronousCounter;
		if (synchronousCounter == 0) {
			setMixerParams(fragmentSize, hostSampleRate);
		}
	}
}

double MSXMixer::getEffectiveSpeed() const
{
	return synchronousCounter ? 1.0 : speedManager.getSpeed();
}

void MSXMixer::updateStream(EmuTime::param time)
{
	unsigned count = prevTime.getTicksTill(time);
	assert(count <= 8192);
	inplace_buffer<StereoFloat, 8192> mixBuffer(uninitialized_tag{}, count);

	// call generate() even if count==0 and even if muted
	generate(mixBuffer, time);

	if (!muteCount && fragmentSize) {
		mixer.uploadBuffer(*this, mixBuffer);
	}

	if (recorder) {
		recorder->addWave(mixBuffer);
	}

	prevTime += count;
}


// Various (inner) loops that multiply one buffer by a constant and add the
// result to a second buffer. Either buffer can be mono or stereo, so if
// necessary the mono buffer is expanded to stereo. It's possible the
// accumulation buffer is still empty (as-if it contains zeros), in that case
// we skip the accumulation step.

// buf[0:n] *= f
static inline void mul(float* buf, size_t n, float f)
{
	// C++ version, unrolled 4x,
	//   this allows gcc/clang to do much better auto-vectorization
	// Note that this can process upto 3 samples too many, but that's OK.
	size_t i = 0;
	do {
		buf[i + 0] *= f;
		buf[i + 1] *= f;
		buf[i + 2] *= f;
		buf[i + 3] *= f;
		i += 4;
	} while (i < n);
}
static inline void mul(std::span<float> buf, float f)
{
	assert(!buf.empty());
	mul(buf.data(), buf.size(), f);
}
static inline void mul(std::span<StereoFloat> buf, float f)
{
	assert(!buf.empty());
	mul(&buf.data()->left, 2 * buf.size(), f);
}

// acc[0:n] += mul[0:n] * f
static inline void mulAcc(
	float* __restrict acc, const float* __restrict mul, size_t n, float f)
{
	// C++ version, unrolled 4x, see comments above.
	size_t i = 0;
	do {
		acc[i + 0] += mul[i + 0] * f;
		acc[i + 1] += mul[i + 1] * f;
		acc[i + 2] += mul[i + 2] * f;
		acc[i + 3] += mul[i + 3] * f;
		i += 4;
	} while (i < n);
}
static inline void mulAcc(std::span<float> acc, std::span<const float> mul, float f)
{
	assert(!acc.empty());
	assert(acc.size() == mul.size());
	mulAcc(acc.data(), mul.data(), acc.size(), f);
}
static inline void mulAcc(std::span<StereoFloat> acc, std::span<const StereoFloat> mul, float f)
{
	assert(!acc.empty());
	assert(acc.size() == mul.size());
	mulAcc(&acc.data()->left, &mul.data()->left, 2 * acc.size(), f);
}

// buf[0:2n+0:2] = buf[0:n] * l
// buf[1:2n+1:2] = buf[0:n] * r
static inline void mulExpand(float* buf, size_t n, float l, float r)
{
	size_t i = n;
	do {
		--i; // back-to-front
		auto t = buf[i];
		buf[2 * i + 0] = l * t;
		buf[2 * i + 1] = r * t;
	} while (i != 0);
}
static inline void mulExpand(std::span<StereoFloat> buf, float l, float r)
{
	mulExpand(&buf.data()->left, buf.size(), l, r);
}

// acc[0:2n+0:2] += mul[0:n] * l
// acc[1:2n+1:2] += mul[0:n] * r
static inline void mulExpandAcc(
	float* __restrict acc, const float* __restrict mul, size_t n,
	float l, float r)
{
	size_t i = 0;
	do {
		auto t = mul[i];
		acc[2 * i + 0] += l * t;
		acc[2 * i + 1] += r * t;
	} while (++i < n);
}
static inline void mulExpandAcc(
	std::span<StereoFloat> acc, std::span<const float> mul, float l, float r)
{
	assert(!acc.empty());
	assert(acc.size() == mul.size());
	mulExpandAcc(&acc.data()->left, mul.data(), acc.size(), l, r);
}

// buf[0:2n+0:2] = buf[0:2n+0:2] * l1 + buf[1:2n+1:2] * l2
// buf[1:2n+1:2] = buf[0:2n+0:2] * r1 + buf[1:2n+1:2] * r2
static inline void mulMix2(std::span<StereoFloat> buf, float l1, float l2, float r1, float r2)
{
	assert(!buf.empty());
	for (auto& s : buf) {
		auto t1 = s.left;
		auto t2 = s.right;
		s.left  = l1 * t1 + l2 * t2;
		s.right = r1 * t1 + r2 * t2;
	}
}

// acc[0:2n+0:2] += mul[0:2n+0:2] * l1 + mul[1:2n+1:2] * l2
// acc[1:2n+1:2] += mul[0:2n+0:2] * r1 + mul[1:2n+1:2] * r2
static inline void mulMix2Acc(
	std::span<StereoFloat> acc, std::span<const StereoFloat> mul,
	float l1, float l2, float r1, float r2)
{
	assert(!acc.empty());
	assert(acc.size() == mul.size());
	auto n = acc.size();
	size_t i = 0;
	do {
		auto t1 = mul[i].left;
		auto t2 = mul[i].right;
		acc[i].left  += l1 * t1 + l2 * t2;
		acc[i].right += r1 * t1 + r2 * t2;
	} while (++i < n);
}


// DC removal filter routines:
//
//  formula:
//     y(n) = x(n) - x(n-1) + R * y(n-1)
//  implemented as:
//     t1 = R * t0 + x(n)    mathematically equivalent, has
//     y(n) = t1 - t0        the same number of operations but
//     t0 = t1               requires only one state variable
//    see: http://en.wikipedia.org/wiki/Digital_filter#Direct_Form_I
//  with:
//     R = 1 - (2*pi * cut-off-frequency / sample-rate)
//  we take R = 511/512
//   44100Hz --> cutoff freq = 14Hz
//   22050Hz                     7Hz
static constexpr auto R = 511.0f / 512.0f;

// No new input, previous output was (non-zero) mono.
static inline float filterMonoNull(float t0, std::span<StereoFloat> out)
{
	assert(!out.empty());
	for (auto& o : out) {
		auto t1 = R * t0;
		auto s = t1 - t0;
		o.left = s;
		o.right = s;
		t0 = t1;
	}
	return t0;
}

// No new input, previous output was (non-zero) stereo.
static inline std::tuple<float, float> filterStereoNull(
	float tl0, float tr0, std::span<StereoFloat> out)
{
	assert(!out.empty());
	for (auto& o : out) {
		float tl1 = R * tl0;
		float tr1 = R * tr0;
		o.left  = tl1 - tl0;
		o.right = tr1 - tr0;
		tl0 = tl1;
		tr0 = tr1;
	}
	return {tl0, tr0};
}

// New input is mono, previous output was also mono.
static inline float filterMonoMono(
	float t0, std::span<const float> in, std::span<StereoFloat> out)
{
	assert(in.size() == out.size());
	assert(!out.empty());
	for (auto [i, o] : view::zip_equal(in, out)) {
		auto t1 = R * t0 + i;
		auto s = t1 - t0;
		o.left  = s;
		o.right = s;
		t0 = t1;
	}
	return t0;
}

// New input is mono, previous output was stereo
static inline std::tuple<float, float>
filterStereoMono(float tl0, float tr0,
                 std::span<const float> in,
                 std::span<StereoFloat> out)
{
	assert(in.size() == out.size());
	assert(!out.empty());
	for (auto [i, o] : view::zip_equal(in, out)) {
		auto tl1 = R * tl0 + i;
		auto tr1 = R * tr0 + i;
		o.left  = tl1 - tl0;
		o.right = tr1 - tr0;
		tl0 = tl1;
		tr0 = tr1;
	}
	return {tl0, tr0};
}

// New input is stereo, (previous output either mono/stereo)
static inline std::tuple<float, float>
filterStereoStereo(float tl0, float tr0,
                   std::span<const StereoFloat> in,
                   std::span<StereoFloat> out)
{
	assert(in.size() == out.size());
	assert(!out.empty());
	for (auto [i, o] : view::zip_equal(in, out)) {
		auto tl1 = R * tl0 + i.left;
		auto tr1 = R * tr0 + i.right;
		o.left  = tl1 - tl0;
		o.right = tr1 - tr0;
		tl0 = tl1;
		tr0 = tr1;
	}
	return {tl0, tr0};
}

// We have both mono and stereo input (and produce stereo output)
static inline std::tuple<float, float>
filterBothStereo(float tl0, float tr0,
                 std::span<const float> inM,
                 std::span<const StereoFloat> inS,
                 std::span<StereoFloat> out)
{
	assert(inM.size() == out.size());
	assert(inS.size() == out.size());
	assert(!out.empty());
	for (auto [im, is, o] : view::zip_equal(inM, inS, out)) {
		auto tl1 = R * tl0 + is.left  + im;
		auto tr1 = R * tr0 + is.right + im;
		o.left  = tl1 - tl0;
		o.right = tr1 - tr0;
		tl0 = tl1;
		tr0 = tr1;
	}
	return {tl0, tr0};
}

static bool approxEqual(float x, float y)
{
	constexpr float threshold = 1.0f / 32768;
	return std::abs(x - y) < threshold;
}

void MSXMixer::generate(std::span<StereoFloat> output, EmuTime::param time)
{
	// The code below is specialized for a lot of cases (before this
	// routine was _much_ shorter). This is done because this routine
	// ends up relatively high (top 5) in a profile run.
	// After these specialization this routine runs about two times
	// faster for the common cases (mono output or no sound at all).
	// In total emulation time this gave a speedup of about 2%.

	// When samples==0, call updateBuffer() but skip all further processing
	// (handling this as a special case allows to simplify the code below).
	auto samples = output.size(); // per channel
	assert(samples <= 8192);
	if (samples == 0) {
		ALIGNAS_SSE std::array<float, 4> dummyBuf;
		for (auto& info : infos) {
			bool ignore = info.device->updateBuffer(0, dummyBuf.data(), time);
			(void)ignore;
		}
		return;
	}

	// +3 to allow processing samples in groups of 4 (and upto 3 samples
	// more than requested).
	inplace_buffer<float,       8192 + 3> monoBufExtra  (uninitialized_tag{}, samples + 3);
	inplace_buffer<StereoFloat, 8192 + 3> stereoBufExtra(uninitialized_tag{}, samples + 3);
	inplace_buffer<StereoFloat, 8192 + 3> tmpBufExtra   (uninitialized_tag{}, samples + 3);
	auto* monoBufPtr   = monoBufExtra.data();
	auto* stereoBufPtr = &stereoBufExtra.data()->left;
	auto* tmpBufPtr    = &tmpBufExtra.data()->left; // can be used either for mono or stereo data
	auto monoBuf      = subspan(monoBufExtra,   0, samples);
	auto stereoBuf    = subspan(stereoBufExtra, 0, samples);
	auto tmpBufStereo = subspan(tmpBufExtra,    0, samples); // StereoFloat
	auto tmpBufMono   = std::span{tmpBufPtr, samples};      // float

	constexpr unsigned HAS_MONO_FLAG = 1;
	constexpr unsigned HAS_STEREO_FLAG = 2;
	unsigned usedBuffers = 0;

	// TODO: The Infos should be ordered such that all the mono
	// devices are handled first
	for (auto& info : infos) {
		SoundDevice& device = *info.device;
		auto l1 = info.left1;
		auto r1 = info.right1;
		if (!device.isStereo()) {
			// device generates mono output
			if (l1 == r1) {
				// no re-panning (means mono remains mono)
				if (!(usedBuffers & HAS_MONO_FLAG)) {
					// generate in 'monoBuf' (because it was still empty)
					// then multiply in-place
					if (device.updateBuffer(samples, monoBufPtr, time)) {
						usedBuffers |= HAS_MONO_FLAG;
						mul(monoBuf, l1);
					}
				} else {
					// generate in 'tmpBuf' (as mono data)
					// then multiply-accumulate into 'monoBuf'
					if (device.updateBuffer(samples, tmpBufPtr, time)) {
						mulAcc(monoBuf, tmpBufMono, l1);
					}
				}
			} else {
				// re-panning -> mono expands to different left and right result
				if (!(usedBuffers & HAS_STEREO_FLAG)) {
					// 'stereoBuf' (which is still empty) is first filled with mono-data,
					// then in-place expanded to stereo-data
					if (device.updateBuffer(samples, stereoBufPtr, time)) {
						usedBuffers |= HAS_STEREO_FLAG;
						mulExpand(stereoBuf, l1, r1);
					}
				} else {
					// 'tmpBuf' is first filled with mono-data,
					// then expanded to stereo and mul-acc into 'stereoBuf'
					if (device.updateBuffer(samples, tmpBufPtr, time)) {
						mulExpandAcc(stereoBuf, tmpBufMono, l1, r1);
					}
				}
			}
		} else {
			// device generates stereo output
			auto l2 = info.left2;
			auto r2 = info.right2;
			if (l1 == r2) {
				// no re-panning
				assert(l2 == 0.0f);
				assert(r1 == 0.0f);
				if (!(usedBuffers & HAS_STEREO_FLAG)) {
					// generate in 'stereoBuf' (because it was still empty)
					// then multiply in-place
					if (device.updateBuffer(samples, stereoBufPtr, time)) {
						usedBuffers |= HAS_STEREO_FLAG;
						mul(stereoBuf, l1);
					}
				} else {
					// generate in 'tmpBuf' (as stereo data)
					// then multiply-accumulate into 'stereoBuf'
					if (device.updateBuffer(samples, tmpBufPtr, time)) {
						mulAcc(stereoBuf, tmpBufStereo, l1);
					}
				}
			} else {
				// re-panning, this means each of the individual left or right generated
				// channels gets distributed over both the left and right output channels
				if (!(usedBuffers & HAS_STEREO_FLAG)) {
					// generate in 'stereoBuf' (because it was still empty)
					// then mix in-place
					if (device.updateBuffer(samples, stereoBufPtr, time)) {
						usedBuffers |= HAS_STEREO_FLAG;
						mulMix2(stereoBuf, l1, l2, r1, r2);
					}
				} else {
					// 'tmpBuf' is first filled with stereo-data,
					// then mixed into stereoBuf
					if (device.updateBuffer(samples, tmpBufPtr, time)) {
						mulMix2Acc(stereoBuf, tmpBufStereo, l1, l2, r1, r2);
					}
				}
			}
		}
	}

	// DC removal filter
	switch (usedBuffers) {
	case 0: // no new input
		if (approxEqual(tl0, tr0)) {
			if (approxEqual(tl0, 0.0f)) {
				// Output was zero, new input is zero,
				// after DC-filter output will still be zero.
				ranges::fill(output, StereoFloat{});
				tl0 = tr0 = 0.0f;
			} else {
				// Output was not zero, but it was the same left and right.
				tl0 = filterMonoNull(tl0, output);
				tr0 = tl0;
			}
		} else {
			std::tie(tl0, tr0) = filterStereoNull(tl0, tr0, output);
		}
		break;

	case HAS_MONO_FLAG: // only mono
		if (approxEqual(tl0, tr0)) {
			// previous output was also mono
			tl0 = filterMonoMono(tl0, monoBuf, output);
			tr0 = tl0;
		} else {
			// previous output was stereo, rarely triggers but needed for correctness
			std::tie(tl0, tr0) = filterStereoMono(tl0, tr0, monoBuf, output);
		}
		break;

	case HAS_STEREO_FLAG: // only stereo
		std::tie(tl0, tr0) = filterStereoStereo(tl0, tr0, stereoBuf, output);
		break;

	default: // mono + stereo
		std::tie(tl0, tr0) = filterBothStereo(tl0, tr0, monoBuf, stereoBuf, output);
	}
}

bool MSXMixer::needStereoRecording() const
{
	return ranges::any_of(infos, [](auto& info) {
		return info.device->isStereo() ||
		       info.balanceSetting->getInt() != 0;
	});
}

void MSXMixer::mute()
{
	if (muteCount == 0) {
		mixer.unregisterMixer(*this);
	}
	++muteCount;
}

void MSXMixer::unmute()
{
	--muteCount;
	if (muteCount == 0) {
		tl0 = tr0 = 0.0f;
		mixer.registerMixer(*this);
	}
}

void MSXMixer::reInit()
{
	prevTime.reset(getCurrentTime());
	prevTime.setPeriod(EmuDuration(getEffectiveSpeed() / double(hostSampleRate)));
	reschedule();
}
void MSXMixer::reschedule()
{
	removeSyncPoints();
	reschedule2();
}
void MSXMixer::reschedule2()
{
	unsigned size = (!muteCount && fragmentSize) ? fragmentSize : 512;
	setSyncPoint(prevTime.getFastAdd(size));
}

void MSXMixer::setMixerParams(unsigned newFragmentSize, unsigned newSampleRate)
{
	// TODO old code checked that values did actually change,
	//      investigate if this optimization is worth it
	hostSampleRate = newSampleRate;
	fragmentSize = newFragmentSize;

	reInit(); // must come before call to setOutputRate()

	for (auto& info : infos) {
		info.device->setOutputRate(newSampleRate, speedManager.getSpeed());
	}
}

void MSXMixer::setRecorder(AviRecorder* newRecorder)
{
	if ((recorder != nullptr) != (newRecorder != nullptr)) {
		setSynchronousMode(newRecorder != nullptr);
	}
	recorder = newRecorder;
}

void MSXMixer::update(const Setting& setting) noexcept
{
	if (&setting == &masterVolume) {
		updateMasterVolume();
	} else if (dynamic_cast<const IntegerSetting*>(&setting)) {
		auto it = find_if_unguarded(infos,
			[&](const SoundDeviceInfo& i) {
				return &setting == one_of(i.volumeSetting .get(),
				                          i.balanceSetting.get()); });
		updateVolumeParams(*it);
	} else if (dynamic_cast<const StringSetting*>(&setting)) {
		changeRecordSetting(setting);
	} else if (dynamic_cast<const BooleanSetting*>(&setting)) {
		changeMuteSetting(setting);
	} else {
		UNREACHABLE;
	}
}

void MSXMixer::changeRecordSetting(const Setting& setting)
{
	for (auto& info : infos) {
		for (auto&& [channel, settings] : enumerate(info.channelSettings)) {
			if (settings.record.get() == &setting) {
				info.device->recordChannel(
					unsigned(channel),
					Filename(FileOperations::expandTilde(std::string(
						 settings.record->getString()))));
				return;
			}
		}
	}
	UNREACHABLE;
}

void MSXMixer::changeMuteSetting(const Setting& setting)
{
	for (auto& info : infos) {
		for (auto&& [channel, settings] : enumerate(info.channelSettings)) {
			if (settings.mute.get() == &setting) {
				info.device->muteChannel(
					unsigned(channel), settings.mute->getBoolean());
				return;
			}
		}
	}
	UNREACHABLE;
}

void MSXMixer::update(const SpeedManager& /*speedManager*/) noexcept
{
	if (synchronousCounter == 0) {
		setMixerParams(fragmentSize, hostSampleRate);
	} else {
		// Avoid calling reInit() while recording because
		// each call causes a small hiccup in the sound (and
		// while recording this call anyway has no effect).
		// This was noticeable while sliding the speed slider
		// in catapult (because this causes many changes in
		// the speed setting).
	}
}

void MSXMixer::update(const ThrottleManager& /*throttleManager*/) noexcept
{
	//reInit();
	// TODO Should this be removed?
}

void MSXMixer::updateVolumeParams(SoundDeviceInfo& info) const
{
	int mVolume = masterVolume.getInt();
	int dVolume = info.volumeSetting->getInt();
	float volume = info.defaultVolume * narrow<float>(mVolume) * narrow<float>(dVolume) * (1.0f / (100.0f * 100.0f));
	int balance = info.balanceSetting->getInt();
	auto [l1, r1, l2, r2] = [&] {
		if (info.device->isStereo()) {
			if (balance < 0) {
				float b = (narrow<float>(balance) + 100.0f) * (1.0f / 100.0f);
				return std::tuple{
					/*l1 =*/ volume,
					/*r1 =*/ 0.0f,
					/*l2 =*/ volume * sqrtf(std::max(0.0f, 1.0f - b)),
					/*r2 =*/ volume * sqrtf(std::max(0.0f,        b))
				};
			} else {
				float b = narrow<float>(balance) * (1.0f / 100.0f);
				return std::tuple{
					/*l1 =*/ volume * sqrtf(std::max(0.0f, 1.0f - b)),
					/*r1 =*/ volume * sqrtf(std::max(0.0f,        b)),
					/*l2 =*/ 0.0f,
					/*r2 =*/ volume
				};
			}
		} else {
			// make sure that in case of rounding errors
			// we don't take sqrt() of negative numbers
			float b = (narrow<float>(balance) + 100.0f) * (1.0f / 200.0f);
			return std::tuple{
				/*l1 =*/ volume * sqrtf(std::max(0.0f, 1.0f - b)),
				/*r1 =*/ volume * sqrtf(std::max(0.0f,        b)),
				/*l2 =*/ 0.0f, // dummy
				/*r2 =*/ 0.0f  // dummy
			};
		}
	}();
	auto [ampL, ampR] = info.device->getAmplificationFactor();
	info.left1  = l1 * ampL;
	info.right1 = r1 * ampR;
	info.left2  = l2 * ampL;
	info.right2 = r2 * ampR;
}

void MSXMixer::updateMasterVolume()
{
	for (auto& p : infos) {
		updateVolumeParams(p);
	}
}

void MSXMixer::updateSoftwareVolume(SoundDevice& device)
{
	auto it = find_unguarded(infos, &device, &SoundDeviceInfo::device);
	updateVolumeParams(*it);
}

void MSXMixer::executeUntil(EmuTime::param time)
{
	updateStream(time);
	reschedule2();

	// This method gets called very regularly, typically 44100/512 = 86x
	// per second (even if sound is muted and even with sound_driver=null).
	// This rate is constant in real-time (compared to e.g. the VDP sync
	// points that are constant in EmuTime). So we can use this to
	// regularly exit from the main CPU emulation loop. Without this there
	// were problems like described in 'bug#563 Console very slow when
	// setting speed to low values like 1'.
	motherBoard.exitCPULoopSync();
}


// Sound device info

const MSXMixer::SoundDeviceInfo* MSXMixer::findDeviceInfo(std::string_view name) const
{
	auto it = ranges::find(infos, name,
		[](auto& i) { return i.device->getName(); });
	return (it != end(infos)) ? std::to_address(it) : nullptr;
}

SoundDevice* MSXMixer::findDevice(std::string_view name) const
{
	auto* info = findDeviceInfo(name);
	return info ? info->device : nullptr;
}

MSXMixer::SoundDeviceInfoTopic::SoundDeviceInfoTopic(
		InfoCommand& machineInfoCommand)
	: InfoTopic(machineInfoCommand, "sounddevice")
{
}

void MSXMixer::SoundDeviceInfoTopic::execute(
	std::span<const TclObject> tokens, TclObject& result) const
{
	auto& msxMixer = OUTER(MSXMixer, soundDeviceInfo);
	switch (tokens.size()) {
	case 2:
		result.addListElements(view::transform(
			msxMixer.infos,
			[](const auto& info) { return info.device->getName(); }));
		break;
	case 3: {
		const auto* device = msxMixer.findDevice(tokens[2].getString());
		if (!device) {
			throw CommandException("Unknown sound device");
		}
		result = device->getDescription();
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

std::string MSXMixer::SoundDeviceInfoTopic::help(std::span<const TclObject> /*tokens*/) const
{
	return "Shows a list of available sound devices.\n";
}

void MSXMixer::SoundDeviceInfoTopic::tabCompletion(std::vector<std::string>& tokens) const
{
	if (tokens.size() == 3) {
		completeString(tokens, view::transform(
			OUTER(MSXMixer, soundDeviceInfo).infos,
			[](auto& info) -> std::string_view { return info.device->getName(); }));
	}
}

} // namespace openmsx
