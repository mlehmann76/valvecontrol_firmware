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
#include <esp_pthread.h>
#include <iomanip>
#include <logger.h>
#include <sstream>
#include <string>

using namespace std::string_literals;
using namespace std::chrono;

static const char *TAG = "tasks";

namespace detail {

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
    m_stateRep[m_taskId]["running"] = run;
    if (run) {
        auto t_c = system_clock::to_time_t(_start);
        m_stateRep[m_taskId]["starttime"] = std::string(std::ctime(&t_c));
    }
}

void TaskState::update(const TaskConfig::TaskWrap &task,
                       const TaskConfig::TasksItemWrap &item) {
    m_stateRep[m_taskId]["channel"] = item.channel();
    m_stateRep[m_taskId]["auto"] = task.autoMode();
}

void TaskState::update(seconds time, seconds remain) {
    m_stateRep[m_taskId]["time"] = (IntType)time.count();
    m_stateRep[m_taskId]["remain"] = (IntType)remain.count();
}

bool TaskState::running() const {
    return m_stateRep[m_taskId]["running"].get<BoolType>();
}

} // namespace detail

Tasks::Tasks(repository &config)
    : m_repo(config), m_activeTask{config, ""}, m_activeItem{config, ""},
      m_thread(), m_aexit(false) {}

void Tasks::setup() {
    // for restarting setup, make sure task is not using data
    std::lock_guard<std::mutex> lock(m_lock);
    m_activeTask.reset();
    m_activeItem.reset();
    // generate taskState for every task
    // register state variables for all tasks
    for (int i = 0; i < 8; i++) {
        detail::TaskConfig::TaskWrap _task = {m_repo, i};
        if (_task.valid()) {
            log_inst.debug(TAG, "setup: {%d-%s} valid", i,
                           _task.taskName().c_str());

            auto _state =
                std::make_shared<detail::TaskState>(m_repo, _task.taskName());
            m_states[_task.taskName()] = _state; // tasks/task1
            _state->regStateVariables();
            regTaskControl(_task.taskName());
        }
    }
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = "tasks task";
    esp_pthread_set_cfg(&cfg);
    m_thread = std::thread([this]() { this->task(); });
}

std::string Tasks::trimTaskName(const std::string &name) {
    return std::string(name).erase(name.find_last_of("/"), name.length());
}

void Tasks::onControl(const property &p) {
    /*
     * control Message
     * "{ \"tasks\" : { \"task5\" : { \"control\" : { \"value\" : \"ON|OFF\" } }
     * } }")
     */
    const std::string name = trimTaskName(p.name());

    log_inst.info(TAG, "onControl: {%s}", name.c_str());

    auto end = p.end();
    auto vit = p.find("value");
    if (vit != end && std::get_if<StringType>(&vit->second)) {
        std::string s = *std::get_if<StringType>(&vit->second);
        if (s == "on" || s == "ON" || s == "On") {
            startTask(name);
        } else {
            stopTask(name);
        }
    }
}

std::string Tasks::activeTaskName() { return m_activeTask.taskName(); }

void Tasks::startTask(const std::string &req) {
    // ON case
    // FIXME stop active task
    // test, if task is already running
    log_inst.debug(TAG, "startTask: %s, active valid %d", req.c_str(),
                   m_activeTask.valid());

    if (!m_activeTask.valid() || (m_states.count(activeTaskName()) &&
                                  !m_states[activeTaskName()]->running())) {

        std::lock_guard<std::mutex> lock(m_lock);
        detail::TaskConfig::TaskWrap _task = {repo(), req};
        auto first = _task.first();
        detail::TaskConfig::TasksItemWrap _item = {repo(), first};
        log_inst.debug(TAG, "Task: valid %d, Item: %s valid %d", _task.valid(),
                       first.c_str(), _item.valid());
        // start task, if not
        if (_task.valid() && _item.valid()) {
            m_activeTask = _task;
            m_activeItem = _item;
            // set remain to sum of all times
            m_remain = seconds(0);
            while (_item.valid()) {
                m_remain += _item.time();
                _item = {repo(), _item.next()};
            }
            start(m_activeTask.first());
            m_states[activeTaskName()]->update(true, system_clock::now());
        }
    }
}

