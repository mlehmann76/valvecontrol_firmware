/*
 * mqtt_client.h
 *
 *  Created on: 28.06.2018
 *      Author: marco
 */

#ifndef MAIN_MQTT_USER_H_
#define MAIN_MQTT_USER_H_

#include "iConnectionObserver.h"
#include <memory>
#include <mqtt_client.h>
#include <set>
#include <string>
#include <vector>

class MainClass;

namespace mqtt {

class AbstractMqttReceiver {
  public:
    virtual ~AbstractMqttReceiver() = default;
    virtual int onMessage(esp_mqtt_event_handle_t event) = 0;
    virtual std::string topic() = 0;
};

struct mqttMessage {
    std::string m_topic;
    std::string m_data;
    mqttMessage() = default;
    mqttMessage(const std::string &_topic, const std::string &_data)
        : m_topic(_topic), m_data(_data) {}
};

using MqttQueueType = std::unique_ptr<mqttMessage>;
/* this doesnt work, since queue only supports pods
using PubQueue = Queue<MqttQueueType,10>;
*/
class MqttWorker;
class MqttConnectionObserver : public iConnectionObserver {
  public:
    MqttConnectionObserver(MqttWorker *_m) : m_mqtt(_m) {}
    virtual void onConnect();
    virtual void onDisconnect();

  private:
    MqttWorker *m_mqtt;
};

class MqttWorker {
    friend MqttConnectionObserver;

  public:
    MqttWorker() : m_obs(this) {}
    virtual ~MqttWorker() = default;
    void init(void);
    iConnectionObserver &obs() { return m_obs; }
    std::set<std::string> subTopics() const { return m_subtopics; }
    void handle(esp_mqtt_event_handle_t event);
    void addHandle(AbstractMqttReceiver *);

    void send(MqttQueueType);

  private:
    static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);
    void connect(void);
    void disconnect(void);
    void send(mqttMessage *);

    MqttConnectionObserver m_obs;
    std::vector<AbstractMqttReceiver *> m_mqttRec;
    AbstractMqttReceiver *m_lastMqttRec = nullptr;
    std::set<std::string> m_subtopics;
    esp_mqtt_client_handle_t client = NULL;
    bool isMqttConnected = false;
    bool isMqttInit = false;
    std::string m_server;
    std::string m_user;
    std::string m_pass;
};

inline void MqttConnectionObserver::onConnect() { m_mqtt->connect(); }

inline void MqttConnectionObserver::onDisconnect() { m_mqtt->disconnect(); }

} // namespace mqtt
#endif /* MAIN_MQTT_USER_H_ */
