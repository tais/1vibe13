#pragma once

#include <string_view>

namespace i18n
{
// Receives borrowed text from the process-selected compiled catalog. The
// implementation must copy section and text before copyEntry returns; the
// adapter never transfers or extends the lifetime of legacy global storage.
class SelectedCatalogExportSink
{
public:
	virtual ~SelectedCatalogExportSink() = default;

	virtual void copyEntry(std::wstring_view section, int index,
		std::wstring_view text) = 0;
};

// Enumerates the selected linked catalog in the historical GameStrings order.
// Language selection remains compile-time and g_lang remains immutable.
void ExportSelectedCatalog(SelectedCatalogExportSink& sink);
}
