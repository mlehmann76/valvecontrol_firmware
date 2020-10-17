/*
 * tasks.cpp
 *
 *  Created on: 28.09.2020
 *      Author: marco
 */

#include "tasks.h"
#include "repository.h"
#include "utilities.h"
#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

using namespace std::string_literals;
using namespace std::chrono;

namespace detail {

TaskConfig::TaskConfig(repository &r) : m_repo(r) {}

void TaskConfig::readTasks() {
    for (size_t i = 1; i <= 8; ++i) {
        std::stringstream _s;
        _s << "task" << i;
        if (!readTask(_s.str()))
            break;
        _s.clear();
    }
}

void TaskConfig::readTaskDetail(const std::string &name, Task &_t) {
    _t.m_name = m_repo.get<StringType>(name, "name");
    _t.m_alt = m_repo.get<StringType>(name, "alt-name");
    _t.m_enabled = m_repo.get<BoolType>(name, "enabled");
    _t.m_autoMode = m_repo.get<BoolType>(name, "auto");
    _t.m_autoTime = m_repo.get<StringType>(name, "starttime");
}

TaskConfig::Task::TasksItem
TaskConfig::readTaskItemDetail(const std::string &_next) {
    Task::TasksItem _item;
    _item.m_channel = m_repo.get<StringType>(_next, "name");
    _item.m_time = seconds((long)(m_repo.get<IntType>(_next, "time")));
    return _item;
}

bool TaskConfig::readTask(const std::string &name) {
    if (m_repo.find(name) != m_repo.end()) {
        Task _task;
        readTaskDetail(name, _task);
        std::string _next = m_repo.get<StringType>(name, "first");
        do {
            Task::TasksItem _item = readTaskItemDetail(_next);
            _task.m_taskitems.push_back(
                std::make_shared<Task::TasksItem>(_item));
            _next = m_repo.get<StringType>(_next, "next", "");
        } while (_next != "");
        m_tasks[name] = std::make_shared<Task>(_task);
        return true;
    } else {
        return false;
    }
}

TaskState::TaskState(repository &state) : m_stateRep(state) {}

void TaskState::regStateVariables() {
    // state : name, alt-name, running, time, remain time, channel, auto,
    // starttime
    m_stateRep.link("task", m_state);

    m_state["name"] = ""s;
    m_state["alt-name"] = ""s;

    m_state["running"] = false;
    m_state["time"] = 0;
    m_state["remain"] = 0;
    m_state["channel"] = ""s;
    m_state["auto"] = false;
    m_state["starttime"] = ""s;
}

void TaskState::update(bool run, const system_clock::time_point &_start) {
    auto t_c = system_clock::to_time_t(_start);
    m_state["running"] = run;
    if (run)
        m_state["starttime"] = std::string(std::ctime(&t_c));
}

void TaskState::update(const TaskConfig::Task &task,
                       const TaskConfig::Task::TasksItem &item) {
    m_state["name"] = task.m_name;
    m_state["alt-name"] = task.m_alt;
    m_state["channel"] = item.m_channel;
    m_state["auto"] = task.m_autoMode;
}

void TaskState::update(seconds time, seconds remain) {
    m_state["time"] = (IntType)time.count();
    m_state["remain"] = (IntType)remain.count();
}

TaskControl::TaskControl(Tasks &task, repository &ctrl) : m_repo(ctrl) {
    ctrl.create("task").set([&task](const property &p) { task.onControl(p); });
}

} // namespace detail

detail::TaskConfig::Task Tasks::NoneTask = detail::TaskConfig::Task();
detail::TaskConfig::Task::TasksItem Tasks::NoneTaskItem =
    detail::TaskConfig::Task::TasksItem();

Tasks::Tasks(repository &config, repository &state, repository &ctrl)
    : m_config(std::make_shared<detail::TaskConfig>(config)),
      m_state(std::make_shared<detail::TaskState>(state)),
      m_control(std::make_shared<detail::TaskControl>(*this, ctrl)),
      m_activeTask(&NoneTask), m_activeItem(&NoneTaskItem),
      m_thread([=]() { this->task(); }), m_aexit(false) {}

