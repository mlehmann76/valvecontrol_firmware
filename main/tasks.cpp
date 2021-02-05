/*
 * tasks.cpp
 *
 *  Created on: 28.09.2020
 *      Author: marco
 */

#include "tasks.h"
#include "config_user.h"
#include "repository.h"
#include "utilities.h"
#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <logger.h>

using namespace std::string_literals;
using namespace std::chrono;

static const char* TAG = "tasks";

namespace detail {

TaskConfig::TaskConfig(repository &r) : m_repo(r) {}

void TaskConfig::readTasks() {
	m_tasks.clear();
    for (size_t i = 1; i <= 8; ++i) {
        std::stringstream _s;
        _s << "/tasks/task" << i;
        if (!readTask(_s.str()))
            break;
        _s.clear();
    }
}

void TaskConfig::readTaskDetail(const std::string &name, Task &_t) const {
    _t.m_name = m_repo.get<StringType>(name, "name"); // task1
    _t.m_alt = m_repo.get<StringType>(name, "alt-name");
    _t.m_enabled = m_repo.get<BoolType>(name, "enabled");
    _t.m_autoMode = m_repo.get<BoolType>(name, "auto");
    _t.m_autoTime = m_repo.get<StringType>(name, "starttime");
}

TaskConfig::Task::TasksItem
TaskConfig::readTaskItemDetail(const std::string &_next) const {
    Task::TasksItem _item;
    _item.m_channel = m_repo.get<StringType>(_next, "name");
    _item.m_time = seconds((long)(m_repo.get<IntType>(_next, "time")));
    return _item;
}

bool TaskConfig::readTask(const std::string &name) {
    const std::string _taskName = name + "/config";
    if (m_repo.find(_taskName) != m_repo.end()) {
        auto _task = std::make_shared<Task>();
        m_tasks[name] = _task;
        readTaskDetail(_taskName, *_task);
        // node name is "/tasks/task1/config/node1"
        std::string _next = m_repo.get<StringType>(_taskName, "first");
        do {
            _task->m_taskitems.push_back(
                std::make_shared<Task::TasksItem>(readTaskItemDetail(_next)));
            _next = m_repo.get<StringType>(_next, "next", "");
        } while (_next != "");
        return true;
    } else {
        return false;
    }
}

TaskState::TaskState(repository &state, std::string &&taskId)
    : m_stateRep(state), m_taskId(taskId + "/state") {}

void TaskState::regStateVariables() {
    m_stateRep[m_taskId]["running"] = false;
    m_stateRep[m_taskId]["auto"] = false;
    m_stateRep[m_taskId]["time"] = 0;
    m_stateRep[m_taskId]["remain"] = 0;
    m_stateRep[m_taskId]["channel"] = ""s;
    m_stateRep[m_taskId]["starttime"] = ""s;
}

void TaskState::update(bool run, const system_clock::time_point &_start) {
    auto t_c = system_clock::to_time_t(_start);
    m_stateRep[m_taskId]["running"] = run;
    if (run)
        m_stateRep[m_taskId]["starttime"] = std::string(std::ctime(&t_c));
}

void TaskState::update(const detail::TaskConfig::Task &task,
                       const TaskConfig::Task::TasksItem &item) {
    m_stateRep[m_taskId]["channel"] = item.m_channel;
    m_stateRep[m_taskId]["auto"] = task.m_autoMode;
}

void TaskState::update(seconds time, seconds remain) {
    m_stateRep[m_taskId]["time"] = (IntType)time.count();
    m_stateRep[m_taskId]["remain"] = (IntType)remain.count();
}

bool TaskState::running() const {
    return m_stateRep[m_taskId]["running"].get<BoolType>();
}

} // namespace detail

detail::TaskConfig::Task Tasks::NoneTask = detail::TaskConfig::Task();
detail::TaskConfig::Task::TasksItem Tasks::NoneTaskItem =
    detail::TaskConfig::Task::TasksItem();

Tasks::Tasks(repository &config)
    : m_repo(config), m_config(std::make_shared<detail::TaskConfig>(config)),
      m_activeTask(&NoneTask), m_activeItem(&NoneTaskItem),
      m_thread([=]() { this->task(); }), m_aexit(false) {}

void Tasks::setup() {
	//for restarting setup, make sure task is not using data
	std::lock_guard<std::mutex> lock(m_lock);
	m_activeTask = &NoneTask;
	m_activeItem = &NoneTaskItem;
    m_config->readTasks();
    // generate taskState for every task
    // register state variables for all tasks
    for (const auto &_item : m_config->map()) {
        auto _state = std::make_shared<detail::TaskState>(
            m_repo, std::string(_item.first));
        m_states[_item.first] = _state; // tasks/task1
        _state->regStateVariables();
        regTaskControl(_item.first);
    }
}

void Tasks::onControl(const property &p) {
    /*
     * control Message
     * "{ \"tasks\" : { \"task5\" : { \"control\" : { \"value\" : \"ON|OFF\" } }
     * } }")
     */
    m_nextRequest =
        p.name().erase(p.name().find_last_of("/"), p.name().length());

    auto end = p.end();
    auto vit = p.find("value");
    if (vit != end && vit->second.is<StringType>()) {
        std::string s = vit->second.get_unchecked<StringType>();
        if (s == "on" || s == "ON" || s == "On") {
            startTask();
        } else {
            stopTask();
        }
        m_nextRequest = "";
    }
}

