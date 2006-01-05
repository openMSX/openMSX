// $Id$

#include "Keys.hh"

using std::string;

namespace openmsx {

Keys::KeyMap Keys::keymap;

Keys::KeyCode Keys::getCode(const string& name)
{
	initialize();

	KeyCode result = static_cast<KeyCode>(0);
	string::size_type lastPos = 0;
	while (lastPos != string::npos) {
		string::size_type pos = name.find_first_of(",+/", lastPos);
		string part = (pos != string::npos)
		            ? name.substr(lastPos, pos - lastPos)
			    : name.substr(lastPos);
		KeyMap::const_iterator it = keymap.find(part);
		if (it != keymap.end()) {
			KeyCode partCode = it->second;
			if ((partCode & K_MASK) && (result & K_MASK)) {
				// more than one non-modifier component
				// is not allowed
				return K_NONE;
			}
			result = static_cast<KeyCode>(result | partCode);
		} else {
			return K_NONE;
		}
		lastPos = pos;
		if (lastPos != string::npos) {
			++lastPos;
		}
	}
	return result;
}

Keys::KeyCode Keys::getCode(SDLKey key, SDLMod mod, bool release)
{
	KeyCode result = (KeyCode)key;
	if (mod & KMOD_CTRL) {
		result = static_cast<KeyCode>(result | KM_CTRL);
	}
	if (mod & KMOD_SHIFT) {
		result = static_cast<KeyCode>(result | KM_SHIFT);
	}
	if (mod & KMOD_ALT) {
		result = static_cast<KeyCode>(result | KM_ALT);
	}
	if (mod & KMOD_META) {
		result = static_cast<KeyCode>(result | KM_META);
	}
	if (release) {
		result = static_cast<KeyCode>(result | KD_RELEASE);
	}
	return result;
}

const string Keys::getName(KeyCode keyCode)
{
	initialize();

	string result;
	for (KeyMap::const_iterator it = keymap.begin();
	     it != keymap.end(); ++it) {
		if (it->second == (keyCode & K_MASK)) {
			result = it->first;
			break;
		}
	}
	if (result.empty()) {
		return "unknown";
	}
	if (keyCode & KM_CTRL) {
		result += "+CTRL";
	}
	if (keyCode & KM_SHIFT) {
		result += "+SHIFT";
	}
	if (keyCode & KM_ALT) {
		result += "+ALT";
	}
	if (keyCode & KM_META) {
		result += "+META";
	}
	if (keyCode & KD_RELEASE) {
		result += ",RELEASE";
	}
	return result;
}

Keys::KeyCode Keys::combine(KeyCode key, KeyCode modifier)
{
	return (KeyCode)((int)key | (int)modifier);
}

void Keys::initialize()
{
	static bool init = false;

	if (init) return;
	init = true;

	keymap["BACKSPACE"]	= K_BACKSPACE;
	keymap["TAB"]		= K_TAB;
	keymap["CLEAR"]		= K_CLEAR;
	keymap["RETURN"]	= K_RETURN;
	keymap["PAUSE"]		= K_PAUSE;
	keymap["ESCAPE"]	= K_ESCAPE;
	keymap["SPACE"]		= K_SPACE;
	keymap["EXCLAIM"]	= K_EXCLAIM;
	keymap["QUOTEDBL"]	= K_QUOTEDBL;
	keymap["HASH"]		= K_HASH;
	keymap["DOLLAR"]	= K_DOLLAR;
	keymap["AMPERSAND"]	= K_AMPERSAND;
	keymap["QUOTE"]		= K_QUOTE;
	keymap["LEFTPAREN"]	= K_LEFTPAREN;
	keymap["RIGHTPAREN"]	= K_RIGHTPAREN;
	keymap["ASTERISK"]	= K_ASTERISK;
	keymap["PLUS"]		= K_PLUS;
	keymap["COMMA"]		= K_COMMA;
	keymap["MINUS"]		= K_MINUS;
	keymap["PERIOD"]	= K_PERIOD;
	keymap["SLASH"]		= K_SLASH;
	keymap["0"]		= K_0;
	keymap["1"]		= K_1;
	keymap["2"]		= K_2;
	keymap["3"]		= K_3;
	keymap["4"]		= K_4;
	keymap["5"]		= K_5;
	keymap["6"]		= K_6;
	keymap["7"]		= K_7;
	keymap["8"]		= K_8;
	keymap["9"]		= K_9;
	keymap["COLON"]		= K_COLON;
	keymap["SEMICOLON"]	= K_SEMICOLON;
	keymap["LESS"]		= K_LESS;
	keymap["EQUALS"]	= K_EQUALS;
	keymap["GREATER"]	= K_GREATER;
	keymap["QUESTION"]	= K_QUESTION;
	keymap["AT"]		= K_AT;

	keymap["LEFTBRACKET"]	= K_LEFTBRACKET;
	keymap["BACKSLASH"]	= K_BACKSLASH;
	keymap["RIGHTBRACKET"]	= K_RIGHTBRACKET;
	keymap["CARET"]		= K_CARET;
	keymap["UNDERSCORE"]	= K_UNDERSCORE;
	keymap["BACKQUOTE"]	= K_BACKQUOTE;
	keymap["A"]		= K_A;
	keymap["B"]		= K_B;
	keymap["C"]		= K_C;
	keymap["D"]		= K_D;
	keymap["E"]		= K_E;
	keymap["F"]		= K_F;
	keymap["G"]		= K_G;
	keymap["H"]		= K_H;
	keymap["I"]		= K_I;
	keymap["J"]		= K_J;
	keymap["K"]		= K_K;
	keymap["L"]		= K_L;
	keymap["M"]		= K_M;
	keymap["N"]		= K_N;
	keymap["O"]		= K_O;
	keymap["P"]		= K_P;
	keymap["Q"]		= K_Q;
	keymap["R"]		= K_R;
	keymap["S"]		= K_S;
	keymap["T"]		= K_T;
	keymap["U"]		= K_U;
	keymap["V"]		= K_V;
	keymap["W"]		= K_W;
	keymap["X"]		= K_X;
	keymap["Y"]		= K_Y;
	keymap["Z"]		= K_Z;
	keymap["DELETE"]	= K_DELETE;

	keymap["WORLD_0"]	= K_WORLD_0;
	keymap["WORLD_1"]	= K_WORLD_1;
	keymap["WORLD_2"]	= K_WORLD_2;
	keymap["WORLD_3"]	= K_WORLD_3;
	keymap["WORLD_4"]	= K_WORLD_4;
	keymap["WORLD_5"]	= K_WORLD_5;
	keymap["WORLD_6"]	= K_WORLD_6;
	keymap["WORLD_7"]	= K_WORLD_7;
	keymap["WORLD_8"]	= K_WORLD_8;
	keymap["WORLD_9"]	= K_WORLD_9;
	keymap["WORLD_10"]	= K_WORLD_10;
	keymap["WORLD_11"]	= K_WORLD_11;
	keymap["WORLD_12"]	= K_WORLD_12;
	keymap["WORLD_13"]	= K_WORLD_13;
	keymap["WORLD_14"]	= K_WORLD_14;
	keymap["WORLD_15"]	= K_WORLD_15;
	keymap["WORLD_16"]	= K_WORLD_16;
	keymap["WORLD_17"]	= K_WORLD_17;
	keymap["WORLD_18"]	= K_WORLD_18;
	keymap["WORLD_19"]	= K_WORLD_19;
	keymap["WORLD_20"]	= K_WORLD_20;
	keymap["WORLD_21"]	= K_WORLD_21;
	keymap["WORLD_22"]	= K_WORLD_22;
	keymap["WORLD_23"]	= K_WORLD_23;
	keymap["WORLD_24"]	= K_WORLD_24;
	keymap["WORLD_25"]	= K_WORLD_25;
	keymap["WORLD_26"]	= K_WORLD_26;
	keymap["WORLD_27"]	= K_WORLD_27;
	keymap["WORLD_28"]	= K_WORLD_28;
	keymap["WORLD_29"]	= K_WORLD_29;
	keymap["WORLD_30"]	= K_WORLD_30;
	keymap["WORLD_31"]	= K_WORLD_31;
	keymap["WORLD_32"]	= K_WORLD_32;
	keymap["WORLD_33"]	= K_WORLD_33;
	keymap["WORLD_34"]	= K_WORLD_34;
	keymap["WORLD_35"]	= K_WORLD_35;
	keymap["WORLD_36"]	= K_WORLD_36;
	keymap["WORLD_37"]	= K_WORLD_37;
	keymap["WORLD_38"]	= K_WORLD_38;
	keymap["WORLD_39"]	= K_WORLD_39;
	keymap["WORLD_40"]	= K_WORLD_40;
	keymap["WORLD_41"]	= K_WORLD_41;
	keymap["WORLD_42"]	= K_WORLD_42;
	keymap["WORLD_43"]	= K_WORLD_43;
	keymap["WORLD_44"]	= K_WORLD_44;
	keymap["WORLD_45"]	= K_WORLD_45;
	keymap["WORLD_46"]	= K_WORLD_46;
	keymap["WORLD_47"]	= K_WORLD_47;
	keymap["WORLD_48"]	= K_WORLD_48;
	keymap["WORLD_49"]	= K_WORLD_49;
	keymap["WORLD_50"]	= K_WORLD_50;
	keymap["WORLD_51"]	= K_WORLD_51;
	keymap["WORLD_52"]	= K_WORLD_52;
	keymap["WORLD_53"]	= K_WORLD_53;
	keymap["WORLD_54"]	= K_WORLD_54;
	keymap["WORLD_55"]	= K_WORLD_55;
	keymap["WORLD_56"]	= K_WORLD_56;
	keymap["WORLD_57"]	= K_WORLD_57;
	keymap["WORLD_58"]	= K_WORLD_58;
	keymap["WORLD_59"]	= K_WORLD_59;
	keymap["WORLD_60"]	= K_WORLD_60;
	keymap["WORLD_61"]	= K_WORLD_61;
	keymap["WORLD_62"]	= K_WORLD_62;
	keymap["WORLD_63"]	= K_WORLD_63;
	keymap["WORLD_64"]	= K_WORLD_64;
	keymap["WORLD_65"]	= K_WORLD_65;
	keymap["WORLD_66"]	= K_WORLD_66;
	keymap["WORLD_67"]	= K_WORLD_67;
	keymap["WORLD_68"]	= K_WORLD_68;
	keymap["WORLD_69"]	= K_WORLD_69;
	keymap["WORLD_70"]	= K_WORLD_70;
	keymap["WORLD_71"]	= K_WORLD_71;
	keymap["WORLD_72"]	= K_WORLD_72;
	keymap["WORLD_73"]	= K_WORLD_73;
	keymap["WORLD_74"]	= K_WORLD_74;
	keymap["WORLD_75"]	= K_WORLD_75;
	keymap["WORLD_76"]	= K_WORLD_76;
	keymap["WORLD_77"]	= K_WORLD_77;
	keymap["WORLD_78"]	= K_WORLD_78;
	keymap["WORLD_79"]	= K_WORLD_79;
	keymap["WORLD_80"]	= K_WORLD_80;
	keymap["WORLD_81"]	= K_WORLD_81;
	keymap["WORLD_82"]	= K_WORLD_82;
	keymap["WORLD_83"]	= K_WORLD_83;
	keymap["WORLD_84"]	= K_WORLD_84;
	keymap["WORLD_85"]	= K_WORLD_85;
	keymap["WORLD_86"]	= K_WORLD_86;
	keymap["WORLD_87"]	= K_WORLD_87;
	keymap["WORLD_88"]	= K_WORLD_88;
	keymap["WORLD_89"]	= K_WORLD_89;
	keymap["WORLD_90"]	= K_WORLD_90;
	keymap["WORLD_91"]	= K_WORLD_91;
	keymap["WORLD_92"]	= K_WORLD_92;
	keymap["WORLD_93"]	= K_WORLD_93;
	keymap["WORLD_94"]	= K_WORLD_94;
	keymap["WORLD_95"]	= K_WORLD_95;

	// Numeric keypad
	keymap["KP0"]		= K_KP0;
	keymap["KP1"]		= K_KP1;
	keymap["KP2"]		= K_KP2;
	keymap["KP3"]		= K_KP3;
	keymap["KP4"]		= K_KP4;
	keymap["KP5"]		= K_KP5;
	keymap["KP6"]		= K_KP6;
	keymap["KP7"]		= K_KP7;
	keymap["KP8"]		= K_KP8;
	keymap["KP9"]		= K_KP9;
	keymap["KP_PERIOD"]	= K_KP_PERIOD;
	keymap["KP_DIVIDE"]	= K_KP_DIVIDE;
	keymap["KP_MULTIPLY"]	= K_KP_MULTIPLY;
	keymap["KP_MINUS"]	= K_KP_MINUS;
	keymap["KP_PLUS"]	= K_KP_PLUS;
	keymap["KP_ENTER"]	= K_KP_ENTER;
	keymap["KP_EQUALS"]	= K_KP_EQUALS;

	// Arrows + Home/End pad
	keymap["UP"]		= K_UP;
	keymap["DOWN"]		= K_DOWN;
	keymap["RIGHT"]		= K_RIGHT;
	keymap["LEFT"]		= K_LEFT;
	keymap["INSERT"]	= K_INSERT;
	keymap["HOME"]		= K_HOME;
	keymap["END"]		= K_END;
	keymap["PAGEUP"]	= K_PAGEUP;
	keymap["PAGEDOWN"]	= K_PAGEDOWN;

	// Function keys
	keymap["F1"]		= K_F1;
	keymap["F2"]		= K_F2;
	keymap["F3"]		= K_F3;
	keymap["F4"]		= K_F4;
	keymap["F5"]		= K_F5;
	keymap["F6"]		= K_F6;
	keymap["F7"]		= K_F7;
	keymap["F8"]		= K_F8;
	keymap["F9"]		= K_F9;
	keymap["F10"]		= K_F10;
	keymap["F11"]		= K_F11;
	keymap["F12"]		= K_F12;
	keymap["F13"]		= K_F13;
	keymap["F14"]		= K_F14;
	keymap["F15"]		= K_F15;

	// Key state modifier keys
	keymap["NUMLOCK"]	= K_NUMLOCK;
	keymap["CAPSLOCK"]	= K_CAPSLOCK;
	keymap["SCROLLOCK"]	= K_SCROLLOCK;
	keymap["RSHIFT"]	= K_RSHIFT;
	keymap["LSHIFT"]	= K_LSHIFT;
	keymap["RCTRL"]		= K_RCTRL;
	keymap["LCTRL"]		= K_LCTRL;
	keymap["RALT"]		= K_RALT;
	keymap["LALT"]		= K_LALT;
	keymap["RMETA"]		= K_RMETA;
	keymap["LMETA"]		= K_LMETA;
	keymap["LSUPER"]	= K_LSUPER;	// Left "Windows" key
	keymap["RSUPER"]	= K_RSUPER;	// Right "Windows" key
	keymap["MODE"]		= K_MODE;	// "Alt Gr" key
	keymap["COMPOSE"]	= K_COMPOSE;	// Multi-key compose key

	// Miscellaneous function keys
	keymap["HELP"]		= K_HELP;
	keymap["PRINT"]		= K_PRINT;
	keymap["SYSREQ"]	= K_SYSREQ;
	keymap["BREAK"]		= K_BREAK;
	keymap["MENU"]		= K_MENU;
	keymap["POWER"]		= K_POWER;	// Power Macintosh power key
	keymap["EURO"]		= K_EURO;	// Some european keyboards
	keymap["UNDO"]		= K_UNDO;

	// Modifiers
	keymap["SHIFT"] 	= KM_SHIFT;
	keymap["CTRL"]		= KM_CTRL;
	keymap["ALT"]		= KM_ALT;
	keymap["META"]		= KM_META;

	// Direction modifiers
	keymap["PRESS"]		= KD_PRESS;
	keymap["RELEASE"]	= KD_RELEASE;
}

} // namespace openmsx


