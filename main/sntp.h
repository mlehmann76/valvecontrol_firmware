/*
 * sntp.h
 *
 *  Created on: 27.04.2019
 *      Author: marco
 */

#ifndef MAIN_SNTP_H_
#define MAIN_SNTP_H_

#include <string>

class SntpSupport {
  public:
    void init();
    bool update();

  private:
    std::string server;
    std::string timeZone;
};
#endif /* MAIN_SNTP_H_ */
