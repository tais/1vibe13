#include "KeyBindingModel.h"

#include <array>
#include <limits>

namespace ja2::runtime_control
{
namespace
{
struct NamedKey
{
	std::uint8_t value;
	std::string_view name;
};

constexpr std::array<NamedKey, 153> namedKeys{{
	{0x01, "LBUTTON"}, {0x02, "RBUTTON"}, {0x03, "CANCEL"},
	{0x04, "MBUTTON"}, {0x05, "XBUTTON1"}, {0x06, "XBUTTON2"},
	{0x08, "BACK"}, {0x08, "BACKSPACE"}, {0x09, "TAB"},
	{0x0c, "CLEAR"}, {0x0d, "RETURN"}, {0x0d, "ENTER"},
	{0x10, "SHIFT"}, {0x11, "CONTROL"}, {0x11, "CTRL"},
	{0x12, "MENU"}, {0x12, "ALT"}, {0x13, "PAUSE"},
	{0x14, "CAPITAL"}, {0x14, "CAPSLOCK"}, {0x14, "CAPS"},
	{0x15, "KANA"}, {0x15, "HANGEUL"}, {0x15, "HANGUL"},
	{0x17, "JUNJA"}, {0x18, "FINAL"}, {0x19, "HANJA"},
	{0x19, "KANJI"}, {0x1b, "ESCAPE"}, {0x1b, "ESC"},
	{0x1c, "CONVERT"}, {0x1d, "NONCONVERT"}, {0x1e, "ACCEPT"},
	{0x1f, "MODECHANGE"}, {0x20, "SPACE"}, {0x21, "PRIOR"},
	{0x21, "PAGEUP"}, {0x21, "PGUP"}, {0x22, "NEXT"},
	{0x22, "PAGEDOWN"}, {0x22, "PGDN"}, {0x23, "END"},
	{0x24, "HOME"}, {0x25, "LEFT"}, {0x26, "UP"},
	{0x27, "RIGHT"}, {0x28, "DOWN"}, {0x29, "SELECT"},
	{0x2a, "PRINT"}, {0x2b, "EXECUTE"}, {0x2c, "SNAPSHOT"},
	{0x2d, "INSERT"}, {0x2d, "INS"}, {0x2e, "DELETE"},
	{0x2e, "DEL"}, {0x2f, "HELP"}, {0x5b, "LWIN"},
	{0x5c, "RWIN"}, {0x5d, "APPS"}, {0x5f, "SLEEP"},
	{0x60, "NUMPAD0"}, {0x61, "NUMPAD1"}, {0x62, "NUMPAD2"},
	{0x63, "NUMPAD3"}, {0x64, "NUMPAD4"}, {0x65, "NUMPAD5"},
	{0x66, "NUMPAD6"}, {0x67, "NUMPAD7"}, {0x68, "NUMPAD8"},
	{0x69, "NUMPAD9"}, {0x6a, "MULTIPLY"}, {0x6b, "ADD"},
	{0x6c, "SEPARATOR"}, {0x6d, "SUBTRACT"}, {0x6e, "DECIMAL"},
	{0x6f, "DIVIDE"}, {0x90, "NUMLOCK"}, {0x91, "SCROLL"},
	{0x92, "OEM_NEC_EQUAL"}, {0x92, "OEM_FJ_JISHO"},
	{0x93, "OEM_FJ_MASSHOU"}, {0x94, "OEM_FJ_TOUROKU"},
	{0x95, "OEM_FJ_LOYA"}, {0x96, "OEM_FJ_ROYA"},
	{0xa0, "LSHIFT"}, {0xa1, "RSHIFT"}, {0xa2, "LCONTROL"},
	{0xa3, "RCONTROL"}, {0xa4, "LMENU"}, {0xa5, "RMENU"},
	{0xa6, "BROWSER_BACK"}, {0xa7, "BROWSER_FORWARD"},
	{0xa8, "BROWSER_REFRESH"}, {0xa9, "BROWSER_STOP"},
	{0xaa, "BROWSER_SEARCH"}, {0xab, "BROWSER_FAVORITES"},
	{0xac, "BROWSER_HOME"}, {0xad, "VOLUME_MUTE"},
	{0xae, "VOLUME_DOWN"}, {0xaf, "VOLUME_UP"},
	{0xb0, "MEDIA_NEXT_TRACK"}, {0xb1, "MEDIA_PREV_TRACK"},
	{0xb2, "MEDIA_STOP"}, {0xb3, "MEDIA_PLAY_PAUSE"},
	{0xb4, "LAUNCH_MAIL"}, {0xb5, "LAUNCH_MEDIA_SELECT"},
	{0xb6, "LAUNCH_APP1"}, {0xb7, "LAUNCH_APP2"},
	{0xba, "OEM_1"}, {0xbb, "OEM_PLUS"}, {0xbc, "OEM_COMMA"},
	{0xbd, "OEM_MINUS"}, {0xbe, "OEM_PERIOD"}, {0xbf, "OEM_2"},
	{0xc0, "OEM_3"}, {0xdb, "OEM_4"}, {0xdc, "OEM_5"},
	{0xdd, "OEM_6"}, {0xde, "OEM_7"}, {0xdf, "OEM_8"},
	{0xe1, "OEM_AX"}, {0xe2, "OEM_102"}, {0xe3, "ICO_HELP"},
	{0xe4, "ICO_00"}, {0xe5, "PROCESSKEY"}, {0xe6, "ICO_CLEAR"},
	{0xe7, "PACKET"}, {0xe9, "OEM_RESET"}, {0xea, "OEM_JUMP"},
	{0xeb, "OEM_PA1"}, {0xec, "OEM_PA2"}, {0xed, "OEM_PA3"},
	{0xee, "OEM_WSCTRL"}, {0xef, "OEM_CUSEL"}, {0xf0, "OEM_ATTN"},
	{0xf1, "OEM_FINISH"}, {0xf2, "OEM_COPY"}, {0xf3, "OEM_AUTO"},
	{0xf4, "OEM_ENLW"}, {0xf5, "OEM_BACKTAB"}, {0xf6, "ATTN"},
	{0xf7, "CRSEL"}, {0xf8, "EXSEL"}, {0xf9, "EREOF"},
	{0xfa, "PLAY"}, {0xfb, "ZOOM"}, {0xfc, "NONAME"},
	{0xfd, "PA1"}, {0xfe, "OEM_CLEAR"}
}};

constexpr bool asciiSpace(char value) noexcept
{
	return value == ' ' || value == '\t' || value == '\r' ||
		value == '\n' || value == '\f' || value == '\v';
}

constexpr char asciiUpper(char value) noexcept
{
	return value >= 'a' && value <= 'z'
		? static_cast<char>(value - ('a' - 'A')) : value;
}

std::string_view trim(std::string_view value) noexcept
{
	while (!value.empty() && asciiSpace(value.front())) value.remove_prefix(1);
	while (!value.empty() && asciiSpace(value.back())) value.remove_suffix(1);
	return value;
}

bool equalsIgnoringAsciiCase(
	std::string_view left, std::string_view right) noexcept
{
	if (left.size() != right.size()) return false;
	for (std::size_t index = 0; index < left.size(); ++index)
		if (asciiUpper(left[index]) != asciiUpper(right[index])) return false;
	return true;
}

std::uint8_t numericKey(std::string_view value) noexcept
{
	if (value.empty()) return 0;
	unsigned base = 10;
	std::size_t offset = 0;
	if (value.size() > 2 && value[0] == '0' &&
		(value[1] == 'x' || value[1] == 'X'))
	{
		base = 16;
		offset = 2;
	}
	else if (value.size() > 1 && value[0] == '0')
	{
		base = 8;
		offset = 1;
	}
	if (offset == value.size()) return 0;

	unsigned result = 0;
	for (; offset < value.size(); ++offset)
	{
		const char current = value[offset];
		unsigned digit = std::numeric_limits<unsigned>::max();
		if (current >= '0' && current <= '9')
			digit = static_cast<unsigned>(current - '0');
		else if (current >= 'a' && current <= 'f')
			digit = static_cast<unsigned>(current - 'a' + 10);
		else if (current >= 'A' && current <= 'F')
			digit = static_cast<unsigned>(current - 'A' + 10);
		if (digit >= base || result > (0xffu - digit) / base) return 0;
		result = result * base + digit;
	}
	return static_cast<std::uint8_t>(result);
}
}

std::uint8_t legacyVirtualKeyFromName(std::string_view name) noexcept
{
	name = trim(name);
	if (name.empty()) return 0;
	if (name.size() == 1)
	{
		const char value = asciiUpper(name.front());
		if ((value >= 'A' && value <= 'Z') ||
			(value >= '0' && value <= '9') || value == ':' ||
			value == ';' || value == '<' || value == '=' ||
			value == '>' || value == '?')
			return static_cast<std::uint8_t>(value);
	}
	if (name.size() >= 2 && asciiUpper(name.front()) == 'F')
	{
		unsigned functionNumber = 0;
		for (std::size_t index = 1; index < name.size(); ++index)
		{
			if (name[index] < '0' || name[index] > '9')
			{
				functionNumber = 0;
				break;
			}
			const unsigned digit =
				static_cast<unsigned>(name[index] - '0');
			// Values above F24 can never become valid again. Reject before
			// multiplying so a long decimal spelling cannot wrap unsigned and
			// accidentally alias F1..F24.
			if (functionNumber > (24u - digit) / 10u) return 0;
			functionNumber = functionNumber * 10u + digit;
		}
		if (functionNumber >= 1 && functionNumber <= 24)
			return static_cast<std::uint8_t>(0x70u + functionNumber - 1u);
	}
	for (const NamedKey& key : namedKeys)
		if (equalsIgnoringAsciiCase(name, key.name)) return key.value;
	return numericKey(name);
}

PackedKeyBinding parsePackedKeyBinding(std::string_view text) noexcept
{
	if (text.size() > maximumKeyBindingTextLength)
		text = text.substr(0, maximumKeyBindingTextLength);
	PackedKeyBinding result = 0;
	std::size_t count = 0;
	std::size_t begin = 0;
	while (begin <= text.size() && count < sizeof(PackedKeyBinding))
	{
		std::size_t end = text.find_first_of("|+", begin);
		if (end == std::string_view::npos) end = text.size();
		const std::uint8_t key = legacyVirtualKeyFromName(
			text.substr(begin, end - begin));
		if (key != 0)
		{
			result |= static_cast<PackedKeyBinding>(key) << (count * 8u);
			++count;
		}
		if (end == text.size()) break;
		begin = end + 1;
	}
	return result;
}

std::size_t unpackPackedKeyBinding(
	PackedKeyBinding binding, std::uint8_t* keys,
	std::size_t capacity) noexcept
{
	std::size_t count = 0;
	while (binding != 0 && count < sizeof(PackedKeyBinding))
	{
		const std::uint8_t key = static_cast<std::uint8_t>(binding & 0xffu);
		if (key == 0) break;
		if (keys != nullptr && count < capacity) keys[count] = key;
		++count;
		binding >>= 8u;
	}
	return count;
}

bool isPackedKeyBindingPressed(
	PackedKeyBinding binding,
	const LegacyKeyStateSource& source) noexcept
{
	if (binding == 0) return false;
	while (binding != 0)
	{
		const std::uint8_t key = static_cast<std::uint8_t>(binding & 0xffu);
		if (key == 0) break;
		if (!source.isPressed(key)) return false;
		binding >>= 8u;
	}
	return true;
}
}
