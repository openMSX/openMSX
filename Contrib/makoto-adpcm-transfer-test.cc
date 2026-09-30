// CPU ADPCM-B transfer boundaries. Build with ymfm_adpcm.cpp (C++17+).
// The x1 END=0 and LIMIT=0 expectations are physical Makoto V5 results
// from Sanyo MSX2+ and Panasonic turbo R. Other cases are software regressions.
#include "ymfm_adpcm.h"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <utility>
#include <vector>

namespace {
void require(bool ok, const char* message)
{
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}

struct Memory : ymfm::ymfm_interface {
    std::vector<uint8_t> ram = std::vector<uint8_t>(1 << 21, 0xff);
    std::vector<uint32_t> reads, writes;
    uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override
    {
        require(type == ymfm::ACCESS_ADPCM_B && address < ram.size(), "Invalid read");
        reads.push_back(address);
        return ram[address];
    }
    void ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t value) override
    {
        require(type == ymfm::ACCESS_ADPCM_B && address < ram.size(), "Invalid write");
        writes.push_back(address);
        ram[address] = value;
    }
};

void setup(ymfm::adpcm_b_engine& chip, uint8_t mode, uint8_t type,
           unsigned start, unsigned end, unsigned limit)
{
    chip.write(0x00, 0);
    chip.write(0x01, type);
    for (auto pair : {std::pair{0x02, start}, {0x04, end}, {0x0c, limit}}) {
        chip.write(pair.first, uint8_t(pair.second));
        chip.write(pair.first + 1, uint8_t(pair.second >> 8));
    }
    chip.write(0x00, 1); // Explicit reset, as required by physical V4/V5 probes.
    chip.write(0x00, mode);
}

void check_write(unsigned shift, uint8_t type, unsigned fixedShift,
                 unsigned start, unsigned end)
{
    Memory memory;
    ymfm::adpcm_b_engine chip(memory, fixedShift);
    chip.reset();
    setup(chip, 0x60, type, start, end, 0xffff);
    unsigned first = start << shift;
    unsigned count = ((end + 1) << shift) - first;
    for (unsigned i = 0; i < count; ++i) chip.write(8, uint8_t(0xa1 + i));
    require(memory.writes.size() == count, "CPU write dropped the final byte");
    for (unsigned i = 0; i < count; ++i) {
        require(memory.writes[i] == first + i, "CPU write address mismatch");
        require(memory.ram[first + i] == uint8_t(0xa1 + i), "CPU write data mismatch");
    }
    require(memory.ram[first + count] == 0xff, "Write escaped end boundary");
    // Keep the existing stopped-transfer policy; V5 did not test extra writes.
    chip.write(8, 0xee);
    require(memory.writes.size() == count, "Stopped write unexpectedly continued");
    setup(chip, 0x20, type, start, 0xffff, 0xffff);
    memory.reads.clear();
    chip.read(8); chip.read(8);
    require(memory.reads.empty(), "Dummy read touched external RAM");
    for (unsigned i = 0; i < count; ++i)
        require(chip.read(8) == uint8_t(0xa1 + i), "Readback mismatch");
}

void check_limit(unsigned shift, uint8_t type, unsigned fixedShift,
                 unsigned start, unsigned limit)
{
    Memory memory;
    ymfm::adpcm_b_engine chip(memory, fixedShift);
    chip.reset();
    unsigned size = (limit + 1) << shift;
    for (unsigned i = 0; i < size; ++i) memory.ram[i] = uint8_t(0xb1 + i);
    setup(chip, 0x20, type, start, 0xffff, limit);
    chip.read(8); chip.read(8);
    require(memory.reads.empty(), "Dummy read touched external RAM");
    unsigned address = start << shift;
    for (unsigned i = 0; i < 2 * size; ++i) {
        require(chip.read(8) == memory.ram[address], "CPU read wrapped before final byte");
        require(memory.reads.back() == address, "CPU read address mismatch");
        address = (address + 1) % size;
    }
}

void check_unfinished_write(uint8_t type, unsigned count)
{
    Memory memory;
    ymfm::adpcm_b_engine chip(memory);
    chip.reset();
    setup(chip, 0x60, type, 0, 0xffff, 0xffff);
    for (unsigned i = 0; i < count; ++i) chip.write(8, uint8_t(0xc1 + i));
    // V4 A/B: stopping/changing mode without RESET does not finish the
    // RAM writer. Reads keep returning its last CPU data-buffer byte.
    chip.write(0, 0);
    chip.write(0, 0x20);
    memory.reads.clear();
    const uint8_t last = uint8_t(0xc0 + count);
    std::vector<uint8_t> before;
    ymfm::ymfm_saved_state saving(before, true);
    chip.save_restore(saving);
    require(chip.peek(8) == last, "Peek missed unfinished writer buffer");
    for (unsigned i = 0; i < 10; ++i)
        require(chip.read(8) == last, "Read incorrectly restarted unfinished writer");
    require(memory.reads.empty(), "Stale-buffer read accessed RAM");
    std::vector<uint8_t> after;
    ymfm::ymfm_saved_state checking(after, true);
    chip.save_restore(checking);
    require(before == after, "Stale-buffer reads/peek changed state");

    chip.reset();
    ymfm::ymfm_saved_state restoring(before, false);
    chip.save_restore(restoring);
    require(chip.peek(8) == last && chip.read(8) == last,
            "Save/restore lost unfinished writer state");
    // V4 C/D: explicit RESET before read mode permits ordinary RAM reads.
    chip.write(0, 1);
    chip.write(0, 0x20);
    chip.read(8); chip.read(8);
    for (unsigned i = 0; i < count; ++i)
        require(chip.read(8) == uint8_t(0xc1 + i), "Reset did not recover reader");
}

} // namespace

int main()
{
    for (auto type : {uint8_t(0), uint8_t(2)})
        for (unsigned count : {1U, 8U, 17U}) check_unfinished_write(type, count);
    unsigned cases = 0;
    for (unsigned mode = 0; mode < 4; ++mode) {
        // x1 RAM, x8 RAM, ROM read addressing, and a fixed-shift core user.
        unsigned shift = mode == 0 ? 2 : 5;
        uint8_t type = mode == 1 ? 2 : mode == 2 ? 1 : 0;
        unsigned fixedShift = mode == 3 ? 5 : 0;
        for (unsigned end : {0U, 1U, 5U, 0x101U}) {
            for (unsigned start : {0U, end}) {
                if (mode != 2) {
                    check_write(shift, type, fixedShift, start, end);
                    ++cases;
                }
                check_limit(shift, type, fixedShift, start, end);
                ++cases;
            }
        }
    }
    std::cout << cases << " CPU boundary cases plus 6 reset/buffer/save cases passed (physical V4/V5 included).\n";
}
