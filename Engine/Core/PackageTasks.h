#ifndef ENGINE_CORE_PACKAGE_TASKS_H
#define ENGINE_CORE_PACKAGE_TASKS_H

#include <string>
#include <utility>

#include <Engine/Core/PackageTaskQueue.h>

class PackageTasks
{
public:
	PackageTasks(std::string packageId, PackageTaskQueue& queue)
		: packageId_(std::move(packageId)), queue_(queue) {}

	const std::string& packageId() const { return packageId_; }
	PackageTaskScheduleResult defer(PackageTaskQueue::Task task) const noexcept
	{
		return queue_.schedule(packageId_, std::move(task));
	}

private:
	std::string packageId_;
	PackageTaskQueue& queue_;
};

#endif