void Tasks::setup() {
    m_config->readTasks();
    m_state->regStateVariables();
}

struct DebugVisitor {
    DebugVisitor(std::stringstream &s, const std::string &f) : _s(s), _f(f) {}

    void operator()(monostate) const {
        _s << "monostate: "
           << "\n";
    }
    void operator()(BoolType i) const {
        _s << "bool: " << _f << " : " << i << "\n";
    }
    void operator()(IntType i) const {
        _s << "int: " << _f << " : " << i << "\n";
    }
    void operator()(FloatType f) const {
        _s << "float: " << _f << " : " << f << "\n";
    }
    void operator()(const StringType &s) const {
        _s << "string: " << _f << " : " << s << "\n";
    }

  private:
    std::stringstream &_s;
    std::string _f;
};

void Tasks::onControl(const property &p) {
    /*
     * control Message
     * { "task" : { "name" : "task|[n]", "value" : "ON|OFF" } }
     */
    // std::cout << "onControl : name -> " << p.name() << "\n";

    std::stringstream s;
    for (auto &v : p) {
        mapbox::util::apply_visitor(DebugVisitor(s, v.first), v.second);
    }
    // std::cout << "onControl : -> " << s.str() << "\n";

    auto nit = p.find("name");
    auto end = p.end();
    if (m_nextRequest == "" && nit != end) {
        m_nextRequest = nit->second.get_unchecked<StringType>();
    } else {
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
}

void Tasks::startTask() {
    // ON case
    // test, if task is already running
    if (!m_state->running()) {
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
        m_state->update(true, m_start);
    }
    std::cout << "startTask"
              << "\n";
}

void Tasks::start(listIterator _findIter) {
    m_activeItem = &**_findIter;
    m_control->repo()[m_activeItem->m_channel]["value"] = "ON"s;
    m_state->update(*m_activeTask, *m_activeItem);
    m_state->update(seconds(0), m_remain);
    m_start = system_clock::now();
}

void Tasks::stopTask() {
    // OFF case
    // test, if task running
    if (m_state->running() &&
        (m_nextRequest == "task" || m_nextRequest == m_state->name())) {
        std::lock_guard<std::mutex> lock(m_lock);
        // update channel
        m_control->repo()[m_activeItem->m_channel]["value"] = "OFF"s;
        // stop task
        stop();
    }
}

void Tasks::stop() {
    m_activeItem = &NoneTaskItem;
    m_activeTask = &NoneTask;
    m_state->update(false, system_clock::now());
    m_state->update(*m_activeTask, *m_activeItem);
    m_remain = seconds(0);
    m_state->update(seconds(0), seconds(0));
}

Tasks::~Tasks() {
    m_aexit = true;
    m_thread.join();
}

void Tasks::task() {
    system_clock::time_point _last = system_clock::now();
    while (m_aexit == false) {
        if (m_activeTask != &NoneTask) {
            std::lock_guard<std::mutex> lock(m_lock);
            // make sure, that tdiff is positive even after system clock change
            auto tdiff = system_clock::now() > m_start
                             ? system_clock::now() - m_start
                             : m_start - system_clock::now();

            if ((system_clock::now() - _last) >= milliseconds(250)) {
                _last = system_clock::now();
                m_state->update(duration_cast<seconds>(tdiff),
                                duration_cast<seconds>(m_remain - tdiff));
            }
            if (tdiff >= m_activeItem->m_time) {
                m_remain -= m_activeItem->m_time;
                m_control->repo()[m_activeItem->m_channel]["value"] = "OFF"s;
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
        std::this_thread::sleep_for(milliseconds(200));
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
    wd << std::put_time(std::gmtime(&t),
                        "%w"); // writes weekday as a decimal number, where
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
