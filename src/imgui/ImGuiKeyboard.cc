// Implementation inspired by:
//   https://github.com/CedricGuillemet/ImHotKey

#include "ImGuiKeyboard.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "Keyboard.hh"
#include "KeyboardInfo.hh"
#include "MSXMotherBoard.hh"
#include "UnicodeKeymap.hh"

#include "strCat.hh"
#include "utf8_unchecked.hh"

#include <imgui.h>

#include <array>
#include <iterator>
#include <optional>
#include <span>
#include <string>

namespace openmsx {

using namespace std::literals;


namespace {

struct Key {
	const char* label; // for keys that don't type a character, see getLabel()
	gl::vec2 pos;
	gl::vec2 size;
	KeyMatrixPosition matrixPos;
	bool onKeypad = false; // (on MSX and SVI the keys in rows 9 and 10 are on the keypad too)
};

}

// The keyboard of a standard international MSX (the positions of the keys on
// other MSX keyboards differ a bit, but the matrix positions are the same).
static constexpr std::array msxKeys = {
	Key{"F1",     {  0,  0}, {52,32}, KeyMatrixPosition{0x65}},
	Key{"F2",     { 56,  0}, {52,32}, KeyMatrixPosition{0x66}},
	Key{"F3",     {112,  0}, {52,32}, KeyMatrixPosition{0x67}},
	Key{"F4",     {168,  0}, {52,32}, KeyMatrixPosition{0x70}},
	Key{"F5",     {224,  0}, {52,32}, KeyMatrixPosition{0x71}},
	Key{"Select", {300,  0}, {52,32}, KeyMatrixPosition{0x76}},
	Key{"Stop",   {356,  0}, {52,32}, KeyMatrixPosition{0x74}},
	Key{"Home",   {432,  0}, {52,32}, KeyMatrixPosition{0x81}},
	Key{"Ins",    {488,  0}, {52,32}, KeyMatrixPosition{0x82}},
	Key{"Del",    {544,  0}, {52,32}, KeyMatrixPosition{0x83}},

	Key{"Esc",    {  0, 36}, {36,32}, KeyMatrixPosition{0x72}},
	Key{"1",      { 40, 36}, {36,32}, KeyMatrixPosition{0x01}},
	Key{"2",      { 80, 36}, {36,32}, KeyMatrixPosition{0x02}},
	Key{"3",      {120, 36}, {36,32}, KeyMatrixPosition{0x03}},
	Key{"4",      {160, 36}, {36,32}, KeyMatrixPosition{0x04}},
	Key{"5",      {200, 36}, {36,32}, KeyMatrixPosition{0x05}},
	Key{"6",      {240, 36}, {36,32}, KeyMatrixPosition{0x06}},
	Key{"7",      {280, 36}, {36,32}, KeyMatrixPosition{0x07}},
	Key{"8",      {320, 36}, {36,32}, KeyMatrixPosition{0x10}},
	Key{"9",      {360, 36}, {36,32}, KeyMatrixPosition{0x11}},
	Key{"0",      {400, 36}, {36,32}, KeyMatrixPosition{0x00}},
	Key{"-",      {440, 36}, {36,32}, KeyMatrixPosition{0x12}},
	Key{"=",      {480, 36}, {36,32}, KeyMatrixPosition{0x13}},
	Key{"\\",     {520, 36}, {36,32}, KeyMatrixPosition{0x14}},
	Key{"BS",     {560, 36}, {36,32}, KeyMatrixPosition{0x75}},

	Key{"Tab",    {  0, 72}, {56,32}, KeyMatrixPosition{0x73}},
	Key{"Q",      { 60, 72}, {36,32}, KeyMatrixPosition{0x46}},
	Key{"W",      {100, 72}, {36,32}, KeyMatrixPosition{0x54}},
	Key{"E",      {140, 72}, {36,32}, KeyMatrixPosition{0x32}},
	Key{"R",      {180, 72}, {36,32}, KeyMatrixPosition{0x47}},
	Key{"T",      {220, 72}, {36,32}, KeyMatrixPosition{0x51}},
	Key{"Y",      {260, 72}, {36,32}, KeyMatrixPosition{0x56}},
	Key{"U",      {300, 72}, {36,32}, KeyMatrixPosition{0x52}},
	Key{"I",      {340, 72}, {36,32}, KeyMatrixPosition{0x36}},
	Key{"O",      {380, 72}, {36,32}, KeyMatrixPosition{0x44}},
	Key{"P",      {420, 72}, {36,32}, KeyMatrixPosition{0x45}},
	Key{"[",      {460, 72}, {36,32}, KeyMatrixPosition{0x15}},
	Key{"]",      {500, 72}, {36,32}, KeyMatrixPosition{0x16}},
	Key{"Return", {548, 72}, {48,68}, KeyMatrixPosition{0x77}},

	Key{"Ctrl",   {  0,108}, {64,32}, KeyMatrixPosition{0x61}},
	Key{"A",      { 68,108}, {36,32}, KeyMatrixPosition{0x26}},
	Key{"S",      {108,108}, {36,32}, KeyMatrixPosition{0x50}},
	Key{"D",      {148,108}, {36,32}, KeyMatrixPosition{0x31}},
	Key{"F",      {188,108}, {36,32}, KeyMatrixPosition{0x33}},
	Key{"G",      {228,108}, {36,32}, KeyMatrixPosition{0x34}},
	Key{"H",      {268,108}, {36,32}, KeyMatrixPosition{0x35}},
	Key{"J",      {308,108}, {36,32}, KeyMatrixPosition{0x37}},
	Key{"K",      {348,108}, {36,32}, KeyMatrixPosition{0x40}},
	Key{"L",      {388,108}, {36,32}, KeyMatrixPosition{0x41}},
	Key{";",      {428,108}, {36,32}, KeyMatrixPosition{0x17}},
	Key{"'",      {468,108}, {36,32}, KeyMatrixPosition{0x20}},
	Key{"`",      {508,108}, {36,32}, KeyMatrixPosition{0x21}},

	Key{"Shift",  {  0,144}, {80,32}, KeyMatrixPosition{0x60}},
	Key{"Z",      { 84,144}, {36,32}, KeyMatrixPosition{0x57}},
	Key{"X",      {124,144}, {36,32}, KeyMatrixPosition{0x55}},
	Key{"C",      {164,144}, {36,32}, KeyMatrixPosition{0x30}},
	Key{"V",      {204,144}, {36,32}, KeyMatrixPosition{0x53}},
	Key{"B",      {244,144}, {36,32}, KeyMatrixPosition{0x27}},
	Key{"N",      {284,144}, {36,32}, KeyMatrixPosition{0x43}},
	Key{"M",      {324,144}, {36,32}, KeyMatrixPosition{0x42}},
	Key{",",      {364,144}, {36,32}, KeyMatrixPosition{0x22}},
	Key{".",      {404,144}, {36,32}, KeyMatrixPosition{0x23}},
	Key{"/",      {444,144}, {36,32}, KeyMatrixPosition{0x24}},
	Key{"Acc",    {484,144}, {36,32}, KeyMatrixPosition{0x25}},
	Key{"Shift",  {524,144}, {72,32}, KeyMatrixPosition{0x60}},

	Key{"Cap",    { 84,180}, {36,32}, KeyMatrixPosition{0x63}},
	Key{"Grp",    {124,180}, {36,32}, KeyMatrixPosition{0x62}},
	Key{"Space",  {164,180},{316,32}, KeyMatrixPosition{0x80}},
	Key{"Cod",    {484,180}, {36,32}, KeyMatrixPosition{0x64}},

	Key{"7",      {632,  0}, {36,32}, KeyMatrixPosition{0xA2}},
	Key{"8",      {672,  0}, {36,32}, KeyMatrixPosition{0xA3}},
	Key{"9",      {712,  0}, {36,32}, KeyMatrixPosition{0xA4}},
	Key{"-",      {752,  0}, {36,32}, KeyMatrixPosition{0xA5}},
	Key{"4",      {632, 36}, {36,32}, KeyMatrixPosition{0x97}},
	Key{"5",      {672, 36}, {36,32}, KeyMatrixPosition{0xA0}},
	Key{"6",      {712, 36}, {36,32}, KeyMatrixPosition{0xA1}},
	Key{"/",      {752, 36}, {36,32}, KeyMatrixPosition{0x92}},
	Key{"1",      {632, 72}, {36,32}, KeyMatrixPosition{0x94}},
	Key{"2",      {672, 72}, {36,32}, KeyMatrixPosition{0x95}},
	Key{"3",      {712, 72}, {36,32}, KeyMatrixPosition{0x96}},
	Key{"*",      {752, 72}, {36,32}, KeyMatrixPosition{0x90}},
	Key{"0",      {632,108}, {36,32}, KeyMatrixPosition{0x93}},
	Key{",",      {672,108}, {36,32}, KeyMatrixPosition{0xA6}},
	Key{".",      {712,108}, {36,32}, KeyMatrixPosition{0xA7}},
	Key{"+",      {752,108}, {36,32}, KeyMatrixPosition{0x91}},

	Key{"Left",   {652,144}, {36,68}, KeyMatrixPosition{0x84}},
	Key{"Up",     {692,144}, {36,32}, KeyMatrixPosition{0x85}},
	Key{"Down",   {692,180}, {36,32}, KeyMatrixPosition{0x86}},
	Key{"Right",  {732,144}, {36,68}, KeyMatrixPosition{0x87}},
};

// The keyboard of the Spectravideo SVI-328
static constexpr std::array sviKeys = {
	Key{"F1",     {  0,  0}, {52,32}, KeyMatrixPosition{0x70}},
	Key{"F2",     { 56,  0}, {52,32}, KeyMatrixPosition{0x71}},
	Key{"F3",     {112,  0}, {52,32}, KeyMatrixPosition{0x72}},
	Key{"F4",     {168,  0}, {52,32}, KeyMatrixPosition{0x73}},
	Key{"F5",     {224,  0}, {52,32}, KeyMatrixPosition{0x74}},
	Key{"Stop",   {300,  0}, {52,32}, KeyMatrixPosition{0x65}},
	Key{"Cls",    {376,  0}, {36,32}, KeyMatrixPosition{0x75}},
	Key{"Ins",    {416,  0}, {36,32}, KeyMatrixPosition{0x76}},
	Key{"Del",    {456,  0}, {36,32}, KeyMatrixPosition{0x82}},

	Key{"Esc",    {  0, 36}, {36,32}, KeyMatrixPosition{0x64}},
	Key{"1",      { 40, 36}, {36,32}, KeyMatrixPosition{0x01}},
	Key{"2",      { 80, 36}, {36,32}, KeyMatrixPosition{0x02}},
	Key{"3",      {120, 36}, {36,32}, KeyMatrixPosition{0x03}},
	Key{"4",      {160, 36}, {36,32}, KeyMatrixPosition{0x04}},
	Key{"5",      {200, 36}, {36,32}, KeyMatrixPosition{0x05}},
	Key{"6",      {240, 36}, {36,32}, KeyMatrixPosition{0x06}},
	Key{"7",      {280, 36}, {36,32}, KeyMatrixPosition{0x07}},
	Key{"8",      {320, 36}, {36,32}, KeyMatrixPosition{0x10}},
	Key{"9",      {360, 36}, {36,32}, KeyMatrixPosition{0x11}},
	Key{"0",      {400, 36}, {36,32}, KeyMatrixPosition{0x00}},
	Key{"-",      {440, 36}, {36,32}, KeyMatrixPosition{0x20}},
	Key{"=",      {480, 36}, {36,32}, KeyMatrixPosition{0x15}},
	Key{"BS",     {520, 36}, {56,32}, KeyMatrixPosition{0x56}},

	Key{"Tab",    {  0, 72}, {56,32}, KeyMatrixPosition{0x81}},
	Key{"Q",      { 60, 72}, {36,32}, KeyMatrixPosition{0x41}},
	Key{"W",      {100, 72}, {36,32}, KeyMatrixPosition{0x47}},
	Key{"E",      {140, 72}, {36,32}, KeyMatrixPosition{0x25}},
	Key{"R",      {180, 72}, {36,32}, KeyMatrixPosition{0x42}},
	Key{"T",      {220, 72}, {36,32}, KeyMatrixPosition{0x44}},
	Key{"Y",      {260, 72}, {36,32}, KeyMatrixPosition{0x51}},
	Key{"U",      {300, 72}, {36,32}, KeyMatrixPosition{0x45}},
	Key{"I",      {340, 72}, {36,32}, KeyMatrixPosition{0x31}},
	Key{"O",      {380, 72}, {36,32}, KeyMatrixPosition{0x37}},
	Key{"P",      {420, 72}, {36,32}, KeyMatrixPosition{0x40}},
	Key{"[",      {460, 72}, {36,32}, KeyMatrixPosition{0x53}},
	Key{"]",      {500, 72}, {36,32}, KeyMatrixPosition{0x55}},
	Key{"\\",     {540, 72}, {36,32}, KeyMatrixPosition{0x54}},

	Key{"Caps",   {  0,108}, {36,68}, KeyMatrixPosition{0x83}},
	Key{"Ctrl",   { 40,108}, {56,32}, KeyMatrixPosition{0x61}},
	Key{"A",      {100,108}, {36,32}, KeyMatrixPosition{0x21}},
	Key{"S",      {140,108}, {36,32}, KeyMatrixPosition{0x43}},
	Key{"D",      {180,108}, {36,32}, KeyMatrixPosition{0x24}},
	Key{"F",      {220,108}, {36,32}, KeyMatrixPosition{0x26}},
	Key{"G",      {260,108}, {36,32}, KeyMatrixPosition{0x27}},
	Key{"H",      {300,108}, {36,32}, KeyMatrixPosition{0x30}},
	Key{"J",      {340,108}, {36,32}, KeyMatrixPosition{0x32}},
	Key{"K",      {380,108}, {36,32}, KeyMatrixPosition{0x33}},
	Key{"L",      {420,108}, {36,32}, KeyMatrixPosition{0x34}},
	Key{":",      {460,108}, {36,32}, KeyMatrixPosition{0x12}},
	Key{"'",      {500,108}, {36,32}, KeyMatrixPosition{0x13}},
	Key{"Enter",  {540,108}, {56,32}, KeyMatrixPosition{0x66}},

	Key{"Shift",  { 40,144}, {76,32}, KeyMatrixPosition{0x60}},
	Key{"Z",      {120,144}, {36,32}, KeyMatrixPosition{0x52}},
	Key{"X",      {160,144}, {36,32}, KeyMatrixPosition{0x50}},
	Key{"C",      {200,144}, {36,32}, KeyMatrixPosition{0x23}},
	Key{"V",      {240,144}, {36,32}, KeyMatrixPosition{0x46}},
	Key{"B",      {280,144}, {36,32}, KeyMatrixPosition{0x22}},
	Key{"N",      {320,144}, {36,32}, KeyMatrixPosition{0x36}},
	Key{"M",      {360,144}, {36,32}, KeyMatrixPosition{0x35}},
	Key{",",      {400,144}, {36,32}, KeyMatrixPosition{0x14}},
	Key{".",      {440,144}, {36,32}, KeyMatrixPosition{0x16}},
	Key{"/",      {480,144}, {36,32}, KeyMatrixPosition{0x17}},
	Key{"Shift",  {520,144}, {76,32}, KeyMatrixPosition{0x60}},

	Key{"LGrp",   { 80,180}, {36,32}, KeyMatrixPosition{0x62}},
	Key{"RGrp",   {120,180}, {36,32}, KeyMatrixPosition{0x63}},
	Key{"Space",  {160,180},{316,32}, KeyMatrixPosition{0x80}},

	Key{"Sel",    {620,  0}, {36,32}, KeyMatrixPosition{0x84}},
	Key{"Up",     {660,  0}, {36,32}, KeyMatrixPosition{0x57}},
	Key{"Print",  {700,  0}, {36,32}, KeyMatrixPosition{0x85}},
	Key{"/",      {740,  0}, {36,32}, KeyMatrixPosition{0xA5}},
	Key{"Left",   {620, 36}, {36,32}, KeyMatrixPosition{0x67}},
	Key{"Down",   {660, 36}, {36,32}, KeyMatrixPosition{0x77}},
	Key{"Right",  {700, 36}, {36,32}, KeyMatrixPosition{0x87}},
	Key{"*",      {740, 36}, {36,32}, KeyMatrixPosition{0xA4}},
	Key{"7",      {620, 72}, {36,32}, KeyMatrixPosition{0x97}},
	Key{"8",      {660, 72}, {36,32}, KeyMatrixPosition{0xA0}},
	Key{"9",      {700, 72}, {36,32}, KeyMatrixPosition{0xA1}},
	Key{"+",      {740, 72}, {36,32}, KeyMatrixPosition{0xA2}},
	Key{"4",      {620,108}, {36,32}, KeyMatrixPosition{0x94}},
	Key{"5",      {660,108}, {36,32}, KeyMatrixPosition{0x95}},
	Key{"6",      {700,108}, {36,32}, KeyMatrixPosition{0x96}},
	Key{"-",      {740,108}, {36,32}, KeyMatrixPosition{0xA3}},
	Key{"1",      {620,144}, {36,32}, KeyMatrixPosition{0x91}},
	Key{"2",      {660,144}, {36,32}, KeyMatrixPosition{0x92}},
	Key{"3",      {700,144}, {36,32}, KeyMatrixPosition{0x93}},
	Key{"Enter",  {740,144}, {36,68}, KeyMatrixPosition{0x66}, true}, // same as the main Enter key
	Key{"0",      {620,180}, {36,32}, KeyMatrixPosition{0x90}},
	Key{",",      {660,180}, {36,32}, KeyMatrixPosition{0xA7}},
	Key{".",      {700,180}, {36,32}, KeyMatrixPosition{0xA6}},
};

// The keyboard of the Sega SC-3000 (and the SK-1100)
static constexpr std::array segaKeys = {
	Key{"1",      { 40,  0}, {36,32}, KeyMatrixPosition{0x00}},
	Key{"2",      { 80,  0}, {36,32}, KeyMatrixPosition{0x10}},
	Key{"3",      {120,  0}, {36,32}, KeyMatrixPosition{0x20}},
	Key{"4",      {160,  0}, {36,32}, KeyMatrixPosition{0x30}},
	Key{"5",      {200,  0}, {36,32}, KeyMatrixPosition{0x40}},
	Key{"6",      {240,  0}, {36,32}, KeyMatrixPosition{0x50}},
	Key{"7",      {280,  0}, {36,32}, KeyMatrixPosition{0x60}},
	Key{"8",      {320,  0}, {36,32}, KeyMatrixPosition{0x70}},
	Key{"9",      {360,  0}, {36,32}, KeyMatrixPosition{0x80}},
	Key{"0",      {400,  0}, {36,32}, KeyMatrixPosition{0x90}},
	Key{"-",      {440,  0}, {36,32}, KeyMatrixPosition{0xA0}},
	Key{"^",      {480,  0}, {36,32}, KeyMatrixPosition{0xB0}},
	Key{"\\",     {520,  0}, {36,32}, KeyMatrixPosition{0xC0}},
	Key{"Break",  {580,  0}, {52,32}, KeyMatrixPosition{0xD0}},

	Key{"Func",   {  0, 36}, {56,32}, KeyMatrixPosition{0xC3}},
	Key{"Q",      { 60, 36}, {36,32}, KeyMatrixPosition{0x01}},
	Key{"W",      {100, 36}, {36,32}, KeyMatrixPosition{0x11}},
	Key{"E",      {140, 36}, {36,32}, KeyMatrixPosition{0x21}},
	Key{"R",      {180, 36}, {36,32}, KeyMatrixPosition{0x31}},
	Key{"T",      {220, 36}, {36,32}, KeyMatrixPosition{0x41}},
	Key{"Y",      {260, 36}, {36,32}, KeyMatrixPosition{0x51}},
	Key{"U",      {300, 36}, {36,32}, KeyMatrixPosition{0x61}},
	Key{"I",      {340, 36}, {36,32}, KeyMatrixPosition{0x07}},
	Key{"O",      {380, 36}, {36,32}, KeyMatrixPosition{0x17}},
	Key{"P",      {420, 36}, {36,32}, KeyMatrixPosition{0x27}},
	Key{"@",      {460, 36}, {36,32}, KeyMatrixPosition{0x37}},
	Key{"[",      {500, 36}, {36,32}, KeyMatrixPosition{0x47}},
	Key{"CR",     {548, 36}, {48,68}, KeyMatrixPosition{0x56}},

	Key{"Ctrl",   {  0, 72}, {64,32}, KeyMatrixPosition{0xD2}},
	Key{"A",      { 68, 72}, {36,32}, KeyMatrixPosition{0x02}},
	Key{"S",      {108, 72}, {36,32}, KeyMatrixPosition{0x12}},
	Key{"D",      {148, 72}, {36,32}, KeyMatrixPosition{0x22}},
	Key{"F",      {188, 72}, {36,32}, KeyMatrixPosition{0x32}},
	Key{"G",      {228, 72}, {36,32}, KeyMatrixPosition{0x42}},
	Key{"H",      {268, 72}, {36,32}, KeyMatrixPosition{0x52}},
	Key{"J",      {308, 72}, {36,32}, KeyMatrixPosition{0x62}},
	Key{"K",      {348, 72}, {36,32}, KeyMatrixPosition{0x06}},
	Key{"L",      {388, 72}, {36,32}, KeyMatrixPosition{0x16}},
	Key{";",      {428, 72}, {36,32}, KeyMatrixPosition{0x26}},
	Key{":",      {468, 72}, {36,32}, KeyMatrixPosition{0x36}},
	Key{"]",      {508, 72}, {36,32}, KeyMatrixPosition{0x46}},

	Key{"Shift",  {  0,108}, {80,32}, KeyMatrixPosition{0xD3}},
	Key{"Z",      { 84,108}, {36,32}, KeyMatrixPosition{0x03}},
	Key{"X",      {124,108}, {36,32}, KeyMatrixPosition{0x13}},
	Key{"C",      {164,108}, {36,32}, KeyMatrixPosition{0x23}},
	Key{"V",      {204,108}, {36,32}, KeyMatrixPosition{0x33}},
	Key{"B",      {244,108}, {36,32}, KeyMatrixPosition{0x43}},
	Key{"N",      {284,108}, {36,32}, KeyMatrixPosition{0x53}},
	Key{"M",      {324,108}, {36,32}, KeyMatrixPosition{0x63}},
	Key{",",      {364,108}, {36,32}, KeyMatrixPosition{0x05}},
	Key{".",      {404,108}, {36,32}, KeyMatrixPosition{0x15}},
	Key{"/",      {444,108}, {36,32}, KeyMatrixPosition{0x25}},
	Key{"Pi",     {484,108}, {36,32}, KeyMatrixPosition{0x35}},
	Key{"Shift",  {524,108}, {72,32}, KeyMatrixPosition{0xD3}},

	Key{"Graph",  { 84,144}, {48,32}, KeyMatrixPosition{0xD1}},
	Key{"Eng/Kana",{136,144}, {56,32}, KeyMatrixPosition{0x04}},
	Key{"Space",  {196,144},{248,32}, KeyMatrixPosition{0x14}},
	Key{"Home",   {452,144}, {48,32}, KeyMatrixPosition{0x24}},
	Key{"Ins",    {504,144}, {48,32}, KeyMatrixPosition{0x34}},

	Key{"Up",     {640, 36}, {36,32}, KeyMatrixPosition{0x66}},
	Key{"Left",   {600, 72}, {36,32}, KeyMatrixPosition{0x55}},
	Key{"Right",  {680, 72}, {36,32}, KeyMatrixPosition{0x65}},
	Key{"Down",   {640,108}, {36,32}, KeyMatrixPosition{0x45}},
};

// The label of a key: for keys that type a character, what they type on this
// machine (without and with SHIFT), according to the unicodemap (and the
// character set) of the machine. That's also what the 'type' command and the
// CHARACTER mapping mode use. Other keys keep the label of the table.
static std::string getLabel(const Key& key, const KeyboardInfo& info)
{
	using Mask = UnicodeKeymap::KeyInfo;
	auto printable = [&](uint8_t mods) -> std::optional<unsigned> {
		auto u = info.getUnicode(key.matrixPos, mods);
		if (u && (*u > 0x20) && (*u != 0x7f) && (*u != 0xa0)) return u;
		return {};
	};
	auto base  = printable(0);
	auto shift = printable(Mask::SHIFT_MASK);
	auto toUtf8 = [](unsigned u) {
		std::string result;
		utf8::unchecked::append(u, std::back_inserter(result));
		return result;
	};
	if (info.isDeadKey(key.matrixPos)) return "Dead";
	if (!base && !shift) return key.label;
	if (!base) return toUtf8(*shift);
	if (!shift || (*shift == *base)) return toUtf8(*base);
	// a letter with its capital (or the other way around): only the capital
	if (('a' <= *base) && (*base <= 'z') && (*shift == *base - 32)) return toUtf8(*shift);
	if (('A' <= *base) && (*base <= 'Z') && (*shift == *base + 32)) return toUtf8(*base);
	// like on the keycaps: the shifted character above the other one
	return strCat(toUtf8(*shift), '\n', toUtf8(*base));
}

void ImGuiKeyboard::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiKeyboard::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

void ImGuiKeyboard::paint(MSXMotherBoard* motherBoard)
{
	if (!show) return;

	if (!motherBoard) return;
	const auto* keyboard = motherBoard->getKeyboard();
	if (!keyboard) return;
	auto matrix = keyboard->getKeys();

	std::span<const Key> keys;
	switch (keyboard->getMatrix()) {
		using enum Keyboard::Matrix;
		case MSX:  keys = msxKeys;  break;
		case SVI:  keys = sviKeys;  break;
		case SEGA: keys = segaKeys; break;
		default:   break; // e.g. ColecoVision: no keyboard
	}

	if (motherBoard->getMachineID() != labelsMachineID) {
		labelsMachineID = motherBoard->getMachineID();
		const auto& info = keyboard->getKeyboardInfo();
		labels.clear();
		for (const auto& key : keys) {
			labels.push_back(getLabel(key, info));
		}
	}
	// the keypad keys only when the machine has a keypad
	// (on MSX and SVI rows 9 and 10 are the keypad, the Sega matrix uses
	// these rows for other keys)
	bool keypadRows = keyboard->getMatrix() != Keyboard::Matrix::SEGA;
	auto shown = [&](const Key& key) {
		bool onKeypad = key.onKeypad || (keypadRows && key.matrixPos.getRow() >= 9);
		return !onKeypad || keyboard->isKeypadEnabled();
	};

	ImGui::SetNextWindowSize({640, 200}, ImGuiCond_FirstUseEver);
	im::Window("Virtual keyboard", &show, [&]{
		if (keys.empty()) {
			ImGui::TextUnformatted("This machine has no keyboard."sv);
			return;
		}

		// (also the keys that are not shown: then switching between
		// machines with and without keypad doesn't change the key size)
		gl::vec2 maxXY;
		for (const auto& key : keys) {
			maxXY = max(maxXY, key.pos + key.size);
		}
		gl::vec2 available = ImGui::GetContentRegionAvail();
		float scale = min_component(available / maxXY);

		gl::vec2 origin = ImGui::GetCursorPos();
		im::ID_for_range(keys.size(), [&](int i) {
			const auto& key = keys[i];
			if (!shown(key)) return;
			auto row    = key.matrixPos.getRow();
			auto column = key.matrixPos.getColumn();
			auto mask = 1 << column;
			bool active = !(matrix[row] & mask);

			ImGui::SetCursorPos(origin + scale * key.pos);
			im::StyleColor(ImGuiCol_Button, getColor(active ? imColor::KEY_ACTIVE : imColor::KEY_NOT_ACTIVE), [&]{
				ImGui::Button(labels[i].c_str(), scale * key.size);
			});

			if (ImGui::IsItemActivated()) {
				manager.execute(makeTclList("keymatrixdown", row, mask));
			}
			if (ImGui::IsItemDeactivated()) {
				manager.execute(makeTclList("keymatrixup", row, mask));
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
				manager.execute(makeTclList((active ? "keymatrixup"sv : "keymatrixdown"sv),
							row, mask));
			}
		});
	});
}

} // namespace openmsx
