//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//
#pragma once

#include "stack/mac/packet/LteSchedulingGrant_m.h"
#include "common/LteCommon.h"
#include "stack/mac/amc/UserTxParams.h"
#include "stack/mac/packet/LteSchedulingGrant.h"
#include "stack/subchannel/Subchannel.h"

class UserTxParams;

class LteSchedulingGrant : public LteSchedulingGrant_Base
{
  protected:

    const UserTxParams* userTxParams;
    RbMap grantedBlocks;
    std::vector<unsigned int> grantedCwBytes;
    Direction direction_;

  public:

    LteSchedulingGrant(const char *name = NULL, int kind = 0) :
        LteSchedulingGrant_Base(name, kind)
    {
        userTxParams = NULL;
        grantedCwBytes.resize(MAX_CODEWORDS);
    }

    ~LteSchedulingGrant()
    {

        if (userTxParams != NULL)
        {
            userTxParams = NULL;
            delete userTxParams;
        }
    }

    LteSchedulingGrant(const LteSchedulingGrant& other) :
        LteSchedulingGrant_Base(other.getName())
    {
        operator=(other);
    }

    LteSchedulingGrant& operator=(const LteSchedulingGrant& other)
    {
        if (other.userTxParams != NULL)
        {
            const UserTxParams* txParams = check_and_cast<const UserTxParams*>(other.userTxParams);
            userTxParams = txParams->dup();
        }
        else
        {
            userTxParams = 0;
        }
        grantedBlocks = other.grantedBlocks;
        grantedCwBytes = other.grantedCwBytes;
        direction_ = other.direction_;
        LteSchedulingGrant_Base::operator=(other);
        return *this;
    }

    virtual LteSchedulingGrant *dup() const
    {
        return new LteSchedulingGrant(*this);
    }

    void setUserTxParams(const UserTxParams* arg)
    {
        if(userTxParams){
            delete userTxParams;
        }
        userTxParams = arg;
    }

    const UserTxParams* getUserTxParams() const
    {
        return userTxParams;
    }

    const unsigned int getBlocks(Remote antenna, Band b) const
        {
        return grantedBlocks.at(antenna).at(b);
    }

    void setBlocks(Remote antenna, Band b, const unsigned int blocks)
    {
        grantedBlocks[antenna][b] = blocks;
    }

    const RbMap& getGrantedBlocks() const
    {
        return grantedBlocks;
    }

    void setGrantedBlocks(const RbMap& rbMap)
    {
        grantedBlocks = rbMap;
    }

    virtual void setGrantedCwBytesArraySize(unsigned int size)
    {
        grantedCwBytes.resize(size);
    }
    virtual unsigned int getGrantedCwBytesArraySize() const
    {
        return grantedCwBytes.size();
    }
    virtual unsigned int getGrantedCwBytes(unsigned int k) const
    {
        return grantedCwBytes.at(k);
    }
    virtual void setGrantedCwBytes(unsigned int k, unsigned int grantedCwBytes_var)
    {
        grantedCwBytes[k] = grantedCwBytes_var;
    }
    void setDirection(Direction dir)
    {
        direction_ = dir;
    }
    Direction getDirection() const
    {
        return direction_;
    }
};


class LteSchedulingGrantMode4 : public LteSchedulingGrant
{
private:
    unsigned int priority;
    unsigned int resourceReservation;
    unsigned int freqResource;
    unsigned int timeGap;
    unsigned int mcs;
    unsigned int retransIndex;
    unsigned int reservedInfo;

    Subchannel* csr;

public:

    ~LteSchedulingGrantMode4()
    {
        if (csr != nullptr)
        {
            csr = NULL;
            delete csr;
        }

    }
    virtual void setCsr(Subchannel* subchannel)
    {
        csr = subchannel;
    }
    virtual Subchannel* getCsr()
    {
        return csr;
    }

    virtual unsigned int getPriority() {return priority;}
    virtual void setPriority(unsigned int p) {priority = p;}

    virtual unsigned int getResourceReservation()
    {
        return resourceReservation;
    }
    virtual void setResourceReservation(unsigned int rr)
    {
        resourceReservation = rr;
    }

    virtual unsigned int getFreqResource() {return freqResource;}
    virtual void setFreqResource(unsigned int fr) {freqResource = fr;}

    virtual unsigned int getTimeGap() {return timeGap;}
    virtual void setTimeGap(unsigned int tg) {timeGap = tg;}

    virtual unsigned int getMcs() {return mcs;}
    virtual void setMcs(unsigned int m) {mcs = m;}

    virtual unsigned int getRetransIndex() {return retransIndex;}
    virtual void setRetransIndex(unsigned int ri) {retransIndex = ri;}

    virtual unsigned int getReservedInfo() {return reservedInfo;}
    virtual void setReservedInfo(unsigned int ri) {reservedInfo = ri;}
};
