#include "DataBoundaryModel.h"

#include <cmath>
#include <clocale>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
	void Require(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}

	struct PublishedState
	{
		int value = 0;
	};
}

int main()
{
	using namespace UtilsDataBoundaryModel;

	std::int64_t signedValue = 0;
	std::uint64_t unsignedValue = 0;
	double realValue = 0.0;
	Require(ParseInt64(" +2147483647 ", signedValue) &&
			signedValue == 2147483647 &&
		!ParseInt64("12tail", signedValue) &&
		!ParseInt64("9223372036854775808", signedValue) &&
		ParseUInt64("+4294967295", unsignedValue) &&
			unsignedValue == 4294967295ULL &&
		!ParseUInt64("-1", unsignedValue) &&
		ParseDouble(" -1.25e2 ", realValue) && realValue == -125.0 &&
		!ParseDouble("1.5tail", realValue) &&
		!ParseDouble("nan", realValue) &&
		!ParseDouble("1e9999", realValue),
		"numeric conversion rejects trailing text, overflow, signs outside the domain, and non-finite values");

	const char* const currentNumericLocale = std::setlocale(LC_NUMERIC, nullptr);
	const std::string savedNumericLocale = currentNumericLocale
		? currentNumericLocale : "C";
	for (const char* candidate : {
		"de_DE.UTF-8", "nl_NL.UTF-8", "fr_FR.UTF-8",
		"German_Germany.1252", "Dutch_Netherlands.1252",
		"French_France.1252"})
	{
		if (std::setlocale(LC_NUMERIC, candidate)) break;
	}
	double localeIndependentValue = 0.0;
	std::vector<float> localeIndependentList;
	const bool localeIndependent =
		ParseDouble("1.25", localeIndependentValue) &&
		localeIndependentValue == 1.25 &&
		ParseFloatList("1.25, 2.5", localeIndependentList) &&
		localeIndependentList == std::vector<float>({1.25f, 2.5f});
	std::setlocale(LC_NUMERIC, savedNumericLocale.c_str());
	Require(localeIndependent,
		"floating conversion remains locale-independent after LC_NUMERIC changes");

	std::vector<std::int32_t> integers{77};
	Require(ParseInt32List("1, -2, +3", integers) &&
		integers == std::vector<std::int32_t>({1, -2, 3}),
		"integer arrays publish a completely parsed list");
	Require(!ParseInt32List("4,5x,6", integers) &&
		integers == std::vector<std::int32_t>({1, -2, 3}) &&
		!ParseInt32List("1,2147483648", integers) &&
		integers == std::vector<std::int32_t>({1, -2, 3}),
		"malformed and overflowing integer arrays leave prior state untouched");

	std::vector<float> reals{77.0f};
	Require(ParseFloatList("0.5, -2, 3e1", reals) && reals.size() == 3 &&
		reals[0] == 0.5f && reals[1] == -2.0f && reals[2] == 30.0f,
		"float arrays publish a completely parsed finite list");
	Require(!ParseFloatList("1,nan,2", reals) && reals.size() == 3 &&
		reals[0] == 0.5f &&
		!ParseFloatList("1,,2", reals) && reals.size() == 3 &&
		!ParseFloatList("1e-50", reals) && reals.size() == 3,
		"malformed float arrays leave prior state untouched");

	char exact[5]{};
	Require(CopyString(exact, "four") && std::string(exact) == "four",
		"bounded INI strings accept an exact-capacity payload");
	Require(!CopyString(exact, "overflow") && std::string(exact) == "over",
		"bounded INI strings truncate and terminate oversized payloads");
	char zeroCapacity = 'Z';
	Require(!CopyString(&zeroCapacity, 0, "value") && zeroCapacity == 'Z' &&
		!CopyString(nullptr, 4, "value"),
		"bounded INI strings reject zero-capacity and null destinations without writing");

	PublishedState live{7};
	Require(!PublishTransactionally(live, [](PublishedState& staged) {
		staged.value = 11;
		return false;
	}) && live.value == 7,
		"failed data loads leave the published state untouched");
	Require(PublishTransactionally(live, [](PublishedState& staged) {
		staged.value = 12;
		return true;
	}) && live.value == 12,
		"successful data loads publish their complete staged state");

	UnknownXmlSubtree unknown;
	Require(unknown.enter() && unknown.enter() && unknown.active() &&
		unknown.depth() == 2 && unknown.leave() && unknown.active() &&
		unknown.leave() && !unknown.active() && !unknown.leave(),
		"unknown property XML subtrees are skipped with balanced nesting depth");

	std::string escaped = "unchanged";
	Require(EscapeXml("<&>\"'", escaped) &&
		escaped == "&lt;&amp;&gt;&quot;&apos;",
		"XML text and attribute payloads escape all reserved characters");
	const std::string invalidControl(1, '\x01');
	Require(!EscapeXml(invalidControl, escaped) && escaped ==
			"&lt;&amp;&gt;&quot;&apos;",
		"invalid XML controls fail without publishing partial output");

	std::string comment;
	Require(SanitizeXmlComment("a--b-", comment) && comment == "a- -b- ",
		"XML comments cannot contain double-dash or end in a dash");
	Require(IsExactTransfer(12, 12) && !IsExactTransfer(12, 11),
		"file transfers succeed only when every requested byte is written");

	std::cout << "Utils data boundary model tests passed\n";
	return 0;
}
