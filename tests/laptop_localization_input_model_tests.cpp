#include "LocalizationInputModel.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <cwchar>

namespace Model = LaptopLocalizationModel;

int main()
{
	{
		char text[6]{};
		assert(Model::AppendText(text, "ab", 2));
		assert(Model::AppendText(text, "cde", 3));
		assert(std::strcmp(text, "abcde") == 0);
		assert(!Model::AppendText(text, "x", 1));
		assert(std::strcmp(text, "abcde") == 0);
	}

	{
		char text[5] = "abc";
		assert(!Model::AppendText(text, "de", 2));
		assert(std::strcmp(text, "abc") == 0);
		assert(!Model::AppendText(text, nullptr, 0));
		assert(!Model::AppendText(text, "x", -1));
	}

	{
		char unterminated[3] = {'a', 'b', 'c'};
		assert(!Model::AppendText(unterminated, "x", 1));
		assert(unterminated[0] == 'a' && unterminated[2] == 'c');
	}

	{
		std::uint8_t byte = 0;
		assert(Model::ParseInteger(" 254\n", byte) && byte == 254);
		assert(!Model::ParseInteger("255x", byte));
		assert(!Model::ParseInteger("256", byte));
		assert(!Model::ParseInteger("-1", byte));
		assert(!Model::ParseInteger("", byte));
	}

	{
		std::int8_t value = 0;
		assert(Model::ParseInteger("-128", value) && value == -128);
		assert(Model::ParseInteger("127", value) && value == 127);
		assert(!Model::ParseInteger("128", value));
		assert(!Model::ParseInteger("-129", value));
	}

	{
		std::uint8_t value = 0;
		assert(Model::ParseIntegerOrMinusOneSentinel("-1", value));
		assert(value == 255);
		assert(Model::ParseIntegerOrMinusOneSentinel("254", value));
		assert(value == 254);
		assert(!Model::ParseIntegerOrMinusOneSentinel("-2", value));
		assert(!Model::ParseIntegerOrMinusOneSentinel("256", value));
	}

	{
		unsigned int flag = 7;
		assert(Model::ParseBoolean("0", flag) && flag == 0);
		assert(Model::ParseBoolean(" 1 ", flag) && flag == 1);
		assert(!Model::ParseBoolean("2", flag));
		assert(!Model::ParseBoolean("-1", flag));
	}

	assert(Model::IsIndexInRange(255, 254));
	assert(!Model::IsIndexInRange(255, 255));
	assert(!Model::IsIndexInRange(255, -1));

	{
		wchar_t destination[5]{};
		const wchar_t source[] = L"safe";
		assert(Model::CopyText(destination, source));
		assert(std::wcscmp(destination, L"safe") == 0);

		wchar_t small[4] = L"old";
		assert(!Model::CopyText(small, source));
		assert(std::wcscmp(small, L"old") == 0);

		const wchar_t unterminated[3] = {L'n', L'o', L't'};
		assert(!Model::CopyText(destination, unterminated));
		assert(std::wcscmp(destination, L"safe") == 0);
	}

	return 0;
}
