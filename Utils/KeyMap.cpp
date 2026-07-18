#include "KeyMap.h"
#include "Text.h"

#ifdef _WIN32
// VK_* virtual-key code table from Win32. Phase 4 (input migration)
// will replace this with an SDL3-scancode-based table that exports
// the same VK_* numeric values for save-game compatibility.
#include <windows.h>

static Str8EnumLookupType gKeyTable[] =
{
	{VK_LBUTTON, "LBUTTON"},
	{VK_RBUTTON, "RBUTTON"},
	{VK_CANCEL, "CANCEL"},
	{VK_MBUTTON, "MBUTTON"},
	#if(_WIN32_WINNT >= 0x0500)
	{VK_XBUTTON1, "XBUTTON1"},
	{VK_XBUTTON2, "XBUTTON2"},
	#endif /* _WIN32_WINNT >= 0x0500 */
	{VK_BACK, "BACK"},
	{VK_TAB, "TAB"},
	{VK_CLEAR, "CLEAR"},
	{VK_RETURN, "RETURN"},
	{VK_SHIFT, "SHIFT"},
	{VK_CONTROL, "CONTROL"},
	{VK_MENU, "MENU"},
	{VK_PAUSE, "PAUSE"},
	{VK_CAPITAL, "CAPITAL"},
	{VK_KANA, "KANA"},
	{VK_HANGEUL, "HANGEUL"},
	{VK_HANGUL, "HANGUL"},
	{VK_JUNJA, "JUNJA"},
	{VK_FINAL, "FINAL"},
	{VK_HANJA, "HANJA"},
	{VK_KANJI, "KANJI"},
	{VK_ESCAPE, "ESCAPE"},
	{VK_CONVERT, "CONVERT"},
	{VK_NONCONVERT, "NONCONVERT"},
	{VK_ACCEPT, "ACCEPT"},
	{VK_MODECHANGE, "MODECHANGE"},
	{VK_SPACE, "SPACE"},
	{VK_PRIOR, "PRIOR"},
	{VK_NEXT, "NEXT"},
	{VK_END, "END"},
	{VK_HOME, "HOME"},
	{VK_LEFT, "LEFT"},
	{VK_UP, "UP"},
	{VK_RIGHT, "RIGHT"},
	{VK_DOWN, "DOWN"},
	{VK_SELECT, "SELECT"},
	{VK_PRINT, "PRINT"},
	{VK_EXECUTE, "EXECUTE"},
	{VK_SNAPSHOT, "SNAPSHOT"},
	{VK_INSERT, "INSERT"},
	{VK_DELETE, "DELETE"},
	{VK_HELP, "HELP"},

	{ '0', "0"},
	{ '1', "1"},
	{ '2', "2"},
	{ '3', "3"},
	{ '4', "4"},
	{ '5', "5"},
	{ '6', "6"},
	{ '7', "7"},
	{ '8', "8"},
	{ '9', "9"},
	{ ':', ":"},
	{ ';', ";"},
	{ '<', "<"},
	{ '=', "="},
	{ '>', ">"},
	{ '?', "?"},
	{ 'A', "A"},
	{ 'B', "B"},
	{ 'C', "C"},
	{ 'D', "D"},
	{ 'E', "E"},
	{ 'F', "F"},
	{ 'G', "G"},
	{ 'H', "H"},
	{ 'I', "I"},
	{ 'J', "J"},
	{ 'K', "K"},
	{ 'L', "L"},
	{ 'M', "M"},
	{ 'N', "N"},
	{ 'O', "O"},
	{ 'P', "P"},
	{ 'Q', "Q"},
	{ 'R', "R"},
	{ 'S', "S"},
	{ 'T', "T"},
	{ 'U', "U"},
	{ 'V', "V"},
	{ 'W', "W"},
	{ 'X', "X"},
	{ 'Y', "Y"},
	{ 'Z', "Z"},

	{VK_LWIN, "LWIN"},
	{VK_RWIN, "RWIN"},
	{VK_APPS, "APPS"},
	{VK_SLEEP, "SLEEP"},
	{VK_NUMPAD0, "NUMPAD0"},
	{VK_NUMPAD1, "NUMPAD1"},
	{VK_NUMPAD2, "NUMPAD2"},
	{VK_NUMPAD3, "NUMPAD3"},
	{VK_NUMPAD4, "NUMPAD4"},
	{VK_NUMPAD5, "NUMPAD5"},
	{VK_NUMPAD6, "NUMPAD6"},
	{VK_NUMPAD7, "NUMPAD7"},
	{VK_NUMPAD8, "NUMPAD8"},
	{VK_NUMPAD9, "NUMPAD9"},
	{VK_MULTIPLY, "MULTIPLY"},
	{VK_ADD, "ADD"},
	{VK_SEPARATOR, "SEPARATOR"},
	{VK_SUBTRACT, "SUBTRACT"},
	{VK_DECIMAL, "DECIMAL"},
	{VK_DIVIDE, "DIVIDE"},
	{VK_F1, "F1"},
	{VK_F2, "F2"},
	{VK_F3, "F3"},
	{VK_F4, "F4"},
	{VK_F5, "F5"},
	{VK_F6, "F6"},
	{VK_F7, "F7"},
	{VK_F8, "F8"},
	{VK_F9, "F9"},
	{VK_F10, "F10"},
	{VK_F11, "F11"},
	{VK_F12, "F12"},
	{VK_F13, "F13"},
	{VK_F14, "F14"},
	{VK_F15, "F15"},
	{VK_F16, "F16"},
	{VK_F17, "F17"},
	{VK_F18, "F18"},
	{VK_F19, "F19"},
	{VK_F20, "F20"},
	{VK_F21, "F21"},
	{VK_F22, "F22"},
	{VK_F23, "F23"},
	{VK_F24, "F24"},
	{VK_NUMLOCK, "NUMLOCK"},
	{VK_SCROLL, "SCROLL"},
	{VK_OEM_NEC_EQUAL, "OEM_NEC_EQUAL"},
	{VK_OEM_FJ_JISHO, "OEM_FJ_JISHO"},
	{VK_OEM_FJ_MASSHOU, "OEM_FJ_MASSHOU"},
	{VK_OEM_FJ_TOUROKU, "OEM_FJ_TOUROKU"},
	{VK_OEM_FJ_LOYA, "OEM_FJ_LOYA"},
	{VK_OEM_FJ_ROYA, "OEM_FJ_ROYA"},
	{VK_LSHIFT, "LSHIFT"},
	{VK_RSHIFT, "RSHIFT"},
	{VK_LCONTROL, "LCONTROL"},
	{VK_RCONTROL, "RCONTROL"},
	{VK_LMENU, "LMENU"},
	{VK_RMENU, "RMENU"},

	#if(_WIN32_WINNT >= 0x0500)
	{VK_BROWSER_BACK, "BROWSER_BACK"},
	{VK_BROWSER_FORWARD, "BROWSER_FORWARD"},
	{VK_BROWSER_REFRESH, "BROWSER_REFRESH"},
	{VK_BROWSER_STOP, "BROWSER_STOP"},
	{VK_BROWSER_SEARCH, "BROWSER_SEARCH"},
	{VK_BROWSER_FAVORITES, "BROWSER_FAVORITES"},
	{VK_BROWSER_HOME, "BROWSER_HOME"},

	{VK_VOLUME_MUTE, "VOLUME_MUTE"},
	{VK_VOLUME_DOWN, "VOLUME_DOWN"},
	{VK_VOLUME_UP, "VOLUME_UP"},
	{VK_MEDIA_NEXT_TRACK, "MEDIA_NEXT_TRACK"},
	{VK_MEDIA_PREV_TRACK, "MEDIA_PREV_TRACK"},
	{VK_MEDIA_STOP, "MEDIA_STOP"},
	{VK_MEDIA_PLAY_PAUSE, "MEDIA_PLAY_PAUSE"},
	{VK_LAUNCH_MAIL, "LAUNCH_MAIL"},
	{VK_LAUNCH_MEDIA_SELECT, "LAUNCH_MEDIA_SELECT"},
	{VK_LAUNCH_APP1, "LAUNCH_APP1"},
	{VK_LAUNCH_APP2, "LAUNCH_APP2"},

	#endif /* _WIN32_WINNT >= 0x0500 */

	{VK_OEM_1, "OEM_1"},
	{VK_OEM_PLUS, "OEM_PLUS"},
	{VK_OEM_COMMA, "OEM_COMMA"},
	{VK_OEM_MINUS, "OEM_MINUS"},
	{VK_OEM_PERIOD, "OEM_PERIOD"},
	{VK_OEM_2, "OEM_2"},
	{VK_OEM_3, "OEM_3"},
	{VK_OEM_4, "OEM_4"},
	{VK_OEM_5, "OEM_5"},
	{VK_OEM_6, "OEM_6"},
	{VK_OEM_7, "OEM_7"},
	{VK_OEM_8, "OEM_8"},
	{VK_OEM_AX, "OEM_AX"},
	{VK_OEM_102, "OEM_102"},
	{VK_ICO_HELP, "ICO_HELP"},
	{VK_ICO_00, "ICO_00"},
	#if(WINVER >= 0x0400)
	{VK_PROCESSKEY, "PROCESSKEY"},
	#endif /* WINVER >= 0x0400 */
	{VK_ICO_CLEAR, "ICO_CLEAR"},
	#if(_WIN32_WINNT >= 0x0500)
	{VK_PACKET, "PACKET"},
	#endif /* _WIN32_WINNT >= 0x0500 */
	{VK_OEM_RESET, "OEM_RESET"},
	{VK_OEM_JUMP, "OEM_JUMP"},
	{VK_OEM_PA1, "OEM_PA1"},
	{VK_OEM_PA2, "OEM_PA2"},
	{VK_OEM_PA3, "OEM_PA3"},
	{VK_OEM_WSCTRL, "OEM_WSCTRL"},
	{VK_OEM_CUSEL, "OEM_CUSEL"},
	{VK_OEM_ATTN, "OEM_ATTN"},
	{VK_OEM_FINISH, "OEM_FINISH"},
	{VK_OEM_COPY, "OEM_COPY"},
	{VK_OEM_AUTO, "OEM_AUTO"},
	{VK_OEM_ENLW, "OEM_ENLW"},
	{VK_OEM_BACKTAB, "OEM_BACKTAB"},
	{VK_ATTN, "ATTN"},
	{VK_CRSEL, "CRSEL"},
	{VK_EXSEL, "EXSEL"},
	{VK_EREOF, "EREOF"},
	{VK_PLAY, "PLAY"},
	{VK_ZOOM, "ZOOM"},
	{VK_NONAME, "NONAME"},
	{VK_PA1, "PA1"},
	{VK_OEM_CLEAR, "OEM_CLEAR"},
	{0, NULL}
};

