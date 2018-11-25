// Author ugo (Ugo Di Girolamo)

#ifndef _ACTION__H_
#define _ACTION__H_

#include <Arduino.h>

class Action {
 public:
  Action(int zone, int heatSetpoint, int coolSetpoint) :
    zone(zone),
    heatSetpoint(heatSetpoint),
    coolSetpoint(coolSetpoint) {
  }

  const int zone;
  const int heatSetpoint;
  const int coolSetpoint;
};

Action* actionFromJson(String &data);

#endif  // _ACTION__H_