std::string Tasks::activeTaskName() { return "/tasks/" + m_activeTask->m_name; }

void Tasks::startTask() {
    // ON case
    // test, if task is already running
    if (!m_states[m_nextRequest]->running()) {
        std::lock_guard<std::mutex> lock(m_lock);
        // start task, if not
        m_activeTask = &m_config->get(m_nextRequest);
        m_activeItem = ((*m_activeTask->m_taskitems.begin()).get());
        // set remain to sum of all times
        m_remain = seconds(0);
        for (auto &_t : m_activeTask->m_taskitems) {
            m_remain += _t->m_time;
        }
        start(m_activeTask->m_taskitems.begin());
        m_states[activeTaskName()]->update(true, system_clock::now());
    }
}

void Tasks::start(listIterator _findIter) {
    m_activeItem = &**_findIter;
    setChannel(m_activeItem->m_channel, true);
    m_states[activeTaskName()]->update(*m_activeTask, *m_activeItem);
    m_states[activeTaskName()]->update(seconds(0), m_remain);
    m_start = steady_clock::now();
}

void Tasks::stopTask() {
    // OFF case
    // test, if task running
    if (m_states[m_nextRequest]->running()) {
        std::lock_guard<std::mutex> lock(m_lock);
        // update channel
        setChannel(m_activeItem->m_channel, false);
        // stop task
        stop();
    }
}

void Tasks::stop() {
    m_states[activeTaskName()]->update(false, system_clock::now());
    m_states[activeTaskName()]->update(*m_activeTask, *m_activeItem);
    m_remain = seconds(0);
    m_states[activeTaskName()]->update(seconds(0), seconds(0));

    m_activeItem = &NoneTaskItem;
    m_activeTask = &NoneTask;
}

Tasks::~Tasks() {
    m_aexit = true;
    m_thread.join();
}

void Tasks::task() {
	steady_clock::time_point _last = steady_clock::now();
    while (m_aexit == false) {
        if (m_activeTask != &NoneTask) {
            std::lock_guard<std::mutex> lock(m_lock);
            // make sure, that tdiff is positive even after system clock change
            auto tdiff = steady_clock::now() - m_start;

			m_states[activeTaskName()]->update(
				duration_cast<seconds>(tdiff),
				m_remain - duration_cast<seconds>(tdiff));
            //}
            if (tdiff >= m_activeItem->m_time) {
                m_remain -= m_activeItem->m_time;
                setChannel(m_activeItem->m_channel, false);
                // switch to the next channel
                auto _iterend = m_activeTask->m_taskitems.end();
                auto _findIter = m_activeTask->m_taskitems.begin();

                while (_findIter != _iterend &&
                       (&**_findIter != m_activeItem)) {
                    ++_findIter;
                }

                if (++_findIter != _iterend) {
                    start(_findIter);
                } else {
                    stop();
                }
            }
        } else {
            checkTimeString();
        }
        std::this_thread::sleep_for(milliseconds(500));
    } // while
}

void Tasks::checkTimeString() {
    // timeString "weekday,weekday,... time (H:M)"
    for (auto &_item : m_config->map()) {
        if (_item.second->m_autoMode) {
            auto parts = utilities::split(_item.second->m_autoTime, " ");
            if (parts.size() > 1 && checkWeekDay(parts[0]) &&
                checkTimePoint(parts[1])) {
                m_nextRequest = _item.second->m_name;
                startTask();
                break;
            }
        }
    }
}

bool Tasks::checkWeekDay(const std::string &d) {
    auto parts = utilities::split(d, ",");
    std::time_t t = std::time(nullptr);
    std::stringstream wd;
    wd << std::put_time(std::gmtime(&t), "%w");
    // writes weekday as a decimal number, where
    // Sunday is 0 (range [0-6])
    const std::string today = wd.str();
    for (const auto &s : parts) {
        if (s == today)
            return true;
    }
    return false;
}

bool Tasks::checkTimePoint(const std::string &d) {
    auto now = system_clock::now();
    auto in_time_t = system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%T");

    auto tp = toTimePoint(d);
    auto tpn = toTimePoint(ss.str());

    auto diff = tp > tpn ? tp - tpn : tpn - tp;
    return diff < seconds(1) ? true : false;
}

system_clock::time_point Tasks::toTimePoint(const std::string &d) {
    std::tm tm = {};
    std::stringstream ss(d);
    ss >> std::get_time(&tm, "%H:%M:%S");
    return system_clock::from_time_t(std::mktime(&tm));
}

void Tasks::setChannel(const std::string &c, bool isOn) {
    m_repo["/actors/" + m_activeItem->m_channel + "/control"]["value"] =
        isOn ? "ON"s : "OFF"s; // FIXME Magic Strings
}

void Tasks::regTaskControl(const std::string &name) {
    std::string _taskId = name + "/control";
    m_repo[_taskId]["value"] = "OFF"s;
    m_repo[_taskId].set([this](const property &p) -> std::optional<property> {
        onControl(p);
        return {};
    });
}
