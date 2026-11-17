#ifndef RESAMPLEHQ_HH
#define RESAMPLEHQ_HH

#include "ResampleAlgo.hh"

#include "MemBuffer.hh"
#include "aligned.hh"

#include <cstdint>
#include <vector>

namespace openmsx {

class DynamicClock;
class ResampledSoundDevice;

/** The coefficient tables of ResampleHQ, one per resample-ratio, shared
  * between all ResampleHQ objects with the same ratio.
  */
class ResampleCoeffs
{
public:
	// The prototype filter, see the comments in ResampleHQ.cc.
	static constexpr double BETA = (100.3 + 0.5) / 10.0; // Kaiser window parameter
	// The original libsamplerate design:
	static constexpr int INDEX_INC = 128; // table units per (output) sample
	static constexpr int HALF_LEN = 2462; // window span, in table units
	static constexpr double FUDGE = 1.0 + 850136.0 / 4194304.0;
	static constexpr double PERIOD = FUDGE * INDEX_INC; // distance between zero crossings
	// fudge(L) = 1 / (1 - FUDGE_C / L), calibrated to the original design
	static constexpr double FUDGE_C = (1.0 - 1.0 / FUDGE) * HALF_LEN / INDEX_INC; // = 3.2416

	/** Kaiser windowed sinc: zero for |x| > halfSpan, zero crossings
	  * at multiples of 'period'. Not normalized. */
	[[nodiscard]] static double proto(double x, double halfSpan, double period);
	/** The sinc period for a window with the given half-span (both in
	  * output samples), such that the stop band starts at Nyquist. */
	[[nodiscard]] static double fudge(double halfSpan);
	/** The original libsamplerate prototype filter, 'x' in table units.
	  * (Only used by the unit test.) */
	[[nodiscard]] static double coeff(double x);

	ResampleCoeffs(const ResampleCoeffs&) = delete;
	ResampleCoeffs(ResampleCoeffs&&) = delete;
	ResampleCoeffs& operator=(const ResampleCoeffs&) = delete;
	ResampleCoeffs& operator=(ResampleCoeffs&&) = delete;

	[[nodiscard]] static ResampleCoeffs& instance();
	void getCoeffs(double ratio, float*& table, unsigned& filterLen);
	void releaseCoeffs(double ratio);

private:
	using Table = MemBuffer<float, SSE_ALIGNMENT>;

	ResampleCoeffs() = default;
	~ResampleCoeffs();

	[[nodiscard]] static Table calcTable(double ratio, unsigned& filterLen);

	struct Element {
		double ratio;
		Table table;
		unsigned filterLen;
		unsigned count;
	};
	std::vector<Element> cache; // typically 1-4 entries -> unsorted vector
};

template<unsigned CHANNELS>
class ResampleHQ final : public ResampleAlgo
{
public:
	// The coefficient table has 2^TAB_BITS rows (sub-sample phases), of
	// which only the first half is stored (the 2nd half is the mirror
	// image). The output is linearly interpolated between the two rows
	// that enclose the exact phase. 128 rows is enough to keep the
	// interpolation error below -99dBFS for all our sample rates (so
	// better than 16-bit).
	static constexpr int TAB_BITS = 7;
	static constexpr size_t TAB_LEN = size_t(1) << TAB_BITS;
	static constexpr size_t HALF_TAB_LEN = TAB_LEN / 2;

public:
	ResampleHQ(ResampledSoundDevice& input, const DynamicClock& hostClock);
	ResampleHQ(const ResampleHQ&) = delete;
	ResampleHQ(ResampleHQ&&) = delete;
	ResampleHQ& operator=(const ResampleHQ&) = delete;
	ResampleHQ& operator=(ResampleHQ&&) = delete;
	~ResampleHQ() override;

	bool generateOutputImpl(float* dataOut, size_t num,
	                        EmuTime time) override;

private:
	void calcOutput(unsigned intPos, uint32_t fracPos, float* output);
	void prepareData(unsigned emuNum);

private:
	const DynamicClock& hostClock;
	const float ratio;
	const uint64_t step; // 'ratio' as 32.32 fixed point
	unsigned bufStart;
	unsigned bufEnd;
	unsigned nonzeroSamples = 0;
	unsigned filterLen;
	std::vector<float> buffer;
	float* table;
};

} // namespace openmsx

#endif
