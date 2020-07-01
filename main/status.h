/*
 * status.h
 *
 *  Created on: 25.03.2019
 *      Author: marco
 */

#ifndef MAIN_STATUS_H_
#define MAIN_STATUS_H_

#ifdef __cplusplus
extern "C" {
#endif

extern EventGroupHandle_t status_event_group;

#define STATUS_EVENT_CONTROL (1u<<0)
#define STATUS_EVENT_FIRMWARE (1u<<1)
#define STATUS_EVENT_HARDWARE (1u << 2)
#define STATUS_EVENT_SENSOR (1u<<3)
#define STATUS_EVENT_ALL (0xFFFFFFFF)

void status_task_setup(void);
void status_task(void *pvParameters);



#ifdef __cplusplus
}
#endif

class StatusProvider {
public:
	virtual ~StatusProvider() {}
	virtual bool hasUpdate() = 0;
	virtual void addStatus(cJSON *) = 0;
private:
	uint32_t m_bit;
};

class FirmwareStatus : public StatusProvider {
public:
	virtual ~FirmwareStatus();
	virtual bool hasUpdate();
	virtual void addStatus(cJSON *);
};

class HardwareStatus : public StatusProvider {
public:
	virtual ~HardwareStatus();
	virtual bool hasUpdate();
	virtual void addStatus(cJSON *);
};


#endif /* MAIN_STATUS_H_ */
