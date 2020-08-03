/*
 * ConnectionObserver.h
 *
 *  Created on: 03.08.2020
 *      Author: marco
 */

#ifndef MAIN_CONNECTIONOBSERVER_H_
#define MAIN_CONNECTIONOBSERVER_H_


class ConnectionObserver {
public:
	virtual ~ConnectionObserver() = default;
	virtual void onConnect() = 0;
	virtual void onDisconnect() = 0;
};



#endif /* MAIN_CONNECTIONOBSERVER_H_ */
