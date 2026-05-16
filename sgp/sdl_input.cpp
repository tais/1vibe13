#include "sdl_input.h"
#include "types.h"

#include <SDL3/SDL.h>
#include <cstdio>

// Phase 4 first-cut SDL3 input source. This TU owns the SDL_Event ->
// JA2 event translation (scancode mapping, mouse-coord packing, JA2
// VK_* values) so the rest of the SDL3 main loop in sgp.cpp can stay
// tiny.
//
// In a properly-wired build the bottom of the file would call
// input.cpp's real QueueEvent and update gfKeyState / gusMouse{X,Y}Pos
// directly. Doing that today drags in ~55 unresolved symbols from
// video.cpp / vsurface.cpp / Intro.cpp / soundman.cpp / mousesystem
// because input.cpp's KeyDown / KeyUp bodies (and therefore the whole
// TU) reference PrintScreen / BltVideoSurface / IntroScreen* /
// SoundLoadSample / ... -- and several of those define classes whose
// private headers pull in DirectDraw. That cascade gets cleared as
// each phase lands its SDL3 rewrite of the underlying subsystem;
// until then we keep the JA2 input queue out of the loop and just
// print translated events to stderr.

// JA2 event-code constants. Mirror input.h's #defines locally so this
// TU stays decoupled from input.cpp until the wire-up lands.
#define KEY_DOWN                  0x0001
#define KEY_UP                    0x0002
#define LEFT_BUTTON_DOWN          0x0008
#define LEFT_BUTTON_UP            0x0010
#define RIGHT_BUTTON_DOWN         0x0080
#define RIGHT_BUTTON_UP           0x0100
#define MOUSE_POS                 0x0400
#define MOUSE_WHEEL_UP            0x0800
#define MOUSE_WHEEL_DOWN          0x1000
#define MIDDLE_BUTTON_DOWN        0x2000
#define MIDDLE_BUTTON_UP          0x4000
#define X1_BUTTON_DOWN            0x8010
#define X1_BUTTON_UP              0x8020
#define X2_BUTTON_DOWN            0x8040
#define X2_BUTTON_UP              0x8050

// Mouse-position cache. When the wire-up lands these get pointed at
// input.cpp's gusMouseXPos / gusMouseYPos extern globals.
static INT16 sMouseXPos = 0;
static INT16 sMouseYPos = 0;

static void DispatchToInputQueue(UINT16 ev, UINT32 usParam, UINT32 uiParam)
{
	// Debug-only sink while the real input.cpp wire-up is blocked
	// behind Phase 5. The translation is correct; the events just go
	// to stderr instead of into the JA2 event queue.
	std::fprintf(stderr,
	             "[sdl-input] ev=0x%04x usParam=%u uiParam=0x%08x\n",
	             ev, usParam, uiParam);
}

// Translate SDL_Scancode + SDL_Keycode to JA2's persisted VK_* code.
// JA2 stores these in savegames and config files so SDL scancodes
// can't be handed through directly. Covers the keys the menu / game
// systems actually look at; alphanumerics fall through to the
// keycode path below.
static UINT16 sdl_to_vk(SDL_Scancode sc, SDL_Keycode key)
{
	switch (sc) {
		case SDL_SCANCODE_ESCAPE:    return 0x1B;
		case SDL_SCANCODE_RETURN:    return 0x0D;
		case SDL_SCANCODE_KP_ENTER:  return 0x0D;
		case SDL_SCANCODE_SPACE:     return 0x20;
		case SDL_SCANCODE_TAB:       return 0x09;
		case SDL_SCANCODE_BACKSPACE: return 0x08;
		case SDL_SCANCODE_DELETE:    return 0x2E;
		case SDL_SCANCODE_INSERT:    return 0x2D;
		case SDL_SCANCODE_HOME:      return 0x24;
		case SDL_SCANCODE_END:       return 0x23;
		case SDL_SCANCODE_PAGEUP:    return 0x21;
		case SDL_SCANCODE_PAGEDOWN:  return 0x22;
		case SDL_SCANCODE_LEFT:      return 0x25;
		case SDL_SCANCODE_UP:        return 0x26;
		case SDL_SCANCODE_RIGHT:     return 0x27;
		case SDL_SCANCODE_DOWN:      return 0x28;
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

	case SDL_EVENT_KEY_DOWN: {
		UINT16 vk = sdl_to_vk(ev->key.scancode, ev->key.key);
		if (vk) DispatchToInputQueue(KEY_DOWN, vk, 0);
		break;
	}
	case SDL_EVENT_KEY_UP: {
		UINT16 vk = sdl_to_vk(ev->key.scancode, ev->key.key);
		if (vk) DispatchToInputQueue(KEY_UP, vk, 0);
		break;
	}

	case SDL_EVENT_MOUSE_MOTION: {
		sMouseXPos = (INT16)ev->motion.x;
		sMouseYPos = (INT16)ev->motion.y;
		DispatchToInputQueue(MOUSE_POS, 0,
		                     pack_xy((int)ev->motion.x, (int)ev->motion.y));
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		const bool down = ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
		const UINT32 xy = pack_xy((int)ev->button.x, (int)ev->button.y);
		UINT16 jaev = 0;
		switch (ev->button.button) {
		case SDL_BUTTON_LEFT:   jaev = down ? LEFT_BUTTON_DOWN   : LEFT_BUTTON_UP;   break;
		case SDL_BUTTON_RIGHT:  jaev = down ? RIGHT_BUTTON_DOWN  : RIGHT_BUTTON_UP;  break;
		case SDL_BUTTON_MIDDLE: jaev = down ? MIDDLE_BUTTON_DOWN : MIDDLE_BUTTON_UP; break;
		case SDL_BUTTON_X1:     jaev = down ? X1_BUTTON_DOWN     : X1_BUTTON_UP;     break;
		case SDL_BUTTON_X2:     jaev = down ? X2_BUTTON_DOWN     : X2_BUTTON_UP;     break;
		default: break;
		}
		if (jaev) DispatchToInputQueue(jaev, 0, xy);
		break;
	}
	case SDL_EVENT_MOUSE_WHEEL: {
		const UINT32 xy = pack_xy(sMouseXPos, sMouseYPos);
		if (ev->wheel.y > 0) DispatchToInputQueue(MOUSE_WHEEL_UP,   0, xy);
		if (ev->wheel.y < 0) DispatchToInputQueue(MOUSE_WHEEL_DOWN, 0, xy);
		break;
	}
	default: break;
	}
	return false;
}
