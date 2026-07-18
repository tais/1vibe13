/* 
 * bfVFS : vfs/Tools/vfs_hp_timer.cpp
 *  - high performance/precision timer, used by profiler
 *
 * Copyright (C) 2008 - 2010 (BF) john.bf.smith@googlemail.com
 * 
 * This file is part of the bfVFS library
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <vfs/Tools/vfs_hp_timer.h>

vfs::HPTimer::HPTimer() : is_running(false), start(), stop()
{}

vfs::HPTimer::~HPTimer()
{
}

void vfs::HPTimer::startTimer()
{
	start = Clock::now();
	stop = start;
	is_running = true;
}

long long vfs::HPTimer::ticks()
{
	const Clock::time_point end = is_running ? Clock::now() : stop;
	return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

double vfs::HPTimer::running()
{
	if (!is_running)
		return 0.0;
	return std::chrono::duration<double>(Clock::now() - start).count();
}

void vfs::HPTimer::stopTimer()
{
	if (is_running)
		stop = Clock::now();
	is_running = false;
}

double vfs::HPTimer::getElapsedTimeInSeconds()
{
	const Clock::time_point end = is_running ? Clock::now() : stop;
	return std::chrono::duration<double>(end - start).count();
}
