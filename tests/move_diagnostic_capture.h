#ifndef TESTS_MOVE_DIAGNOSTIC_CAPTURE_H
#define TESTS_MOVE_DIAGNOSTIC_CAPTURE_H
#include <cstdio>
#include <cstdlib>
#include <string>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

class MoveDiagnosticCapture
{
public:
	explicit MoveDiagnosticCapture(bool enabled)
	{
		if (const char* value = std::getenv("JA2_COOP_MOVE_DIAGNOSTIC")) { hadEnvironment_ = true; environment_ = value; }
		setEnvironment(enabled ? "1" : nullptr);
		file_ = std::tmpfile();
		if (!file_) return;
		std::fflush(stderr);
#ifdef _WIN32
		saved_ = _dup(_fileno(stderr));
		if (saved_ >= 0) _dup2(_fileno(file_), _fileno(stderr));
#else
		saved_ = dup(fileno(stderr));
		if (saved_ >= 0) dup2(fileno(file_), fileno(stderr));
#endif
	}
	~MoveDiagnosticCapture()
	{
		restore();
		if (file_) std::fclose(file_);
		setEnvironment(hadEnvironment_ ? environment_.c_str() : nullptr);
	}
	bool valid() const { return file_ != nullptr && saved_ >= 0; }
	std::string finish()
	{
		std::fflush(stderr);
		restore();
		std::string result;
		if (!file_) return result;
		std::rewind(file_);
		char bytes[1024];
		for (std::size_t count; (count = std::fread(bytes, 1, sizeof(bytes), file_)) != 0;) result.append(bytes, count);
		return result;
	}
private:
	static void setEnvironment(const char* value)
	{
#ifdef _WIN32
		_putenv_s("JA2_COOP_MOVE_DIAGNOSTIC", value ? value : "");
#else
		if (value) setenv("JA2_COOP_MOVE_DIAGNOSTIC", value, 1);
		else unsetenv("JA2_COOP_MOVE_DIAGNOSTIC");
#endif
	}
	void restore()
	{
		if (saved_ < 0) return;
		std::fflush(stderr);
#ifdef _WIN32
		_dup2(saved_, _fileno(stderr)); _close(saved_);
#else
		dup2(saved_, fileno(stderr)); close(saved_);
#endif
		saved_ = -1;
	}
	std::FILE* file_ = nullptr;
	int saved_ = -1;
	bool hadEnvironment_ = false;
	std::string environment_;
};
#endif
