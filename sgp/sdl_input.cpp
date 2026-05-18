#include "sdl_input.h"
#include "types.h"
#include "input.h"
#include "video.h"  // InvalidateScreen()

#include <SDL3/SDL.h>

// SDL3 input source. Translates a single SDL_Event into JA2 input
// atoms by calling input.cpp's QueueEvent and updating the global
// gfKeyState / button-state / wheel-delta tables. Replaces the Win32
// KeyboardHandler / MouseHandler hooks; the existing input atom
// queue and string-input redirection are reused unchanged.

// Translate SDL_Scancode + SDL_Keycode to JA2's gfKeyState[] index.
//
// Critically: gfKeyState is indexed by JA2's *english.h symbolic* key
// codes (UPARROW=253, DNARROW=248, HOME=252, etc.) -- not by Win32 VK
// codes. The original Win32 keyboard handler in input.cpp's KeyChange()
// translates Win32 VK codes to those symbolic indices before storing
// state. Polling code uses _KeyDown(UPARROW) which is gfKeyState[253],
// so the SDL side must produce the same indices for the polls to fire.
// Alphanumerics and a handful of control keys (ESC/RET/TAB/BS/space)
// happen to share the same numeric value across VK and english.h, so
// they pass through unchanged.
static UINT16 sdl_to_vk(SDL_Scancode sc, SDL_Keycode key)
{
	switch (sc) {
		case SDL_SCANCODE_ESCAPE:    return 27;   // ESC
		case SDL_SCANCODE_RETURN:    return 13;   // ENTER
		case SDL_SCANCODE_KP_ENTER:  return 13;
		case SDL_SCANCODE_SPACE:     return 32;   // SPACE
		case SDL_SCANCODE_TAB:       return 9;    // TAB
		case SDL_SCANCODE_BACKSPACE: return 8;    // BACKSPACE
		// english.h symbolic codes (sgp/english.h):
		case SDL_SCANCODE_INSERT:    return 245;  // INSERT
		case SDL_SCANCODE_DELETE:    return 246;  // DEL
		case SDL_SCANCODE_END:       return 247;  // END
		case SDL_SCANCODE_DOWN:      return 248;  // DNARROW
		case SDL_SCANCODE_PAGEDOWN:  return 249;  // PGDN
		case SDL_SCANCODE_LEFT:      return 250;  // LEFTARROW
		case SDL_SCANCODE_RIGHT:     return 251;  // RIGHTARROW
		case SDL_SCANCODE_HOME:      return 252;  // HOME
		case SDL_SCANCODE_UP:        return 253;  // UPARROW
		case SDL_SCANCODE_PAGEUP:    return 254;  // PGUP
		case SDL_SCANCODE_LSHIFT:    return 0xA0;
		case SDL_SCANCODE_RSHIFT:    return 0xA1;
		case SDL_SCANCODE_LCTRL:     return 0xA2;
		case SDL_SCANCODE_RCTRL:     return 0xA3;
		case SDL_SCANCODE_LALT:      return 0xA4;
		case SDL_SCANCODE_RALT:      return 0xA5;
		case SDL_SCANCODE_F1:        return 0x70;
		case SDL_SCANCODE_F2:        return 0x71;
		case SDL_SCANCODE_F3:        return 0x72;
		case SDL_SCANCODE_F4:        return 0x73;
		case SDL_SCANCODE_F5:        return 0x74;
		case SDL_SCANCODE_F6:        return 0x75;
		case SDL_SCANCODE_F7:        return 0x76;
		case SDL_SCANCODE_F8:        return 0x77;
		case SDL_SCANCODE_F9:        return 0x78;
		case SDL_SCANCODE_F10:       return 0x79;
		case SDL_SCANCODE_F11:       return 0x7A;
		case SDL_SCANCODE_F12:       return 0x7B;
		default: break;
	}
	// ASCII printable -> VK_* for alphanumerics matches up (Win32
	// VK_A..VK_Z are 0x41..0x5A, VK_0..VK_9 are 0x30..0x39).
	if (key >= 0x20 && key < 0x7F) {
		return (UINT16)(key >= 'a' && key <= 'z' ? key - 32 : key);
	}
	return 0;
}

// Pack mouse coords the same way the Win32 hook packed them into
// LPARAM: y<<16 | x. JA2 unpacks via _EvMouseX/_EvMouseY macros that
// expect this layout.
static UINT32 pack_xy(int x, int y)
{
	return (UINT32)((y & 0xFFFF) << 16) | (UINT32)(x & 0xFFFF);
}

