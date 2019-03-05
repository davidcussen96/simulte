#pragma once

#include "stack/phy/packet/CsrMessage_m.h"
#include "stack/subchannel/Subchannel.h"

class CsrMessage : public CsrMessage_Base
{

private:
    std::vector<Subchannel*> csrList;
    
public:
    //CsrMessage();

    //CsrMessage(const char *name=nullptr, short kind=0);

    virtual std::vector<Subchannel*> getCsrList()
    {
        return csrList;
    }
    virtual void setCsrList(std::vector<Subchannel*> c)
    {
        csrList = c;
    }
};    