void Tasks::start(const std::string &item) {
    m_activeItem = {repo(), item};
    if (m_activeItem.valid()) {
        setChannel(m_activeItem.channel(), true);
        m_states[activeTaskName()]->update(m_activeTask, m_activeItem);
        m_states[activeTaskName()]->update(seconds(0), m_remain);
        m_start = steady_clock::now();
    }
}

void Tasks::stopTask(const std::string &req) {
    // OFF case
    // test, if task running
    log_inst.debug(TAG, "stopTask: m_nextRequest() %s", req.c_str());
    if (m_states.count(req) && m_states[req]->running()) {
        std::lock_guard<std::mutex> lock(m_lock);
        // update channel
        setChannel(m_activeItem.channel(), false);
        // stop task
        stop(req);
    }
}

void Tasks::stop(const std::string &req) {
    if (m_states.count(req)) {
        m_states[req]->update(false, system_clock::now());
        m_states[req]->update(m_activeTask, m_activeItem);
        m_remain = seconds(0);
        m_states[req]->update(seconds(0), seconds(0));
    }
    m_activeItem.reset();
    m_activeTask.reset();
}

Tasks::~Tasks() {
    m_aexit = true;
    m_thread.join();
}

void Tasks::task() {
    // steady_clock::time_point _last = steady_clock::now();
    while (m_aexit == false) {
        if (m_activeTask.valid()) {
            std::lock_guard<std::mutex> lock(m_lock);
            // make sure, that tdiff is positive even after system clock change
            auto tdiff = steady_clock::now() - m_start;
            if (m_states.count(activeTaskName())) {
                m_states[activeTaskName()]->update(
                    duration_cast<seconds>(tdiff),
                    m_remain - duration_cast<seconds>(tdiff));
            }
            if (tdiff >= m_activeItem.time()) {
                m_remain -= m_activeItem.time();
                setChannel(m_activeItem.channel(), false);
                // switch to the next channel
                detail::TaskConfig::TasksItemWrap _item = {repo(),
                                                           m_activeItem.next()};
                if (_item.valid()) {
                    start(m_activeItem.next());
                } else {
                    stop(activeTaskName());
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
    for (int i = 0; i < 8; i++) {
        detail::TaskConfig::TaskWrap _task = {m_repo, i};
        if (_task.valid()) {
            if (_task.enabled() && _task.autoMode()) {
                auto parts = utilities::split(_task.autoTime(), " ");
                if (parts.size() > 1 && checkWeekDay(parts[0]) &&
                    checkTimePoint(parts[1])) {
                    startTask(_task.taskName());
                    break;
                }
            }
        }
    }
}

bool Tasks::checkWeekDay(const std::string &d) {
    auto parts = utilities::split(d, ",");
    std::time_t t = std::time(nullptr);
    std::stringstream wd;
    wd << std::put_time(std::gmtime(&t), "%u");
    // writes weekday as a decimal number, where
    // Sunday is 7 (range [1-7])
    const std::string today = wd.str();
    for (const auto &s : parts) {
        if (s == today) {
            return true;
        }
    }
    return false;
}

bool Tasks::checkTimePoint(const std::string &d) {
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    auto parts = utilities::split(d, ":");
    if (parts.size()==2) {
    	auto _h = utilities::strtol(parts[0],10);
    	auto _m = utilities::strtol(parts[1],10);
    	if (_h && *_h == tm.tm_hour && _m && *_m == tm.tm_min) {
    		return true;
    	}
    }
    return false;
}

void Tasks::setChannel(const std::string &c, bool isOn) {
    m_repo["/actors/" + m_activeItem.channel() + "/control"]["value"] =
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
