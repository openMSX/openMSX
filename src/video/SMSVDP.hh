#ifndef SMSVDP_HH
#define SMSVDP_HH

#include "SMSVDPCore.hh"

#include "BooleanSetting.hh"
#include "DynamicClock.hh"
#include "IRQHelper.hh"
#include "Schedulable.hh"
#include "SimpleDebuggable.hh"
#include "VideoSystemChangeListener.hh"

#include "EmuTime.hh"
#include "outer.hh"
#include "serialize_meta.hh"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace openmsx {

class DeviceConfig;
class MSXMotherBoard;
class PostProcessor;
class Scheduler;
class SMSVDPRenderer;
class VDP;

/** Emulation of the Sega Master System VDP (315-5124/315-5246) as found in
  * the SuperSoniqs Franky cartridge.
  *
  * This class connects the (pure) SMSVDPCore to openMSX: it schedules the
  * scanline timing on the emulation time line, maps the register/counter
  * accesses, drives the /INT line and hosts the renderer (for the separate
  * output window, like the V9990/Video9000 implementation).
  */
class SMSVDP final : public VideoSystemChangeListener
{
public:
	enum class VideoStandard : uint8_t { NTSC, PAL };

	/** Name of the video source registered for the Franky's VDP output. */
	static constexpr std::string_view VIDEO_SOURCE = "Franky";

	SMSVDP(MSXMotherBoard& motherBoard, std::string name,
	       VideoStandard standard, SMSVDPCore::Variant variant);
	~SMSVDP();

	void reset(EmuTime time);

	// ports 0x88/0x89 (data/control)
	[[nodiscard]] uint8_t readData(EmuTime time);
	void writeData(uint8_t value, EmuTime time);
	[[nodiscard]] uint8_t readControl(EmuTime time);
	void writeControl(uint8_t value, EmuTime time);

	// ports 0x48/0x49 (V/H counters)
	[[nodiscard]] uint8_t readVCounter(EmuTime time);
	[[nodiscard]] uint8_t readHCounter(EmuTime time);

	// Side-effect-free variants of the reads above and of the data/control
	// reads, for MSXDevice::peekIO().
	[[nodiscard]] uint8_t peekVCounter(EmuTime time) const;
	[[nodiscard]] uint8_t peekHCounter() const;
	[[nodiscard]] uint8_t peekData() const;
	[[nodiscard]] uint8_t peekControl(EmuTime time) const;

	[[nodiscard]] PostProcessor* getPostProcessor() const;
	[[nodiscard]] MSXMotherBoard& getMotherBoard() const { return motherBoard; }
	[[nodiscard]] Scheduler& getScheduler() const;

	/** The 80 chip colors (RGB888), used by the rasterizer to build its
	  * native palette (with the video settings applied).
	  */
	[[nodiscard]] std::span<const uint32_t, 80> getPaletteColors() const {
		return core.paletteColors();
	}

	/** The MSX VDP watched by the auto video switch. Called from
	  * Franky::init() with the device declared via <device idref="VDP"/>.
	  */
	void setMsxVdp(VDP& vdp);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// VideoSystemChangeListener
	void preVideoSystemChange() noexcept override;
	void postVideoSystemChange() noexcept override;

	void createRenderer();
	void updateClock();
	void updateAutoVideoSwitch(EmuTime time);
	[[nodiscard]] unsigned currentHpos(EmuTime time) const;
	[[nodiscard]] bool isPal() const {
		return videoStandard == VideoStandard::PAL;
	}

	// scheduled events (all relative to the start of a scanline)
	struct SyncBase : Schedulable {
		explicit SyncBase(const SMSVDP& vdp) : Schedulable(vdp.getScheduler()) {}
		using Schedulable::setSyncPoint;
		using Schedulable::removeSyncPoint;
	protected:
		~SyncBase() = default;
	};
	struct SyncLine final : SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime time) override {
			auto& vdp = OUTER(SMSVDP, syncLine);
			vdp.executeLine(time);
		}
	};
	struct SyncInt final : SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime time) override {
			auto& vdp = OUTER(SMSVDP, syncInt);
			vdp.executeInt(time);
		}
	};
	struct SyncDraw final : SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime time) override {
			auto& vdp = OUTER(SMSVDP, syncDraw);
			vdp.executeDraw(time);
		}
	};
	struct SyncEol final : SyncBase {
		using SyncBase::SyncBase;
		void executeUntil(EmuTime time) override {
			auto& vdp = OUTER(SMSVDP, syncEol);
			vdp.executeEol(time);
		}
	};

	void executeLine(EmuTime time);
	void executeInt(EmuTime time);
	void executeDraw(EmuTime time);
	void executeEol(EmuTime time);

private:
	MSXMotherBoard& motherBoard;

	VideoStandard videoStandard;
	BooleanSetting autoVideoSwitchSetting;
	VDP* msxVdp = nullptr;

	// irq is declared before core: the core constructor resets the chip,
	// which notifies the /INT callback (forwarded to irq).
	IRQHelper irq;
	SMSVDPCore core;
	std::unique_ptr<SMSVDPRenderer> renderer;

	// Debuggables: "Franky regs" and "Franky status regs".
	struct RegDebug final : SimpleDebuggable {
		RegDebug(const SMSVDP& vdp, std::string_view name);
		[[nodiscard]] uint8_t read(unsigned address) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
	} regDebug;
	struct StatusDebug final : SimpleDebuggable {
		StatusDebug(const SMSVDP& vdp, std::string_view name);
		[[nodiscard]] uint8_t read(unsigned address) override;
	} statusDebug;

	/** Pixel clock of the currently selected video standard. */
	DynamicClock pixelClock;

	/** Line currently being emulated (0..height-1). */
	int vpos = 0;
	bool frameActive = false;

	// Note: declared last so that they are constructed after 'motherBoard'
	// (their constructors call getScheduler()).
	SyncLine syncLine{*this};
	SyncInt syncInt{*this};
	SyncDraw syncDraw{*this};
	SyncEol syncEol{*this};
};

} // namespace openmsx

#endif
