#ifndef _SDL_INPUT_H_
#define _SDL_INPUT_H_

// SDL3-driven input source. Translates a single SDL_Event into JA2
// input atoms by calling input.cpp's QueueEvent / KeyDown / KeyUp.
// Replaces the Win32 KeyboardHandler / MouseHandler hooks; the
// existing input atom queue, key-state tables, double-click tracking,
// and string-input redirection are all reused unchanged.

union SDL_Event;

// Returns true if the event was a window-close (SDL_EVENT_QUIT or a
// SDL_EVENT_WINDOW_CLOSE_REQUESTED), so the main loop can break.
bool SgpHandleSDLEvent(const SDL_Event* event);

#endif // _SDL_INPUT_H_
