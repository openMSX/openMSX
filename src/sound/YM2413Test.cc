#include "YM2413Okazaki.hh"
#include "YM2413Burczynski.hh"
#include "WavWriter.hh"
#include "WavData.hh"
#include "Filename.hh"
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>

using namespace std;
using namespace openmsx;


// global vars
string coreName;
string testName;


static constexpr unsigned CHANNELS = 11;


struct RegWrite
{
	RegWrite(byte reg_, byte val_) : reg(reg_), val(val_) {}
	byte reg;
	byte val;
};
using RegWrites = vector<RegWrite>;
struct LogEvent
{
	vector<RegWrite> regWrites;
	unsigned samples; // number of samples between this and next event
};
using Log = vector<LogEvent>;
using Samples = vector<int>;


static void error(const string& message)
{
	cout << message << '\n';
}


static void saveWav(const string& filename, const Samples& data)
{
	WavWriter writer(Filename(filename), 1, 16, 3579545 / 72);
	writer.write16mono(&data[0], data.size());
}

static void loadWav(const string& filename, Samples& data)
{
	WavData wav(filename);
	assert(wav.getFreq() == 3579545 / 72);

	auto rawData = wav.getData();
	data.assign(rawData, rawData + wav.getSize());
}

static void loadWav(Samples& data)
{
	string filename = coreName + '-' + testName + ".wav";
	loadWav(filename, data);
}

static void createSilence(const Log& log, Samples& result)
{
	unsigned size = 0;
	for (auto& l : log) {
		size += l.samples;
	}
	result.resize(size);
}


static void test(YM2413Core& core, const Log& log,
                 const Samples* expectedSamples[CHANNELS])
{
	cout << " test " << testName << " ...\n";

	Samples generatedSamples[CHANNELS];

	for (auto& l : log) {
		// write registers
		for (auto& w : l.regWrites) {
			core.writeReg(w.reg, w.val);
		}

		unsigned samples = l.samples;

		// setup buffers
		int* bufs[CHANNELS];
		unsigned oldSize = generatedSamples[0].size();
		for (unsigned i = 0; i < CHANNELS; ++i) {
			generatedSamples[i].resize(oldSize + samples);
			bufs[i] = &generatedSamples[i][oldSize];
		}

		// actually generate samples
		core.generateChannels(bufs, samples);
	}

	// amplify generated data
	// (makes comparison between different cores easier)
	unsigned factor = core.getAmplificationFactor();
	for (unsigned i = 0; i < CHANNELS; ++i) {
		for (unsigned j = 0; j < generatedSamples[i].size(); ++j) {
			int s = generatedSamples[i][j];
			s *= factor;
			assert(s == int16_t(s)); // shouldn't overflow 16-bit
			generatedSamples[i][j] = s;
		}
	}

	// verify generated samples
	for (unsigned i = 0; i < CHANNELS; ++i) {
		string msg = strCat("Error in channel ", i, ": ");
		bool err = false;
		if (generatedSamples[i].size() != expectedSamples[i]->size()) {
			strAppend(msg, "wrong size, expected ", expectedSamples[i]->size(),
			          " but got ", generatedSamples[i].size());
			err = true;
		} else if (generatedSamples[i] != *expectedSamples[i]) {
			strAppend(msg, "Wrong data");
			err = true;
		}
		if (err) {
			string filename = strCat(
				"bad-", coreName, '-', testName,
				"-ch", i, ".wav");
			strAppend(msg, " writing data to ", filename);
			error(msg);
			saveWav(filename, generatedSamples[i]);
		}
	}
}

static void testSingleChannel(YM2413Core& core, const Log& log,
                              const Samples& channelData, unsigned channelNum)
{
	Samples silence;
	createSilence(log, silence);

	const Samples* samples[CHANNELS];
	for (unsigned i = 0; i < CHANNELS; ++i) {
		if (i == channelNum) {
			samples[i] = &channelData;
		} else {
			samples[i] = &silence;
		}
	}

	test(core, log, samples);
}


static void testSilence(YM2413Core& core)
{
	testName = "silence";
	Log log;
	{
		LogEvent event;
		// no register writes
		event.samples = 1000;
		log.push_back(event);
	}
	Samples silence;
	createSilence(log, silence);

	const Samples* samples[CHANNELS];
	for (unsigned i = 0; i < CHANNELS; ++i) {
		samples[i] = &silence;
	}

	test(core, log, samples);
}

static void testViolin(YM2413Core& core)
{
	testName = "violin";
	Log log;
	{
		LogEvent event;
		event.regWrites.emplace_back(0x30, 0x10); // instrument / volume
		event.regWrites.emplace_back(0x10, 0xAD); // frequency
		event.regWrites.emplace_back(0x20, 0x14); // key-on / frequency
		event.samples = 11000;
		log.push_back(event);
	}
	{
		LogEvent event;
		event.regWrites.emplace_back(0x20, 0x16); // change freq
		event.samples = 11000;
		log.push_back(event);
	}
	{
		LogEvent event;
		event.regWrites.emplace_back(0x20, 0x06); // key-off
		event.samples = 11000;
		log.push_back(event);
	}
	Samples gold;
	loadWav(gold);

	testSingleChannel(core, log, gold, 0);
}

template<typename CORE, typename FUNC> void testOnCore(FUNC f)
{
	CORE core;
	f(core);
}

template<typename CORE> static void testAll(const string& coreName_)
{
	coreName = coreName_;
	cout << "Testing YM2413 core " << coreName << '\n';
	testOnCore<CORE>(testSilence);
	testOnCore<CORE>(testViolin);
	cout << '\n';
}

int main()
{
	testAll<YM2413Okazaki::   YM2413>("Okazaki");
	testAll<YM2413Burczynski::YM2413>("Burczynski");
	return 0;
}