static inline CHAR8 *Trim(CHAR8 *&p) {
	while(*p && isspace((unsigned char)*p)) *p++ = 0;
	const size_t length = strlen(p);
	if (length == 0)
		return p;
	CHAR8 *e = p + length;
	while (e > p && isspace((unsigned char)e[-1])) *--e = 0;
	return p;
}

// Checks VK for each byte in the integer, all must be true to return TRUE.
extern BOOLEAN IsKeyPressed(int value)
{
	if (!value)
		return 0;

	BOOLEAN ok = 0;
	UINT8* ptr = (UINT8*)&value;
	int len = sizeof(int) / sizeof(UINT8);
	for (int i=0;i<len && ptr[i];++i)
	{
		if ( 0 != GetAsyncKeyState( (INT32)ptr[i] ) )
			ok = 1;
		else
			return 0;
	}
	return ok;
}


extern int ParseKeyString(const STR value)
{
	if (!value)
		return 0;

	STRING512 buffer;
	strncpy(buffer, value, _countof(buffer));
	buffer[_countof(buffer)-1] = 0;
	int iresult = 0;
	int idx = 0;
	UINT8* ptr = (UINT8*)&iresult;
	const STR sDelims = "|+";
	for ( CHAR8 *key = strtok(buffer, sDelims);
	      key != NULL && idx < (int)sizeof(iresult);
	      key = strtok(NULL, sDelims) )
	{
		Trim(key);
		if (!*key)
			continue;
		int ichr = StringToEnum(key, gKeyTable);
		if (ichr > 0 && ichr <= 0xFF)
			ptr[idx++] = (UINT8)ichr;
	}
	return iresult;
}
#else

