#ifndef ENGINE_CORE_LOG_SINK_H
#define ENGINE_CORE_LOG_SINK_H

#include <string>
#include <utility>
#include <vector>

enum class LogSeverity
{
	Trace,
	Info,
	Warning,
	Error
};

struct LogRecord
{
	LogSeverity severity;
	std::string category;
	std::string message;
};

class LogSink
{
public:
	virtual ~LogSink() = default;
	virtual void write(LogRecord record) = 0;
};

class NullLogSink final : public LogSink
{
public:
	static NullLogSink& instance()
	{
		static NullLogSink sink;
		return sink;
	}
	void write(LogRecord) override {}

private:
	NullLogSink() = default;
};

class MemoryLogSink final : public LogSink
{
public:
	void write(LogRecord record) override { records_.push_back(std::move(record)); }
	const std::vector<LogRecord>& records() const { return records_; }
	void clear() { records_.clear(); }

private:
	std::vector<LogRecord> records_;
};

#endif
