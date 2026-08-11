#include "LegacyUtilitiesModel.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
	void Require(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

int main()
{
	using namespace LegacyUtilitiesModel;

	WideTextSplit split{L"unchanged", L"unchanged"};
	Require(SplitWideText(L"alpha beta gamma", 10, split) &&
		split.first == L"alpha beta" && split.second == L"gamma",
		"wide text wraps at the last in-range space");
	Require(SplitWideText(L"unbroken", 4, split) &&
		split.first == L"unbr" && split.second == L"-oken",
		"wide text preserves the legacy hyphen fallback without underflow");
	const WideTextSplit retained = split;
	Require(!SplitWideText(L"short", 5, split) &&
		split.first == retained.first && split.second == retained.second,
		"an absent overflow leaves the prior split untouched");

	std::string path = "unchanged";
	Require(JoinPath("Data/", "INTRO.SLF", 100, path) &&
		path == "Data/INTRO.SLF",
		"legacy media paths join within the filename boundary");
	const std::string retainedPath = path;
	Require(!JoinPath(std::string(99, 'x'), "y", 100, path) &&
		path == retainedPath &&
		JoinPath("", std::string(99, 'x'), 100, path) && path.size() == 99,
		"path joins reject overflow transactionally and accept the exact limit");

	std::cout << "Utils legacy utilities model tests passed\n";
	return 0;
}
