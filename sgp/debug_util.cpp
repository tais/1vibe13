// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug_util.h"

const void *const *StackTrace::Addresses(size_t* count) {
  *count = trace_.size();
  if (trace_.size())
    return &trace_[0];
  return NULL;
}

void sgp::dumpStackTrace(vfs::String const& msg)
{
	// the first error is the important one anyway
	static bool already_dumping = false;

	if(!already_dumping) // needs a mutes to be sure
	{
		already_dumping = true;
		StackTrace str;
		str.PrintBacktrace(msg.utf8().c_str());
		already_dumping = false;
	}
}

#ifdef __MINGW32__
StackTrace::StackTrace()
{
}

void StackTrace::PrintBacktrace(const char* msg)
{
}

#elif !defined(_MSC_VER)
// Portable backtrace via execinfo.h. macOS, glibc, and the BSDs ship
// backtrace() / backtrace_symbols(); musl notably does not, so a
// non-glibc Linux build would need a different path.
#include <execinfo.h>
#include <cstdio>
#include <cstdlib>

StackTrace::StackTrace()
{
	constexpr int kMaxFrames = 64;
	void* frames[kMaxFrames];
	int count = backtrace(frames, kMaxFrames);
	if (count > 0) {
		trace_.assign(frames, frames + count);
	}
}

void StackTrace::PrintBacktrace(const char* msg)
{
	if (msg && *msg) std::fprintf(stderr, "Backtrace: %s\n", msg);
	if (trace_.empty()) {
		std::fprintf(stderr, "  (no frames captured)\n");
		return;
	}
	char** syms = backtrace_symbols(trace_.data(), (int)trace_.size());
	for (size_t i = 0; i < trace_.size(); ++i) {
		std::fprintf(stderr, "  %s\n", syms ? syms[i] : "(symbol resolution failed)");
	}
	std::free(syms);
}

void StackTrace::OutputToStream(const char* msg, sgp::Logger::LogInstance* /*os*/)
{
	// The sgp::Logger overload path was wired up to the MSVC
	// DbgHelp / SymFromAddr machinery; the portable execinfo path
	// just dumps to stderr so we don't need to thread the symbol
	// strings through the logger stream operators here.
	PrintBacktrace(msg);
}
#endif
