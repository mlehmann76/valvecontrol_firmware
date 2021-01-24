/*
 * tasks.h
 *
 *  Created on: 28.09.2020
 *      Author: marco
 */

#ifndef TASKS_H_
#define TASKS_H_

#include "property.h"
#include <atomic>
#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

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
            bool operator==(const TasksItem &rhs) const {
                return m_channel == rhs.m_channel;
            }
            bool operator!=(const TasksItem &rhs) const {
                return m_channel != rhs.m_channel;
            }
        };
        using itemListType = std::list<std::shared_ptr<TasksItem>>;
        itemListType m_taskitems;
        Task() : m_enabled(false), m_autoMode(false) {}
    };
    using taskListType = std::map<std::string, std::shared_ptr<Task>>;

    void readTasks();

    Task &get(const std::string &name) const { return *m_tasks.at(name); }

    const taskListType &map() const { return m_tasks; }

    repository &repo() const { return m_repo; }

  private:
    bool readTask(const std::string &name);
    void readTaskDetail(const std::string &name, Task &_t) const;
    Task::TasksItem readTaskItemDetail(const std::string &_next) const;

    taskListType m_tasks;
    repository &m_repo;
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
    TaskState(repository &state, std::string &&taskId);
    void regStateVariables();
    void update(bool run, const std::chrono::system_clock::time_point &_start);
    void update(const detail::TaskConfig::Task &task,
                const TaskConfig::Task::TasksItem &item);
    void update(std::chrono::seconds time, std::chrono::seconds remain);
    bool running() const;
    repository &repo() const { return m_stateRep; }

  private:
    repository &m_stateRep;
    const std::string m_taskId;
};

} // namespace detail
/**
 *
 */
class Tasks {
    static detail::TaskConfig::Task NoneTask;
    static detail::TaskConfig::Task::TasksItem NoneTaskItem;
    using listIterator = detail::TaskConfig::Task::itemListType::iterator;

  public:
    Tasks(repository &config);
    ~Tasks();
    void setup();
    void onControl(const property &p);
    void startTask();
    void stopTask();

  private:
    void task();
    void start(listIterator _findIter);
    void stop();
    void checkTimeString();
    bool checkWeekDay(const std::string &d);
    bool checkTimePoint(const std::string &d);
    std::chrono::system_clock::time_point toTimePoint(const std::string &d);
    void setChannel(const std::string &c, bool isOn);
    std::string activeTaskName();
    void regTaskControl(const std::string &name);

    repository &m_repo;
    std::shared_ptr<detail::TaskConfig> m_config;
    std::map<std::string, std::shared_ptr<detail::TaskState>> m_states;
    std::string m_nextRequest;
    detail::TaskConfig::Task *m_activeTask;
    detail::TaskConfig::Task::TasksItem *m_activeItem;
    std::chrono::seconds m_remain;
    std::thread m_thread;
    std::atomic<bool> m_aexit;
    std::chrono::steady_clock::time_point m_start;
    std::mutex m_lock;
};

#endif /* TASKS_H_ */
