/*
 * tasks.h
 *
 *  Created on: 28.09.2020
 *      Author: marco
 */

#ifndef TASKS_H_
#define TASKS_H_

#include "property.h"
#include "repository.h"
#include "utilities.h"
#include <atomic>
#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <deque>

class repository;
class Tasks;

using namespace std::string_literals;

namespace detail {
/**
 *
 */
namespace TaskConfig {
//
class TaskWrap {
    repository *m_repo;
    std::string m_name;
    std::string m_fullname;
    bool m_valid;

  public:
    TaskWrap(repository &r, int index)
        : TaskWrap(r, utilities::string_format("/tasks/task%d", index)) {}

    TaskWrap(repository &r, const std::string &_task)
        : m_repo(&r), m_name(_task), m_fullname(_task + "/config"),
          m_valid(m_repo->count(m_fullname) > 0) {}
    //
    bool valid() const { return m_valid && m_repo->count(m_fullname) > 0; }
    //
    void reset() { m_valid = false; }
    //
    std::string taskName() const { return m_name; }
    //
    std::string name() const { return m_repo->get(m_fullname, "name", ""s); }
    std::string alt() const { return m_repo->get(m_fullname, "alt-name", ""s); }
    std::string first() const { return m_repo->get(m_fullname, "first", ""s); }
    std::string autoTime() const {
        return m_repo->get(m_fullname, "starttime", ""s);
    }
    bool enabled() const { return m_repo->get(m_fullname, "enabled", false); }
    bool autoMode() const { return m_repo->get(m_fullname, "auto", false); }
};

class TasksItemWrap {
    repository *m_repo;
    std::string m_nodename;
    bool m_valid;

  public:
    TasksItemWrap(repository &r, const std::string &node)
        : m_repo(&r), m_nodename(node), m_valid(m_repo->count(node) > 0) {}
    //
    bool valid() const { return m_valid && m_repo->count(m_nodename) > 0; }
    //
    void reset() { m_valid = false; }
    //
    std::string channel() const { return m_repo->get(m_nodename, "name", ""s); }
    std::string next() const { return m_repo->get(m_nodename, "next", ""s); }
    std::chrono::seconds time() const {
        return std::chrono::seconds(
            (long)(m_repo->get(m_nodename, "time", -1)));
    }
};

}; // namespace TaskConfig
/**
 *
 */
class TaskState {
  public:
    TaskState(repository &state, std::string &&taskId);
    void regStateVariables();
    void update(bool run, const std::chrono::system_clock::time_point &_start);
    void update(const detail::TaskConfig::TaskWrap &task,
                const detail::TaskConfig::TasksItemWrap &item);
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

	template<typename container>
	class EventQueue {
	    using value_type = container;
	    using reference = container &;
	    using const_reference = const container &;

	  public:
	    void push_back(const_reference &d) {
	        std::lock_guard<std::mutex> lck(mutex);
	        m_data.push_back(d);
	    }
	    reference front() {
	        std::lock_guard<std::mutex> lck(mutex);
	        return m_data.front();
	    }
	    void pop_front() {
	        std::lock_guard<std::mutex> lck(mutex);
	        m_data.pop_front();
	    }
	    bool empty() {
	        std::lock_guard<std::mutex> lck(mutex);
	        return m_data.empty();
	    }

	  private:
	    std::deque<value_type> m_data;
	    std::mutex mutex;
	};

	enum event_e {e_Start, e_Stop};

  public:
    Tasks(repository &config);
    ~Tasks();
    void setup();
    void onControl(const property &p);
    repository &repo() { return m_repo; }

  private:
    void startTask(const std::string &);
    void stopTask(const std::string &);
    void task();
    void start(const std::string &);
    void stop(const std::string &req);
    void checkTimeString();
    bool checkWeekDay(const std::string &d);
    bool checkTimePoint(const std::string &d);
    void setChannel(const std::string &c, bool isOn);
    std::string activeTaskName();
    void regTaskControl(const std::string &name);
    std::string trimTaskName(const std::string &name);
    //

    repository &m_repo;
    std::map<std::string, std::shared_ptr<detail::TaskState>> m_states;
    detail::TaskConfig::TaskWrap m_activeTask;
    detail::TaskConfig::TasksItemWrap m_activeItem;
    std::chrono::seconds m_remain;
    std::thread m_thread;
    std::atomic<bool> m_aexit;
    std::chrono::steady_clock::time_point m_start;
    std::mutex m_lock;
    EventQueue<std::pair<event_e, std::string>> m_eventq;
};

#endif /* TASKS_H_ */
