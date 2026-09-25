#include "MSXMakoto.hh"
#include "DeviceConfig.hh"
#include "ResampledSoundDevice.hh"
#include "Schedulable.hh"
#include "IRQHelper.hh"
#include "IntegerSetting.hh"
#include "Rom.hh"
#include "SimpleDebuggable.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "3rdparty/ymfm/ymfm_opn.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <optional>
#include <vector>

namespace openmsx {
// Cartridge integration is intentionally separate from the unmodified YMFM core.
class MakotoSound final : public ResampledSoundDevice, private Schedulable,
                          private ymfm::ymfm_interface {
public:
 MakotoSound(DeviceConfig& config, EmuTime time)
  : ResampledSoundDevice(config.getMotherBoard(), "Makoto", "Makoto YM2608 OPNA", 1,
                         unsigned(config.getChildDataAsInt("clock", 8000000)) / 8, true)
  , Schedulable(config.getScheduler())
  , irq(config.getMotherBoard(), "Makoto.IRQ", config)
  , mainVolume(config.getCommandController(), "makoto_master_volume", "Makoto master output volume", 50, 0, 100)
  , psgVolume(config.getCommandController(), "makoto_psg_volume", "Makoto PSG volume", 50, 0, 100)
  , clock(unsigned(config.getChildDataAsInt("clock", 8000000)))
  , contextTime(time), busyEnd(time), chip(*this)
  , registers(config.getMotherBoard(), *this)
 {
  if (clock < 1000000 || clock > 16000000) throw MSXException("Invalid Makoto clock");
  if (config.findChild("rom")) {
   rhythm = std::make_unique<Rom>("Makoto rhythm ROM", "YM2608 internal rhythm samples", config);
   if (rhythm->size() != 8192) throw MSXException("Makoto rhythm ROM must be 8192 bytes");
  }
  auto filename = config.getChildData("tracefile", "");
  if (!filename.empty()) {
   trace.open(std::string(filename));
   if (!trace) throw MSXException("Cannot open Makoto trace file");
   trace << "ticks,event,address,value\n";
  }
  reset(time);
  registerSound(config);
 }
 ~MakotoSound() { unregisterSound(); removeSyncPoints(); }
 void reset(EmuTime time) {
  updateStream(time);
  contextTime = time;
  deadlines.fill(EmuTime::infinity());
  removeSyncPoints();
  chip.reset();
  busyEnd = time;
  latch = 0;
  regs.fill(0);
  irq.reset();
  log("reset", 0, 0);
 }
 byte read(unsigned port, EmuTime time) {
  updateStream(time);
  contextTime = time;
  auto value = chip.read(port);
  log("read", port, value);
  return value;
 }
 byte peek(unsigned port, EmuTime time) {
  // Debugger reads must not advance ADPCM RAM, audio, flags or timers.
  if (port == 0 || port == 2) {
   auto savedTime = contextTime;
   contextTime = time;
   std::vector<uint8_t> state;
   ymfm::ymfm_saved_state saved(state, true);
   chip.save_restore(saved);
   suppressIRQ = true;
   auto result = port == 0 ? chip.read_status() : chip.read_status_hi();
   ymfm::ymfm_saved_state restore(state, false);
   chip.save_restore(restore);
   suppressIRQ = false;
   contextTime = savedTime;
   return result;
  }
  return (port == 1 && latch < 14) ? regs[latch] : 0xff;
 }
 void write(unsigned port, byte value, EmuTime time) {
  updateStream(time);
  contextTime = time;
  if (!(port & 1)) latch = uint16_t(value | ((port & 2) << 7));
  else if (unsigned(latch >> 8) == (port >> 1)) {
   regs[latch] = value;
   log("write", latch, value);
  }
  chip.write(port, value);
  reschedule();
 }
 template<typename Archive> void serialize(Archive& ar, unsigned /*version*/) {
  if constexpr (!Archive::IS_LOADER) updateStream(Schedulable::getCurrentTime());
  std::vector<uint8_t> state;
  if constexpr (!Archive::IS_LOADER) {
   ymfm::ymfm_saved_state saved(state, true);
   chip.save_restore(saved);
  }
  ar.serialize("core", state, "deadlines", deadlines, "busyEnd", busyEnd,
               "regs", regs, "latch", latch, "sampleRAM", sampleRAM, "irq", irq,
               "sampleClock", getEmuClock());
  if constexpr (Archive::IS_LOADER) {
   // Reject truncated core state instead of allowing YMFM's zero-fill fallback.
   std::vector<uint8_t> expected;
   ymfm::ymfm_saved_state measure(expected, true);
   chip.save_restore(measure);
   if (state.size() != expected.size()) throw MSXException("Invalid Makoto core state size");
   ymfm::ymfm_saved_state saved(state, false);
   chip.save_restore(saved);
   chip.set_fidelity(ymfm::OPN_FIDELITY_MAX);
   chip.invalidate_caches();
   contextTime = Schedulable::getCurrentTime();
   reschedule();
  }
 }
private:
 void log(const char* event, unsigned address, unsigned value) {
  if (trace.is_open()) trace << (contextTime - EmuTime::zero()).toUint64() << ','
                            << event << ',' << address << ',' << value << '\n';
 }
 EmuDuration clocks(unsigned count) const {
  return EmuDuration::sec(double(count) / clock);
 }
 void reschedule() {
  removeSyncPoints();
  std::optional<EmuTime> next;
  for (auto deadline : deadlines) if (deadline != EmuTime::infinity() && (!next || deadline < *next)) next = deadline;
  if (next) setSyncPoint(*next);
 }
 void executeUntil(EmuTime time) override {
  updateStream(time);
  contextTime = time;
  for (unsigned i = 0; i < 2; ++i) {
   if (deadlines[i] <= time) {
    deadlines[i] = EmuTime::infinity();
    log("timer", i, 1);
    m_engine->engine_timer_expired(i);
   }
  }
  reschedule();
 }
 void ymfm_set_timer(uint32_t timer, int32_t duration) override {
  if (duration < 0) deadlines[timer] = EmuTime::infinity();
  else deadlines[timer] = contextTime + clocks(unsigned(duration));
 }
 void ymfm_set_busy_end(uint32_t duration) override {
  busyEnd = contextTime + clocks(duration);
 }
 bool ymfm_is_busy() override { return contextTime < busyEnd; }
 void ymfm_update_irq(bool asserted) override { if (!suppressIRQ) { irq.set(asserted); log("irq", 0, asserted); } }
 uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override {
  if (type == ymfm::ACCESS_ADPCM_B) return sampleRAM[address & 0x3ffff];
  if (type == ymfm::ACCESS_ADPCM_A) return rhythm ? (*rhythm)[address & 0x1fff] : 0;
  return 0xff; // SSG GPIO is not attached to the MSX keyboard or joysticks.
 }
 void ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t value) override {
  if (type == ymfm::ACCESS_ADPCM_B) sampleRAM[address & 0x3ffff] = value;
 }
 void setOutputRate(unsigned hostSampleRate, double speed) override {
  const auto previousClock = getEmuClock();
  ResampledSoundDevice::setOutputRate(hostSampleRate, speed);
  if (sampleClockInitialized && previousClock.getPeriod() == getEmuClock().getPeriod()) {
   getEmuClock().reset(previousClock.getTime());
  }
  sampleClockInitialized = true;
 }
 void generateChannels(std::span<float*> buffers, unsigned num) override {
  auto* out = buffers[0];
  float fmGain = float(mainVolume.getInt()) / 100.0f;
  float psgGain = float(psgVolume.getInt()) / 100.0f;
  for (unsigned i = 0; i < num; ++i) {
   ymfm::ym2608::output_data sample;
   chip.generate(&sample);
   // FM/ADPCM stereo plus the mono SSG output. Analogue balance is provisional.
   out[2 * i] = fmGain * (float(sample.data[0]) + psgGain * float(sample.data[2]));
   out[2 * i + 1] = fmGain * (float(sample.data[1]) + psgGain * float(sample.data[2]));
  }
 }
 struct Registers final : SimpleDebuggable {
  Registers(MSXMotherBoard& board, MakotoSound& owner_)
   : SimpleDebuggable(board, "Makoto registers", "Last YM2608 register writes", 512), owner(owner_) {}
  byte read(unsigned address) override { return owner.regs[address]; }
  MakotoSound& owner;
 };
 bool sampleClockInitialized = false;
 bool suppressIRQ = false;
 OptionalIRQHelper irq;
 IntegerSetting mainVolume;
 IntegerSetting psgVolume;
 const unsigned clock;
 EmuTime contextTime;
 EmuTime busyEnd;
 std::array<EmuTime, 2> deadlines = {EmuTime::infinity(), EmuTime::infinity()};
 ymfm::ym2608 chip;
 std::array<byte, 512> regs = {};
 uint16_t latch = 0;
 std::array<byte, 262144> sampleRAM = {};
 std::unique_ptr<Rom> rhythm;
 std::ofstream trace;
 Registers registers;
};

MSXMakoto::MSXMakoto(DeviceConfig& config)
 : MSXDevice(config), sound(std::make_unique<MakotoSound>(config, getCurrentTime())) {}
MSXMakoto::~MSXMakoto() = default;
void MSXMakoto::reset(EmuTime time) { sound->reset(time); }
byte MSXMakoto::readIO(uint16_t port, EmuTime time) { return sound->read(port & 3, time); }
byte MSXMakoto::peekIO(uint16_t port, EmuTime time) const { return sound->peek(port & 3, time); }
void MSXMakoto::writeIO(uint16_t port, byte value, EmuTime time) { sound->write(port & 3, value, time); }
template<typename Archive>
void MSXMakoto::serialize(Archive& ar, unsigned /*version*/) {
 ar.template serializeBase<MSXDevice>(*this);
 ar.serialize("sound", *sound);
}
INSTANTIATE_SERIALIZE_METHODS(MSXMakoto);
REGISTER_MSXDEVICE(MSXMakoto, "Makoto");
}

