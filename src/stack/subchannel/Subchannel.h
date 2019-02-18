#include "lte/src/stack/sci/Sci.h"

class Subchannel
{
private:
    RbMap rbmap;
    Sci sci;
    int rsrp;
    int rssi;
    bool notSensed;
    bool isFree = true;
    int subframe;
    int subchannel;
    simtime_t timeReceived;

public:
    Subchannel();

    Subchannel(RbMap rbmap, Sci sci, int rsrp, int rssi, simtime_t timeRec);

    bool isRsrpGreaterThan(int Th);

    RbMap getRbMap() {return rbmap;}
    Sci getSci() {return sci;}
    int getRsrp() {return rsrp;}
    int getRssi() {return rssi;}
    bool getNotSensed() {return notSensed;}

    void setNotSensed(bool sensed) {notSensed = sensed; isFree = false;}

    void setSubframe(int frame) {subframe = frame;}
    int getSubframe() {return subframe;}
    void setSubchannel(int channel) {subchannel = channel;}
    int getSubchannel() {return subchannel;}

    simtime_t getTime() {return timeReceived;}
};
