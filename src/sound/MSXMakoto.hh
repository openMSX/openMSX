#ifndef MSXMAKOTO_HH
#define MSXMAKOTO_HH
#include "MSXDevice.hh"
#include <memory>
namespace openmsx {
class MakotoSound;
class MSXMakoto final : public MSXDevice {
public:
 explicit MSXMakoto(DeviceConfig& config);
 ~MSXMakoto() override;
 void reset(EmuTime time) override;
 byte readIO(uint16_t port, EmuTime time) override;
 byte peekIO(uint16_t port, EmuTime time) const override;
 void writeIO(uint16_t port, byte value, EmuTime time) override;
 template<typename Archive> void serialize(Archive& ar, unsigned version);
private:
 std::unique_ptr<MakotoSound> sound;
};
}
#endif
