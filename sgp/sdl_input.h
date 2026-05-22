#ifndef _SDL_INPUT_H_
#define _SDL_INPUT_H_

// SDL3-driven input source. Translates a single SDL_Event into JA2
// input atoms by calling input.cpp's QueueEvent / KeyDown / KeyUp.
// Replaces the Win32 KeyboardHandler / MouseHandler hooks; the
// existing input atom queue, key-state tables, double-click tracking,
// and string-input redirection are all reused unchanged.

union SDL_Event;
struct SDL_Renderer;

// Returns true if the event was a window-close (SDL_EVENT_QUIT or a
// SDL_EVENT_WINDOW_CLOSE_REQUESTED), so the main loop can break.
bool SgpHandleSDLEvent(const SDL_Event* event);

// The active SDL renderer (owned by the video manager). Exposed so the
// event pump can map window-space mouse coordinates into the renderer's
// logical 640x480 space via SDL_ConvertEventToRenderCoordinates.
SDL_Renderer* SGP_GetSDLRenderer(void);

#endif // _SDL_INPUT_H_
