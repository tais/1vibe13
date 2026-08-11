#include "KeyMap.h"

#include "KeyBindingModel.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstring>

namespace
{
using ja2::runtime_control::LegacyKeyStateSource;

std::size_t boundedKeyBindingTextLength(const char* value) noexcept
{
	std::size_t length = 0;
	while (length < ja2::runtime_control::maximumKeyBindingTextLength &&
		value[length] != '\0')
		++length;
	return length;
}

SDL_Scancode virtualKeyToScancode(std::uint8_t key) noexcept
{
	if (key >= 'A' && key <= 'Z')
		return static_cast<SDL_Scancode>(SDL_SCANCODE_A + key - 'A');
	if (key >= '1' && key <= '9')
		return static_cast<SDL_Scancode>(SDL_SCANCODE_1 + key - '1');
	if (key == '0') return SDL_SCANCODE_0;
	if (key >= 0x70 && key <= 0x7b)
		return static_cast<SDL_Scancode>(SDL_SCANCODE_F1 + key - 0x70);
	if (key >= 0x7c && key <= 0x87)
		return static_cast<SDL_Scancode>(SDL_SCANCODE_F13 + key - 0x7c);
	if (key >= 0x60 && key <= 0x69)
	{
		if (key == 0x60) return SDL_SCANCODE_KP_0;
		return static_cast<SDL_Scancode>(SDL_SCANCODE_KP_1 + key - 0x61);
	}

	switch (key)
	{
	case 0x03: return SDL_SCANCODE_CANCEL;
	case 0x08: return SDL_SCANCODE_BACKSPACE;
	case 0x09: return SDL_SCANCODE_TAB;
	case 0x0c: return SDL_SCANCODE_KP_CLEAR;
	case 0x0d: return SDL_SCANCODE_RETURN;
	case 0x13: return SDL_SCANCODE_PAUSE;
	case 0x14: return SDL_SCANCODE_CAPSLOCK;
	case 0x15: return SDL_SCANCODE_LANG1;
	case 0x19: return SDL_SCANCODE_LANG2;
	case 0x1b: return SDL_SCANCODE_ESCAPE;
	case 0x1c: return SDL_SCANCODE_INTERNATIONAL4;
	case 0x1d: return SDL_SCANCODE_INTERNATIONAL5;
	case 0x20: return SDL_SCANCODE_SPACE;
	case 0x21: return SDL_SCANCODE_PAGEUP;
	case 0x22: return SDL_SCANCODE_PAGEDOWN;
	case 0x23: return SDL_SCANCODE_END;
	case 0x24: return SDL_SCANCODE_HOME;
	case 0x25: return SDL_SCANCODE_LEFT;
	case 0x26: return SDL_SCANCODE_UP;
	case 0x27: return SDL_SCANCODE_RIGHT;
	case 0x28: return SDL_SCANCODE_DOWN;
	case 0x29: return SDL_SCANCODE_SELECT;
	case 0x2a: return SDL_SCANCODE_PRINTSCREEN;
	case 0x2b: return SDL_SCANCODE_EXECUTE;
	case 0x2c: return SDL_SCANCODE_PRINTSCREEN;
	case 0x2d: return SDL_SCANCODE_INSERT;
	case 0x2e: return SDL_SCANCODE_DELETE;
	case 0x2f: return SDL_SCANCODE_HELP;
	case ':': return SDL_SCANCODE_SEMICOLON;
	case ';': return SDL_SCANCODE_SEMICOLON;
	case '<': return SDL_SCANCODE_COMMA;
	case '=': return SDL_SCANCODE_EQUALS;
	case '>': return SDL_SCANCODE_PERIOD;
	case '?': return SDL_SCANCODE_SLASH;
	case 0x5b: return SDL_SCANCODE_LGUI;
	case 0x5c: return SDL_SCANCODE_RGUI;
	case 0x5d: return SDL_SCANCODE_APPLICATION;
	case 0x5f: return SDL_SCANCODE_SLEEP;
	case 0x6a: return SDL_SCANCODE_KP_MULTIPLY;
	case 0x6b: return SDL_SCANCODE_KP_PLUS;
	case 0x6c: return SDL_SCANCODE_KP_COMMA;
	case 0x6d: return SDL_SCANCODE_KP_MINUS;
	case 0x6e: return SDL_SCANCODE_KP_PERIOD;
	case 0x6f: return SDL_SCANCODE_KP_DIVIDE;
	case 0x90: return SDL_SCANCODE_NUMLOCKCLEAR;
	case 0x91: return SDL_SCANCODE_SCROLLLOCK;
	case 0x92: return SDL_SCANCODE_KP_EQUALS;
	case 0xa6: return SDL_SCANCODE_AC_BACK;
	case 0xa7: return SDL_SCANCODE_AC_FORWARD;
	case 0xa8: return SDL_SCANCODE_AC_REFRESH;
	case 0xa9: return SDL_SCANCODE_AC_STOP;
	case 0xaa: return SDL_SCANCODE_AC_SEARCH;
	case 0xab: return SDL_SCANCODE_AC_BOOKMARKS;
	case 0xac: return SDL_SCANCODE_AC_HOME;
	case 0xad: return SDL_SCANCODE_MUTE;
	case 0xae: return SDL_SCANCODE_VOLUMEDOWN;
	case 0xaf: return SDL_SCANCODE_VOLUMEUP;
	case 0xb0: return SDL_SCANCODE_MEDIA_NEXT_TRACK;
	case 0xb1: return SDL_SCANCODE_MEDIA_PREVIOUS_TRACK;
	case 0xb2: return SDL_SCANCODE_MEDIA_STOP;
	case 0xb3: return SDL_SCANCODE_MEDIA_PLAY_PAUSE;
	case 0xb4: return SDL_SCANCODE_UNKNOWN;
	case 0xb5: return SDL_SCANCODE_MEDIA_SELECT;
	case 0xba: return SDL_SCANCODE_SEMICOLON;
	case 0xbb: return SDL_SCANCODE_EQUALS;
	case 0xbc: return SDL_SCANCODE_COMMA;
	case 0xbd: return SDL_SCANCODE_MINUS;
	case 0xbe: return SDL_SCANCODE_PERIOD;
	case 0xbf: return SDL_SCANCODE_SLASH;
	case 0xc0: return SDL_SCANCODE_GRAVE;
	case 0xdb: return SDL_SCANCODE_LEFTBRACKET;
	case 0xdc: return SDL_SCANCODE_BACKSLASH;
	case 0xdd: return SDL_SCANCODE_RIGHTBRACKET;
	case 0xde: return SDL_SCANCODE_APOSTROPHE;
	case 0xe2: return SDL_SCANCODE_NONUSBACKSLASH;
	case 0xf2: return SDL_SCANCODE_COPY;
	case 0xfa: return SDL_SCANCODE_MEDIA_PLAY;
	// SDL3 does not expose state-bearing scancodes for the remaining
	// persisted Win32 IME/OEM/application-launch values. Keeping them in
	// the codec preserves stored configuration and injected-host parity;
	// the platform adapter deliberately treats them as unavailable.
	default: return SDL_SCANCODE_UNKNOWN;
	}
}

class SdlLegacyKeyStateSource final : public LegacyKeyStateSource
{
public:
	bool isPressed(std::uint8_t key) const noexcept override
	{
		const SDL_Keymod modifiers = SDL_GetModState();
		switch (key)
		{
		case 0x01: return (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0;
		case 0x02: return (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_RMASK) != 0;
		case 0x04: return (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MMASK) != 0;
		case 0x05: return (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_X1MASK) != 0;
		case 0x06: return (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_X2MASK) != 0;
		case 0x10: return (modifiers & SDL_KMOD_SHIFT) != 0;
		case 0x11: return (modifiers & SDL_KMOD_CTRL) != 0;
		case 0x12: return (modifiers & SDL_KMOD_ALT) != 0;
		case 0xa0: return (modifiers & SDL_KMOD_LSHIFT) != 0;
		case 0xa1: return (modifiers & SDL_KMOD_RSHIFT) != 0;
		case 0xa2: return (modifiers & SDL_KMOD_LCTRL) != 0;
		case 0xa3: return (modifiers & SDL_KMOD_RCTRL) != 0;
		case 0xa4: return (modifiers & SDL_KMOD_LALT) != 0;
		case 0xa5: return (modifiers & SDL_KMOD_RALT) != 0;
		default: break;
		}

		const SDL_Scancode scancode = virtualKeyToScancode(key);
		if (scancode == SDL_SCANCODE_UNKNOWN) return false;
		int keyCount = 0;
		const bool* state = SDL_GetKeyboardState(&keyCount);
		return state != nullptr && static_cast<int>(scancode) < keyCount &&
			state[scancode];
	}
};

class BoundLegacyKeyStateSource final : public LegacyKeyStateSource
{
public:
	void bind(JA2_KEY_STATE_SOURCE source, void* context) noexcept
	{
		source_ = source;
		context_ = context;
	}
	bool isPressed(std::uint8_t key) const noexcept override
	{
		return source_ != nullptr && source_(key, context_) != FALSE;
	}

private:
	JA2_KEY_STATE_SOURCE source_ = nullptr;
	void* context_ = nullptr;
};

SdlLegacyKeyStateSource gPlatformKeyStateSource;
BoundLegacyKeyStateSource gBoundKeyStateSource;
const LegacyKeyStateSource* gKeyStateSource = &gPlatformKeyStateSource;
}

BOOLEAN BindJA2KeyStateSource(
	JA2_KEY_STATE_SOURCE source, void* context) noexcept
{
	if (source == nullptr) return FALSE;
	gBoundKeyStateSource.bind(source, context);
	gKeyStateSource = &gBoundKeyStateSource;
	return TRUE;
}

void RestoreJA2PlatformKeyStateSource() noexcept
{
	gKeyStateSource = &gPlatformKeyStateSource;
	gBoundKeyStateSource.bind(nullptr, nullptr);
}

BOOLEAN IsKeyPressed(int value)
{
	return ja2::runtime_control::isPackedKeyBindingPressed(
		static_cast<std::uint32_t>(value), *gKeyStateSource)
		? TRUE : FALSE;
}

int ParseKeyString(const STR value)
{
	if (value == nullptr) return 0;
	const std::string_view boundedValue(
		value, boundedKeyBindingTextLength(value));
	const std::uint32_t packed =
		ja2::runtime_control::parsePackedKeyBinding(boundedValue);
	static_assert(sizeof(packed) == sizeof(int),
		"legacy packed key bindings require a 32-bit int");
	int result = 0;
	std::memcpy(&result, &packed, sizeof(result));
	return result;
}
