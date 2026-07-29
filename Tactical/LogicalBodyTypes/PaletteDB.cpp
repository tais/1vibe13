#include "PaletteDB.h"

#include <cstring>  // libstdc++ doesn't transitively expose strcmp/memset/memcpy the way MSVC's STL does

namespace LogicalBodyTypes {

PaletteDB::PaletteDB(void) : AbstractXMLLoader(StartElementHandle, EndElementHandle, CharacterDataHandle) {
}

PaletteDB::~PaletteDB(void) {
	PaletteTableMap::iterator i = paletteTables.begin();
	for (; i != paletteTables.end(); i++) delete i->second;
}

bool PaletteDB::AddPaletteTable(std::string name, PaletteTable* paletteTable) {
	PaletteTableMap::iterator i = paletteTables.find(name);
	if (i != paletteTables.end()) return false;
	paletteTables[name] = paletteTable;
	return true;
}

PaletteTable* PaletteDB::FindPaletteTable(std::string name) {
	PaletteTableMap::iterator i = paletteTables.find(name);
	if (i == paletteTables.end()) return NULL;
	return i->second;
}

void XMLCALL PaletteDB::StartElementHandle(void* userData, const XML_Char* name, const XML_Char** atts) {
	ParseData* data = (ParseData*)userData;
	switch (data->state) {
		case E_NONE:
			if (strcmp(name, "Palettes") == 0) {
				data->state = E_ELEMENT_PALETTES;
				break;
			}
		case E_ELEMENT_PALETTES:
			if (strcmp(name, "Palette") == 0) {
				data->state = E_ELEMENT_PALETTE;
				XML_Char const* aName = GetAttribute("name", atts);
				XML_Char const* aFileName = GetAttribute("filename", atts);
				if (aFileName == NULL || aName == NULL) throw XMLParseException("Mandatory attribute missing!", name, data->pParser);
				PaletteTable* paletteTable = new PaletteTable();
				if (!paletteTable->Load(aFileName)) {
					delete paletteTable;
					throw XMLParseException("Palette table could not be loaded from the specified file!", name, data->pParser);
				}
				if (!Instance().AddPaletteTable(aName, paletteTable)) {
					delete paletteTable;  // duplicate name: not stored, so we own it
					throw XMLParseException("Palette table defined twice!", name, data->pParser);
				}
				break;
			}
		default:
			throw XMLParseException("Unexpected element!", name, data->pParser);
	}
}

void XMLCALL PaletteDB::EndElementHandle(void* userData, const XML_Char* name) {
	ParseData* data = (ParseData*)userData;
	switch (data->state) {
		case E_ELEMENT_PALETTE:
			if (strcmp(name, "Palette") == 0) {
				data->state = E_ELEMENT_PALETTES;
				break;
			}
		case E_ELEMENT_PALETTES:
			if (strcmp(name, "Palettes") == 0) {
				data->state = E_NONE;
				break;
			}
		default:
			throw XMLParseException("Unexpected element! Closing tag missing?", name, data->pParser);
			return;
	}
}

void XMLCALL PaletteDB::CharacterDataHandle(void* userData, const XML_Char* str, int len) {
}
}
