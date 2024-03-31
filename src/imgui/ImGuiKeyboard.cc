// Implementation inspired by:
//   https://github.com/CedricGuillemet/ImHotKey

#include "ImGuiKeyboard.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "Keyboard.hh"
#include "MSXMotherBoard.hh"
#include "MSXPPI.hh"
#include "UnicodeKeymap.hh" // KeyMatrixPosition

#include <imgui.h>

#include <array>

namespace openmsx {

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
	auto* ppi = dynamic_cast<MSXPPI*>(motherBoard->findDevice("ppi"));
	if (!ppi) return;
	const auto& keyboard = ppi->getKeyboard();
	auto matrix = keyboard.getKeys();

	ImGui::SetNextWindowSize({640, 200}, ImGuiCond_FirstUseEver);
	im::Window("Virtual keyboard", &show, [&]{
		struct Key {
			const char* label;
			gl::vec2 pos;
			gl::vec2 size;
			KeyMatrixPosition matrixPos;
		};
		// TODO read this from some (MSX machine specific) config file
		static constexpr std::array keys = {
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

		gl::vec2 maxXY;
		for (const auto& key : keys) {
			maxXY = max(maxXY, key.pos + key.size);
		}
		gl::vec2 available = ImGui::GetContentRegionAvail();
		float scale = min_component(available / maxXY);

		gl::vec2 origin = ImGui::GetCursorPos();
		im::ID_for_range(keys.size(), [&](int i) {
			const auto& key = keys[i];
			auto row    = key.matrixPos.getRow();
			auto column = key.matrixPos.getColumn();
			auto mask = 1 << column;
			bool active = !(matrix[row] & mask);

			ImGui::SetCursorPos(origin + scale * key.pos);
			im::StyleColor(ImGuiCol_Button, getColor(active ? imColor::KEY_ACTIVE : imColor::KEY_NOT_ACTIVE), [&]{
				ImGui::Button(key.label, scale * key.size);
			});

			if (ImGui::IsItemActivated()) {
				manager.execute(makeTclList("keymatrixdown", row, mask));
			}
			if (ImGui::IsItemDeactivated()) {
				manager.execute(makeTclList("keymatrixup", row, mask));
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
				manager.execute(makeTclList((active ? "keymatrixup" : "keymatrixdown"),
							row, mask));
			}
		});
	});
}

} // namespace openmsx
