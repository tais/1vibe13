#include <types.h>

// The public value remains a little-endian sequence of up to four persisted
// Win32 virtual-key bytes. Hosts may bind an input adapter; SDL is the
// production default and parsing never depends on the compiled platform.
//
// Binding/restoring is a game-thread lifecycle operation and must not race
// IsKeyPressed. The caller owns context and must keep it alive until restoring
// the platform source. Callbacks are type-enforced nonthrowing because input
// polling crosses a noexcept model boundary.
//
// SDL3 has no state-bearing scancode for some persisted legacy IME/OEM and
// application-launch VK values. The default adapter safely reports those as
// not pressed; an injected host may still implement their persisted values.
typedef BOOLEAN (*JA2_KEY_STATE_SOURCE)(
	UINT8 virtualKey, void* context) noexcept;

extern int ParseKeyString(const STR value);
extern BOOLEAN IsKeyPressed(int value);
BOOLEAN BindJA2KeyStateSource(
	JA2_KEY_STATE_SOURCE source, void* context) noexcept;
void RestoreJA2PlatformKeyStateSource(void) noexcept;
