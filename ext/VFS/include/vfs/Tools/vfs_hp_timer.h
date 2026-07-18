/* 
 * bfVFS : vfs/Tools/vfs_hp_timer.h
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

#ifndef _VFS_HP_TIMER_
#define _VFS_HP_TIMER_

#include <vfs/vfs_config.h>
#include <chrono>

namespace vfs
{
	class VFS_API HPTimer
	{
	public:
		HPTimer();
		~HPTimer();

		void		startTimer();
		void		stopTimer();
		
		long long	ticks();
		double		running();

		double		getElapsedTimeInSeconds();
	protected:
		using Clock = std::chrono::steady_clock;

		bool              is_running;
		Clock::time_point start;
		Clock::time_point stop;
	};
}

#endif // _VFS_HP_TIMER_
