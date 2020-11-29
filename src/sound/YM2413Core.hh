#ifndef YM2413CORE_HH
#define YM2413CORE_HH

#include <cstdint>

namespace openmsx {

/** Abstract interface for the YM2413 core.
 *
 * We currently have two concrete implementations:
 *  - YM2413Okazaki
 *  - YM2413Burczynski
 * The long term goal is to study the difference between these two
 * implementations and merge the best parts of either core into a single core.
 *
 * This interface separates the actual YM2413 emulation from the rest of the
 * (sound) emulation details. This allows to more easily share this
 * implementation between different emulators. It also allows to more easily
 * test the YM2413 emulation in isolation.
 *
 * There are two main functions in this interface: write to registers and get
 * output samples. All timing information is implicit in the order of the calls
 * to these functions (e.g. write some register, generate 10 output samples,
 * write another register, ...).
 */
class YM2413Core
{
public:
	/** Input clock frequency.
	 * An output sample is generated every 72 cycles. So the output sample
	 * frequency is effectively 49716Hz. If you need the output at e.g.
	 * 44100Hz you need to resample the data. Generating the output at the
	 * native YM2413 frequency allows for quite some simplifications in the
	 * implementation of the cores. It also generally results in better
	 * sound quality. At least if the resampler step itself doesn't degrade
	 * the quality again (Certainly don't simply skip some of the samples
	 * to get to 44kHz, at the very least do linear interpolation. But
	 * preferably use a better resample algorithm).
	 */
	static constexpr int CLOCK_FREQ = 3579545;

	virtual ~YM2413Core() = default;

	/** Reset this YM2413 core.
	 */
	virtual void reset() = 0;

	/** Write to the YM2413 register/data port.
	 * Writing to a register is a 2-step process: First write the register
	 * number to the register port (0), then write the value for that
	 * register to the data port (1). There are various timing constraits
	 * that must be taken into account (not described here).
	 *
	 * We emulate the YM2413 as-if it generates a stream at 49.7kHz and at
	 * each step it produces one sample for each of the 9+5 sound channels.
	 * Though in reality the YM2413 spreads out the generation of the
	 * channels over time. So it's like a stream at 18 x 49.7kHz where the
	 * 9+5 channels have a fixed position in the 18 possible slots (so not
	 * all 18 slots produce a (non-zero) output).
	 *
	 * In other words: we emulate the YM2413 18 steps at a time. Though for
	 * accurate emulation we still need to take the exact timing of
	 * register writes into account. That's what the 'offset' parameter is
	 * for. It must have a value [0..17] and indicates the sub-sample
	 * timing of the port-write.
	 */
	virtual void writePort(bool port, uint8_t value, int offset) = 0;

	/** Write to a YM2413 register (for debug).
	 * Because of the timing constraint on writing registers via
	 * writePort(), writing registers via that way is not very suited
	 * for use in a debugger. Use this method as an alternative.
	 */
	virtual void pokeReg(uint8_t reg, uint8_t value) = 0;

	/** Read from a YM2413 register (for debug).
	 * Note that the real YM2413 chip doesn't allow to read the registers.
	 * This returns the last written value or the default value if this
	 * register hasn't been written to since the last reset() call.
	 * Reading registers has no influence on the generated sound.
	 */
	[[nodiscard]] virtual uint8_t peekReg(uint8_t reg) const = 0;

	/** Generate the sound output.
	 * @param bufs Pointers to output buffers.
	 * @param num The number of required output samples.
	 *
	 * The requested number of samples must be strictly bigger than zero.
	 *
	 * The output of the different channels is put in separate output
	 * buffers. This makes it possible to e.g. record individual channels
	 * or to pan, mute or adjust volume per channel. The YM2413 can operate
	 * in two modes: 9 channels or 6 channels + 5 drum channels. The latter
	 * mode requires a total of 11 output buffers. In the first mode, the
	 * last two output buffers are filled with silence (this is very
	 * efficient, see below). Each output buffer should be big enough to
	 * hold at least 'num' number of ints.
	 *
	 * The output is not simply stored in the buffer, but added to the
	 * existing data in the buffer. So you'll have to zero the content
	 * of the buffer before passing it to this function. OTOH if you want
	 * to combine (some of) the channels, you can pass the same buffer for
	 * all those channels. This approach may have a little overhead when
	 * you're interested in all channels, but it is very efficient if
	 * you're only interested in the combined channels (the most common
	 * case). You can even directly combine this output with other chips
	 * with the same native frequency (like Y8950 or YMF262).
	 *
	 * When the core detects that some channel is silent, it will assign
	 * nullptr to the buffer pointer (so the content of the buffer is left
	 * unchanged, but the pointer to that buffer is set to zero). When all
	 * the channels you're interested in are silent you can even skip all
	 * subsequent audio processing for this channel (e.g. skip resampling to
	 * 44kHz). It is very common that some or even all of the channels are
	 * silent, so it's definitely worth it to implement this optimization.
	 * Also the cores internally try to detect silent channels very early,
	 * so an idle YM2413 core generally requires very little emulation
	 * time.
	 */
	virtual void generateChannels(float* bufs[11], unsigned num) = 0;

	/** Returns normalization factor.
	 * The output of the generateChannels() method should still be
	 * amplified (=multiplied) with this factor to get a consistent volume
	 * level across the different implementations of the YM2413 core. This
	 * allows to internally calculate with native volume levels, and
	 * possibly results in slightly simpler and/or faster code. It's very
	 * likely that subsequent audio processing steps (like resampler,
	 * filters or volume adjustments) must anyway still multiply the output
	 * sample values, so this factor can be folded-in for free.
	 */
	[[nodiscard]] virtual float getAmplificationFactor() const = 0;

protected:
	YM2413Core() = default;
};

} // namespace openmsx

#endif
