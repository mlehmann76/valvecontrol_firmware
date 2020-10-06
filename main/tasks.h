/*
 * tasks.h
 *
 *  Created on: 28.09.2020
 *      Author: marco
 */

#ifndef TASKS_H_
#define TASKS_H_

#include <chrono>
#include <string>
#include <list>
#include <map>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include "property.h"

class repository;
class Tasks;

namespace detail {
/**
 *
 */
class TaskConfig {
public:

	TaskConfig(repository &r);

	struct Task {
		std::string m_name;
		std::string m_alt;
		std::string m_autoTime;
		bool m_enabled;
		bool m_autoMode;
		struct TasksItem {
			std::string m_channel;
			std::chrono::seconds m_time;
			TasksItem() : m_channel(), m_time() {}
			bool operator==(const TasksItem& rhs) const {
				return m_channel == rhs.m_channel;
			}
			bool operator!=(const TasksItem& rhs) const {
				return m_channel != rhs.m_channel;
			}
		};
		using listType = std::list<std::shared_ptr<TasksItem>>;
		listType m_taskitems;
		Task() : m_enabled(false), m_autoMode(false) {}
	};

	std::map<std::string,std::shared_ptr<Task>> m_tasks;

	void readTasks();
	bool readTask(const std::string &name);

	Task& get(const std::string &name) const {
		return *m_tasks.at(name);
	}

	void readTaskDetail(const std::string &name, Task &_t);
	Task::TasksItem readTaskItemDetail(const std::string &_next);
	repository& repo() const { return m_repo;}

private:
	repository& m_repo;
};
/**
 *
 */
class TaskThread {
public:
};

/**
 *
 */
class TaskState {
public:
	TaskState(repository& state);
	void regStateVariables();
	void update(bool run, const std::chrono::system_clock::time_point& _start);
	void update(const TaskConfig::Task& task, const TaskConfig::Task::TasksItem& item);
	void update(std::chrono::seconds time, std::chrono::seconds remain);

	std::string name() { return m_state["name"]().get<StringType>();}
	bool running() { return m_state["running"]().get<BoolType>();}
	repository& repo() const { return m_stateRep;}

private:

	repository& m_stateRep;
	property m_state;
};

class TaskControl {
public:
	TaskControl(Tasks& task, repository& ctrl);
	repository& repo() const { return m_repo;}
private:
	repository& m_repo;
};
} //namespace detail
/**
 *
 */
class Tasks {
	static detail::TaskConfig::Task NoneTask;
	static detail::TaskConfig::Task::TasksItem NoneTaskItem;
	using listIterator = detail::TaskConfig::Task::listType::iterator;
public:
	Tasks(repository& config, repository& state, repository& ctrl);
	~Tasks();
	void setup();
	void onControl(const property& p);
	void startTask();
	void stopTask();

private:
	void task();
	void _start(listIterator _findIter);
	void _stop();
	void _checkTimeString();

	std::shared_ptr<detail::TaskConfig> m_config;
	std::shared_ptr<detail::TaskState> m_state;
	std::shared_ptr<detail::TaskControl> m_control;
	std::string m_nextRequest;
	detail::TaskConfig::Task* m_activeTask;
	detail::TaskConfig::Task::TasksItem* m_activeItem;
	std::chrono::seconds m_remain;
	std::thread m_thread;
	std::atomic<bool> m_aexit;
	std::chrono::system_clock::time_point m_start;
	std::mutex m_lock;

};

#endif /* TASKS_H_ */
