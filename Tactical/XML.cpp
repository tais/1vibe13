#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "XML.h"
#include "Debug Control.h"

const XML_Char* GetAttribute(const XML_Char* name, const XML_Char** atts) {
	for (const XML_Char** pIter = atts; *pIter != NULL; pIter++) {
		const XML_Char* key = *pIter++;
		if (strcmp(key, name) == 0) return *pIter;
	}
	return NULL;
}

// Compatibility wrapper for loaders already converted to the historical
// ParseXMLFile helper. The bounded platform adapter now owns asset reads, parser
// lifetime, allocation failures, and parse diagnostics for these callers too.
//
// pErrorContext is used only for the parse-error log line (e.g. "Bloodcats.xml").
// Returns false on read/parse failure, true on a clean parse.
bool ParseXMLFile(STR fileName,
                  XML_StartElementHandler startHandler,
                  XML_EndElementHandler endHandler,
                  XML_CharacterDataHandler charHandler,
                  void* userData,
                  const char* pErrorContext)
{
	const LegacyXmlCallbacks callbacks{
		userData, startHandler, endHandler, charHandler};
	const LegacyXmlResult result = ParseLegacyXmlFile(fileName, callbacks);
	if (!result)
	{
		if (result.status != LegacyXmlStatus::NotFound &&
			result.status != LegacyXmlStatus::ReadError)
		{
			const char* const context =
				pErrorContext && pErrorContext[0] ? pErrorContext : fileName;
			const auto message = FormatLegacyXmlFailure(context, result);
			LiveMessage(message.data());
		}
		return false;
	}

	return true;
}