bool SgpHandleSDLEvent(const SDL_Event* ev)
{
	if (!ev) return false;
	switch (ev->type) {
	case SDL_EVENT_QUIT:
	case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		return true;

	case SDL_EVENT_WINDOW_FOCUS_GAINED:
	case SDL_EVENT_WINDOW_EXPOSED:
	case SDL_EVENT_WINDOW_RESTORED:
		// macOS will re-show the system arrow on focus events and may
		// drop the streaming texture's stored content. Re-hide the OS
		// cursor and force a full-screen invalidate so the next frame
		// re-uploads everything cleanly.
		SDL_HideCursor();
		InvalidateScreen();
		break;

	case SDL_EVENT_KEY_DOWN: {
		// Cmd+Q on macOS is normally translated by the OS into
		// SDL_EVENT_QUIT, but we accept it explicitly in case the
		// translation gets dropped. ESC is NOT a quit path -- it
		// queues as VK_ESCAPE so per-screen handlers (laptop, popups,
		// game UI, etc.) can close their own thing.
		if ((ev->key.key == SDLK_Q) && (ev->key.mod & SDL_KMOD_GUI)) return true;

		UINT16 vk = sdl_to_vk(ev->key.scancode, ev->key.key);
		if (vk) {
			// Update gfKeyState directly instead of calling KeyDown,
			// which has a JA2BETAVERSION-only Ctrl+F12 PrintScreen
			// path that pulls in the video subsystem. The queue +
			// _KeyDown(vk) callers see the same state either way.
			gfKeyState[vk & 0xFF] = TRUE;
			QueueEvent(KEY_DOWN, vk, 0);
		}
		break;
	}
	case SDL_EVENT_KEY_UP: {
		UINT16 vk = sdl_to_vk(ev->key.scancode, ev->key.key);
		if (vk) {
			gfKeyState[vk & 0xFF] = FALSE;
			QueueEvent(KEY_UP, vk, 0);
		}
		break;
	}

	case SDL_EVENT_MOUSE_MOTION: {
		// NB: do NOT QueueEvent(MOUSE_POS, ...) here. The legacy Win32
		// mouse hook only queued button/wheel atoms and let the game
		// poll gusMouseXPos/gusMouseYPos directly. DequeueSpecificEvent
		// in Ja2/gameloop.cpp peeks the queue head and bails when it
		// doesn't match the mask, so a MOUSE_POS at the head would
		// pin button events behind it forever (clicks would register
		// in gfLeftButtonState but the button-system hooks never run).
		gusMouseXPos = (INT16)ev->motion.x;
		gusMouseYPos = (INT16)ev->motion.y;
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		const bool down = ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
		const UINT32 xy = pack_xy((int)ev->button.x, (int)ev->button.y);
		UINT16 jaev = 0;
		switch (ev->button.button) {
		case SDL_BUTTON_LEFT:
			gfLeftButtonState = down ? TRUE : FALSE;
			jaev = down ? LEFT_BUTTON_DOWN : LEFT_BUTTON_UP;
			break;
		case SDL_BUTTON_RIGHT:
			gfRightButtonState = down ? TRUE : FALSE;
			jaev = down ? RIGHT_BUTTON_DOWN : RIGHT_BUTTON_UP;
			break;
		case SDL_BUTTON_MIDDLE:
			gfMiddleButtonState = down ? TRUE : FALSE;
			jaev = down ? MIDDLE_BUTTON_DOWN : MIDDLE_BUTTON_UP;
			break;
		case SDL_BUTTON_X1: jaev = down ? X1_BUTTON_DOWN : X1_BUTTON_UP; break;
		case SDL_BUTTON_X2: jaev = down ? X2_BUTTON_DOWN : X2_BUTTON_UP; break;
		default: break;
		}
		if (jaev) QueueEvent(jaev, 0, xy);
		break;
	}
	case SDL_EVENT_MOUSE_WHEEL: {
		const UINT32 xy = pack_xy(gusMouseXPos, gusMouseYPos);
		if (ev->wheel.y > 0) {
			gsMouseWheelDeltaValue = 120;
			QueueEvent(MOUSE_WHEEL_UP, 0, xy);
		} else if (ev->wheel.y < 0) {
			gsMouseWheelDeltaValue = -120;
			QueueEvent(MOUSE_WHEEL_DOWN, 0, xy);
		}
		break;
	}
	default: break;
	}
	return false;
}
