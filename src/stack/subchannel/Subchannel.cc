#include "stack/subchannel/Subchannel.h"
#include "stack/sci/Sci.h"

Subchannel::Subchannel()
{
    isFree = true;
}

Subchannel::Subchannel(Sci* s, int rp, int ri, uint16_t sid, int subch)
{
    sci = s;
    rsrp = rp;
    rssi = ri;
    isFree = false;
    notSensed = false;
    sourceId = sid;
    subchannel = subch;
}

Subchannel::Subchannel(int rp, int ri, uint16_t sid, int subch)
{
    rsrp = rp;
    rssi = ri;
    isFree = false;
    notSensed = false;
    sourceId = sid;
    subchannel = subch;
}


bool Subchannel::isRsrpLessThan(int Th)
{
    if (getRsrp() < Th)
    {
        return true;
    }
    return false;
}

Subchannel* Subchannel::add(Subchannel* sc)
{
    int rsrp = this->getRsrp() + sc->getRsrp();
    int rssi = this->getRssi() + sc->getRssi();

    return new Subchannel(this->getSci(), rsrp, rssi, this->getSourceId(), this->getSubchannel());
}

/*
Subchannel* Subchannel::operator+(Subchannel &s1, Subchannel &s2)
{
    int rsrp = s1.getRsrp() + s2.getRsrp();
    int rssi = s1.getRssi() + s2.getRssi();

    // TODO How do we combine/add the 2 rbmaps -ANS They know their own subchannel index and therefore rbmap.
    return new Subchannel(s1.getSci(), s1.getRbMap(), rsrp, rssi, s1.getSourceId(), s1.getSubchannel());
}


Subchannel* operator+(Subchannel& a, Subchannel& b)
{
    int rsrp = a.getRsrp() + b.getRsrp();
    int rssi = a.getRssi() + b.getRssi();
    Sci* sci = a.getSci();

    // TODO How do we combine/add the 2 rbmaps -ANS They know their own subchannel index and therefore rbmap.
    return new Subchannel(sci, a.getRbMap(), rsrp, rssi, a.getSourceId(), a.getSubchannel());
}
*/