#include <SDL3/SDL.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Portable key polling retains the packed Win32 VK values used by existing
// JA2 configuration strings, translating them to SDL scancodes at the edge.
static bool IsPortableKeyPressed(UINT8 key)
{
	const SDL_Keymod modifiers = SDL_GetModState();
	if (key == 0x10) return (modifiers & SDL_KMOD_SHIFT) != 0;
	if (key == 0x11) return (modifiers & SDL_KMOD_CTRL) != 0;
	if (key == 0x12) return (modifiers & SDL_KMOD_ALT) != 0;

	SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
	if (key >= 'A' && key <= 'Z')
		scancode = (SDL_Scancode)(SDL_SCANCODE_A + key - 'A');
	else if (key >= '1' && key <= '9')
		scancode = (SDL_Scancode)(SDL_SCANCODE_1 + key - '1');
	else if (key == '0') scancode = SDL_SCANCODE_0;
	else if (key >= 0x70 && key <= 0x7B)
		scancode = (SDL_Scancode)(SDL_SCANCODE_F1 + key - 0x70);
	else
	{
		switch (key)
		{
		case 0x08: scancode = SDL_SCANCODE_BACKSPACE; break;
		case 0x09: scancode = SDL_SCANCODE_TAB; break;
		case 0x0D: scancode = SDL_SCANCODE_RETURN; break;
		case 0x13: scancode = SDL_SCANCODE_PAUSE; break;
		case 0x14: scancode = SDL_SCANCODE_CAPSLOCK; break;
		case 0x1B: scancode = SDL_SCANCODE_ESCAPE; break;
		case 0x20: scancode = SDL_SCANCODE_SPACE; break;
		case 0x21: scancode = SDL_SCANCODE_PAGEUP; break;
		case 0x22: scancode = SDL_SCANCODE_PAGEDOWN; break;
		case 0x23: scancode = SDL_SCANCODE_END; break;
		case 0x24: scancode = SDL_SCANCODE_HOME; break;
		case 0x25: scancode = SDL_SCANCODE_LEFT; break;
		case 0x26: scancode = SDL_SCANCODE_UP; break;
		case 0x27: scancode = SDL_SCANCODE_RIGHT; break;
		case 0x28: scancode = SDL_SCANCODE_DOWN; break;
		case 0x2D: scancode = SDL_SCANCODE_INSERT; break;
		case 0x2E: scancode = SDL_SCANCODE_DELETE; break;
		default: return false;
		}
	}

	int keyCount = 0;
	const bool* state = SDL_GetKeyboardState(&keyCount);
	return state && scancode > SDL_SCANCODE_UNKNOWN && (int)scancode < keyCount && state[scancode];
}

