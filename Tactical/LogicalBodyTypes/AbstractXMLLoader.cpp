#include "AbstractXMLLoader.h"

#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>
#include <Engine/Adapters/Legacy/PlatformAssets.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string>

// The in-tree sgp/expat.h is from expat 1.95.7 (vendored with the
// original JA2 source), but the build links against modern expat
// (2.6+, fetched via FetchContent). The billion-laughs-DoS guard added
// in 2.4.0 needs to be relaxed for JA2's data files. Forward-declare
// it here rather than swapping every expat include over to the modern
// header all at once.
extern "C" XMLPARSEAPI(XML_Bool)
XML_SetBillionLaughsAttackProtectionMaximumAmplification(
	XML_Parser parser, float maximumAmplification);

namespace LogicalBodyTypes {

struct AbstractXMLLoader::ParserContext {
	AbstractXMLLoader* loader;
	const AssetSource* assets;
	std::string directory;
	std::unique_ptr<ParseData> data;
	XML_Parser activeParser = nullptr;
};

AbstractXMLLoader::AbstractXMLLoader(XML_StartElementHandler startHandler, XML_EndElementHandler endHandler, XML_CharacterDataHandler charHandler, ParseDataFactoryFunc parseDataFactF) {
	startElementHandler = startHandler;
	endElementHandler = endHandler;
	characterDataHandler = charHandler;
	parseDataFactFuncPntr = parseDataFactF;
}

AbstractXMLLoader::~AbstractXMLLoader(void) {
}

AbstractXMLLoader::ParseData::ParseData(XML_Parser* parser) {
	pParser = parser;
	state = E_NONE;
	level = 0;
	szCharData[0] = '\0';
	szErrorTxt[0] = '\0';
}

AbstractXMLLoader::ParseData* AbstractXMLLoader::MakeParseData(XML_Parser* parser) {
	return new ParseData(parser);
}

bool AbstractXMLLoader::LoadFromFile(const char* directoryName, const char* fileName,
	CHAR8* errorBuf, size_t errorCapacity) {
	if (!directoryName || !fileName || !errorBuf || errorCapacity == 0) {
		static constexpr const char invalidInput[] =
			"Can't load LogicalBodyTypes XML: invalid path or error buffer";
		if (errorBuf && errorCapacity)
			std::snprintf(errorBuf, errorCapacity, "%s", invalidInput);
		LiveMessage(invalidInput);
		return false;
	}
	try {
		SetDirectoryName(directoryName);
		SetFileName(fileName);

		std::string logicalPath(directoryName);
		logicalPath += fileName;

		std::string msg = "Loading ";
		msg += fileName;
		DebugMsg(TOPIC_JA2, DBG_LEVEL_3, msg.c_str());

		ParserContext context{
			this, &GetPlatformAssetSource(), directoryName, nullptr, nullptr};
		LegacyXmlCallbacks callbacks;
		callbacks.userData = &context;
		callbacks.parserReady = PrepareParser;
		const LegacyXmlResult result = ParseLegacyXmlAsset(
			*context.assets, logicalPath, callbacks);
		if (result) return true;

		const auto failure =
			FormatLegacyXmlFailure(logicalPath.c_str(), result);
		std::snprintf(errorBuf, errorCapacity, "%s", failure.data());
		if (result.status == LegacyXmlStatus::Malformed ||
			result.status == LegacyXmlStatus::CallbackError)
			LiveMessage(errorBuf);
		return false;
	}
	catch (const std::bad_alloc&) {
		std::snprintf(errorBuf, errorCapacity,
			"Not enough memory to load LogicalBodyTypes XML: %s%s",
			directoryName, fileName);
		return false;
	}
	catch (...) {
		std::snprintf(errorBuf, errorCapacity,
			"Unexpected failure loading LogicalBodyTypes XML: %s%s",
			directoryName, fileName);
		return false;
	}
}

void AbstractXMLLoader::PrepareParser(XML_Parser parser, void* userData) {
	ParserContext* context = static_cast<ParserContext*>(userData);
	if (!context || !context->loader) return;

	context->activeParser = parser;
	// JA2's shipped AnimationSurfaces.xml legitimately expands a number of
	// large external entities. Preserve the established compatibility setting
	// while keeping parser creation and destruction inside the engine adapter.
	XML_SetBillionLaughsAttackProtectionMaximumAmplification(
		parser, 1000000.0f);
	context->data.reset(
		context->loader->parseDataFactFuncPntr(&context->activeParser));
	XML_SetElementHandler(parser,
		context->loader->startElementHandler,
		context->loader->endElementHandler);
	XML_SetCharacterDataHandler(
		parser, context->loader->characterDataHandler);
	XML_SetUserData(parser, context->data.get());
	XML_SetExternalEntityRefHandler(parser, ExternalEntityHandler);
	XML_SetExternalEntityRefHandlerArg(parser, context);
}

void AbstractXMLLoader::PrepareExternalParser(
	XML_Parser parser, void* userData) {
	ParserContext* context = static_cast<ParserContext*>(userData);
	if (!context || !context->loader || !context->data) return;

	context->activeParser = parser;
	XML_SetElementHandler(parser,
		context->loader->startElementHandler,
		context->loader->endElementHandler);
	XML_SetCharacterDataHandler(
		parser, context->loader->characterDataHandler);
	XML_SetUserData(parser, context->data.get());
	XML_SetExternalEntityRefHandler(parser, ExternalEntityHandler);
	XML_SetExternalEntityRefHandlerArg(parser, context);
}

int XMLCALL AbstractXMLLoader::ExternalEntityHandler(XML_Parser args,
	const XML_Char* context, const XML_Char*, const XML_Char* systemId,
	const XML_Char*) {
	ParserContext* parserContext = reinterpret_cast<ParserContext*>(args);
	if (!parserContext || !parserContext->assets ||
		!parserContext->activeParser || !systemId || !systemId[0]) {
		LiveMessage("Invalid LogicalBodyTypes external entity reference");
		return XML_STATUS_ERROR;
	}

	const XML_Parser parentParser = parserContext->activeParser;
	try {
		std::string logicalPath = parserContext->directory;
		logicalPath += systemId;

		LegacyXmlCallbacks callbacks;
		callbacks.userData = parserContext;
		callbacks.parserReady = PrepareExternalParser;
		const LegacyXmlResult result = ParseLegacyXmlExternalEntityAsset(
			*parserContext->assets, logicalPath, parentParser, context,
			callbacks);
		parserContext->activeParser = parentParser;
		if (result) return XML_STATUS_OK;

		const auto failure =
			FormatLegacyXmlFailure(logicalPath.c_str(), result);
		LiveMessage(failure.data());
		return XML_STATUS_ERROR;
	}
	catch (...) {
		parserContext->activeParser = parentParser;
		LiveMessage(
			"Unexpected failure loading LogicalBodyTypes external entity");
		return XML_STATUS_ERROR;
	}
};


const char* AbstractXMLLoader::GetFileName() {
	return fileName.c_str();
}

const char* AbstractXMLLoader::GetDirectoryName() {
	return directoryName.c_str();
}

void AbstractXMLLoader::SetFileName(const char* fileName) {
	this->fileName = fileName ? fileName : "";
}

void AbstractXMLLoader::SetDirectoryName(const char* directoryName) {
	this->directoryName = directoryName ? directoryName : "";
}

const XML_Char* AbstractXMLLoader::GetAttribute(const XML_Char* name, const XML_Char** atts) {
	for (const XML_Char** pIter = atts; *pIter != NULL; pIter++) {
		const XML_Char* key = *pIter++;
		if (strcmp(key, name) == 0) return *pIter;
	}
	return NULL;
}

bool AbstractXMLLoader::ConvertStringToINT8(const XML_Char* num, INT8* int8) {
	long l = strtol(num, NULL, 10);
	if (l > 127 || l < -128) return false;
	if (l == 0 && strcmp(num, "0") != 0) return false;
	*int8 = (INT8)l;
	return true;
}

bool AbstractXMLLoader::ConvertStringToUINT8(const XML_Char* num, UINT8* uInt8) {
	unsigned long ul = strtoul(num, NULL, 10);
	if (ul > 255) return false;
	if (ul == 0 && strcmp(num, "0") != 0) return false;
	*uInt8 = (UINT8)ul;
	return true;
}

bool AbstractXMLLoader::ConvertStringToINT32(const XML_Char* num, INT32* int32) {
	long l = strtol(num, NULL, 10);
	// TODO: add range checks also for long types (and use the defined macros)
	// TODO: string should be trimmed (other convert functions also)
	if (l == 0 && strcmp(num, "0") != 0) return false;
	*int32 = (INT32)l;
	return true;
}

bool AbstractXMLLoader::ConvertStringToUINT32(const XML_Char* num, UINT32* uInt32) {
	unsigned long ul = strtoul(num, NULL, 10);
	if (ul == 0 && strcmp(num, "0") != 0) return false;
	*uInt32 = ul;
	return true;
}

bool AbstractXMLLoader::ConvertStringToBOOLEAN(const XML_Char* num, BOOLEAN* fBoolean) {
	unsigned long ul = strtoul(num, NULL, 10);
	if (ul == 0 && strcmp(num, "0") != 0) return false;
	*fBoolean = ul != 0;
	return true;
}

}
