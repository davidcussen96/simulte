#include "stack/subchannel/Subchannel.h"

Subchannel::Subchannel()
{
    isFree = true;
}

Subchannel::Subchannel(RbMap rbmap, Sci s, int rsrp, int rssi, simtime_t time)
{
    rbmap = rbmap;
    sci = s;
    rsrp = rsrp;
    rssi = rssi;
    isFree = false;
    notSensed = false;
}

bool Subchannel::isRsrpGreaterThan(int Th)
{
    if (getRsrp() < Th)
    {
        return true;
    }
    return false;
}
