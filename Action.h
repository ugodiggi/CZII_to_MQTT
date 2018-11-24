// Author ugo (Ugo Di Girolamo)

#ifndef _ACTION__H_
#define _ACTION__H_

class Action {
 public:
  Action(int zone, int heatSetpointDelta, int coolSetpointDelta) :
    zone(zone),
    heatSetpointDelta(heatSetpointDelta),
    coolSetpointDelta(coolSetpointDelta) {
  }

  const int zone;
  const int heatSetpointDelta;
  const int coolSetpointDelta;
};

#endif  // _ACTION__H_
