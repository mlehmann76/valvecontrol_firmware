/*
 * ConnectionObserver.h
 *
 *  Created on: 03.08.2020
 *      Author: marco
 */

#ifndef MAIN_ICONNECTIONOBSERVER_H_
#define MAIN_ICONNECTIONOBSERVER_H_

class iConnectionObserver {
  public:
    virtual ~iConnectionObserver() = default;
    virtual void onConnect() = 0;
    virtual void onDisconnect() = 0;
};

#endif /* MAIN_ICONNECTIONOBSERVER_H_ */
