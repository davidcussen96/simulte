#pragma once

#include "stack/sci/Sci.h"
#include "common/LteCommon.h"

class Subchannel
{
private:
    //RbMap rbmap;        // Don't think Subchannel needs RbMap, can be determined using subchannel.
    Sci* sci;
    int rsrp;
    int rssi;
    bool notSensed;
    bool isFree = true;
    int subframe;
    int subchannel;
    uint16_t sourceId;

public:
    Subchannel();

    Subchannel(Sci* sci, int rsrp, int rssi, uint16_t sourceId, int subch);

    Subchannel(int rsrp, int rssi, uint16_t sourceId, int subch);

    // Copy constructor
    Subchannel(const Subchannel &s2) {isFree = s2.isFree;}

    bool isRsrpLessThan(int Th);

    //RbMap getRbMap() {return rbmap;}
    Sci* getSci() {return sci;}
    int getRsrp() {return rsrp;}
    int getRssi() {return rssi;}
    bool getNotSensed() {return notSensed;}

    void setNotSensed(bool sensed) {notSensed = sensed; isFree = false;}

    void setSubframe(int frame) {subframe = frame;}
    int getSubframe() {return subframe;}
    void setSubchannel(int channel) {subchannel = channel;}
    int getSubchannel() {return subchannel;}


    void setSourceId(uint16_t sid) {sourceId = sid;}
    uint16_t getSourceId() {return sourceId;}

    bool getIsFree() {return isFree;}
    void setIsFree(bool f) {isFree = f;}

    friend Subchannel operator+(Subchannel &s1, Subchannel &s2);

    Subchannel* add(Subchannel* sc);
};

//Subchannel* operator+(Subchannel& a, Subchannel& b);
