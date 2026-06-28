#include "XML.h"
#include "FileMan.h"
#include "MemMan.h"
#include "Debug Control.h"
#include <stdio.h>

const XML_Char* GetAttribute(const XML_Char* name, const XML_Char** atts) {
	for (const XML_Char** pIter = atts; *pIter != NULL; pIter++) {
		const XML_Char* key = *pIter++;
		if (strcmp(key, name) == 0) return *pIter;
	}
	return NULL;
}

// Shared open/read/parse/free for the hand-rolled expat loaders. The ~100
// ReadIn* loaders each re-implement this identical sequence with hand-duplicated
// cleanup on every early-return; the divergences are the documented XML
// leak-on-error bug class (parser leaked on FileOpen-fail, parser+handle leaked
// on FileRead-fail). This helper owns the file handle, buffer and parser for the
// whole scope via RAII, so EVERY exit path is leak-free by construction. A loader
// converted to it shrinks to its handler table + this one call; behaviour and the
// on-disk format are unchanged.
//
// pErrorContext is used only for the parse-error log line (e.g. "Bloodcats.xml").
// Returns false on open/alloc/read/parse failure, true on a clean parse.
bool ParseXMLFile(STR fileName,
                  XML_StartElementHandler startHandler,
                  XML_EndElementHandler endHandler,
                  XML_CharacterDataHandler charHandler,
                  void* userData,
                  const char* pErrorContext)
{
	// RAII guards so any early return frees everything.
	struct FileGuard {
		HWFILE h;
		FileGuard(HWFILE hh) : h(hh) {}
		~FileGuard() { if (h) FileClose(h); }
	};
	struct ParserGuard {
		XML_Parser p;
		ParserGuard(XML_Parser pp) : p(pp) {}
		~ParserGuard() { if (p) XML_ParserFree(p); }
	};
	struct BufGuard {
		CHAR8* b;
		BufGuard() : b(NULL) {}
		~BufGuard() { if (b) MemFree(b); }
	};

	HWFILE hFile = FileOpen(fileName, FILE_ACCESS_READ, FALSE);
	if (!hFile) return false;
	FileGuard fg(hFile);

	UINT32 uiFSize = FileGetSize(hFile);
	BufGuard buf;
	buf.b = (CHAR8*)MemAlloc(uiFSize + 1);
	if (!buf.b) return false;

	UINT32 uiBytesRead = 0;
	if (!FileRead(hFile, buf.b, uiFSize, &uiBytesRead)) return false;
	buf.b[uiFSize] = 0;

	XML_Parser parser = XML_ParserCreate(NULL);
	if (!parser) return false;
	ParserGuard pg(parser);

	XML_SetElementHandler(parser, startHandler, endHandler);
	if (charHandler) XML_SetCharacterDataHandler(parser, charHandler);
	XML_SetUserData(parser, userData);

	if (!XML_Parse(parser, buf.b, uiFSize, TRUE))
	{
		CHAR8 errorBuf[511];
		sprintf(errorBuf, "XML Parser Error in %s: %s at line %d",
			pErrorContext ? pErrorContext : (fileName ? fileName : "?"),
			XML_ErrorString(XML_GetErrorCode(parser)),
			(int)XML_GetCurrentLineNumber(parser));
		LiveMessage(errorBuf);
		return false;
	}

	return true;
}
