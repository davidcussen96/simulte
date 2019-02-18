/*
 * LteSchedulingGrantMode4.h
 *
 *  Created on: Feb 18, 2019
 *      Author: djc10
 */

#include "stack/mac/packet/LteSchedulingGrant_m.h"
#include "stack/mac/packet/LteSchedulingGrantMode4.h"
#include "common/LteCommon.h"
#include "stack/mac/amc/UserTxParams.h"

class UserTxParams;

class LteSchedulingGrantMode4 : public LteSchedulingGrant
{
  protected:

    const UserTxParams* userTxParams;
    RbMap grantedBlocks;
    std::vector<unsigned int> grantedCwBytes;
    Direction direction_;
    unsigned int priority_;
    unsigned int resourceRes_;
    unsigned int freqResource_;
    unsigned int timeGap_;
    unsigned int mcs_;
    unsigned int retransIndex_;
    unsigned int slResourceReselectionCounter_;

  public:

    LteSchedulingGrantMode4(const char *name = NULL, int kind = 0) :
        LteSchedulingGrant(name, kind)
    {
        userTxParams = NULL;
        grantedCwBytes.resize(MAX_CODEWORDS);
    }

    ~LteSchedulingGrantMode4()
    {
        if (userTxParams != NULL)
        {
            delete userTxParams;
            userTxParams = NULL;
        }
    }

    LteSchedulingGrantMode4(const LteSchedulingGrantMode4& other) :
        LteSchedulingGrant(other.getName())
    {
        operator=(other);
    }

    LteSchedulingGrantMode4& operator=(const LteSchedulingGrantMode4& other)
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
        LteSchedulingGrant::operator=(other);
        return *this;
    }

    virtual LteSchedulingGrantMode4 *dup() const
    {
        return new LteSchedulingGrantMode4(*this);
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

    void setPriority(unsigned int p)
    {
        priority_ = p;
    }
    unsigned int getPriority()
    {
        return priority_;
    }
    void setResourceRes(unsigned int r)
    {
        resourceRes_ = r;
    }
    unsigned int getFreqResource()
    {
        return freqResource_;
    }
    void setFreqResource(unsigned int f)
    {
        freqResource_ = f;
    }
    unsigned int getFreqResource()
    {
        return freqResource_;
    }
    void setTimeGap(unsigned int t)
    {
        timeGap_ = t;
    }
    unsigned int getTimeGap()
    {
        return timeGap_;
    }
    void setMcs(unsigned int m)
    {
        mcs_ = m;
    }
    unsigned int getMcs()
    {
        return mcs_;
    }
    void setRetransIndex(unsigned int r)
    {
        retransIndex_ = r;
    }
    unsigned int getRetransIndex()
    {
        return retransIndex_;
    }
};