BOOLEAN IsKeyPressed(int value)
{
	if (!value)
		return FALSE;

	const UINT8* keys = (const UINT8*)&value;
	for (size_t i = 0; i < sizeof(value) && keys[i]; ++i)
	{
		if (!IsPortableKeyPressed(keys[i]))
			return FALSE;
	}
	return TRUE;
}

static UINT8 PortableKeyCode(const char* name)
{
	if (!name || !*name) return 0;
	if (name[1] == '\0')
	{
		const unsigned char c = (unsigned char)name[0];
		if (isalnum(c)) return (UINT8)toupper(c);
	}
	if (_stricmp(name, "SHIFT") == 0) return 0x10;
	if (_stricmp(name, "CTRL") == 0 || _stricmp(name, "CONTROL") == 0) return 0x11;
	if (_stricmp(name, "ALT") == 0) return 0x12;
	if (_stricmp(name, "BACKSPACE") == 0 || _stricmp(name, "BACK") == 0) return 0x08;
	if (_stricmp(name, "TAB") == 0) return 0x09;
	if (_stricmp(name, "ENTER") == 0 || _stricmp(name, "RETURN") == 0) return 0x0D;
	if (_stricmp(name, "PAUSE") == 0) return 0x13;
	if (_stricmp(name, "CAPSLOCK") == 0 || _stricmp(name, "CAPS") == 0) return 0x14;
	if (_stricmp(name, "ESC") == 0 || _stricmp(name, "ESCAPE") == 0) return 0x1B;
	if (_stricmp(name, "SPACE") == 0) return 0x20;
	if (_stricmp(name, "PAGEUP") == 0 || _stricmp(name, "PGUP") == 0) return 0x21;
	if (_stricmp(name, "PAGEDOWN") == 0 || _stricmp(name, "PGDN") == 0) return 0x22;
	if (_stricmp(name, "END") == 0) return 0x23;
	if (_stricmp(name, "HOME") == 0) return 0x24;
	if (_stricmp(name, "LEFT") == 0) return 0x25;
	if (_stricmp(name, "UP") == 0) return 0x26;
	if (_stricmp(name, "RIGHT") == 0) return 0x27;
	if (_stricmp(name, "DOWN") == 0) return 0x28;
	if (_stricmp(name, "INSERT") == 0 || _stricmp(name, "INS") == 0) return 0x2D;
	if (_stricmp(name, "DELETE") == 0 || _stricmp(name, "DEL") == 0) return 0x2E;
	if ((name[0] == 'F' || name[0] == 'f') && name[1])
	{
		char* end = NULL;
		const long functionKey = strtol(name + 1, &end, 10);
		if (end && *end == '\0' && functionKey >= 1 && functionKey <= 12)
			return (UINT8)(0x70 + functionKey - 1);
	}
	return 0;
}

int ParseKeyString(const STR value)
{
	if (!value) return 0;
	STRING512 buffer;
	snprintf(buffer, sizeof(buffer), "%s", value);
	int result = 0;
	int index = 0;
	UINT8* keys = (UINT8*)&result;
	for (char* key = strtok(buffer, "|+"); key && index < (int)sizeof(result); key = strtok(NULL, "|+"))
	{
		while (*key && isspace((unsigned char)*key)) ++key;
		char* end = key + strlen(key);
		while (end > key && isspace((unsigned char)end[-1])) *--end = '\0';
		const UINT8 code = PortableKeyCode(key);
		if (code) keys[index++] = code;
	}
	return result;
}

#endif // _WIN32
