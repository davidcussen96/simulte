/*
 * LteMacVueV2X.h
 *
 *  Created on: Feb 18, 2019
 *      Author: djc10
 */

#ifndef STACK_MAC_LAYER_LTEMACVUEV2X_H_
#define STACK_MAC_LAYER_LTEMACVUEV2X_H_

#include "stack/mac/layer/LteMacUeRealistic.h"
#include "stack/mac/layer/LteMacUeRealisticD2D.h"
#include "stack/mac/layer/LteMacEnbRealisticD2D.h"
#include "stack/mac/buffer/harq_d2d/LteHarqBufferTxD2D.h"
#include "stack/subchannel/Subchannel.h"

class LteSchedulerUeUl;
class LteBinder;

class LteMacVueV2X : public LteMacUeRealisticD2D
{
    // reference to the eNB
    LteMacEnbRealisticD2D* enb_;

    // RAC Handling variables
    bool racD2DMulticastRequested_;
    // Multicast D2D BSR handling
    bool bsrD2DMulticastTriggered_;

    // if true, use the preconfigured TX params for transmission, else use that signaled by the eNB
    bool usePreconfiguredTxParams_;
    UserTxParams* preconfiguredTxParams_;
    UserTxParams* getPreconfiguredTxParams();  // build and return new user tx params

    Subchannel* currentCsr;
    bool csrReceived = false;
    bool dataPktReceived = false;
    int64_t pktSize;

  protected:

    /**
     * Reads MAC parameters for ue and performs initialization.
     */
    virtual void initialize(int stage);

    /**
     * Analyze gate of incoming packet
     * and call proper handler
     */
    virtual void handleMessage(cMessage *msg);

    /**
     * Main loop
     */
    virtual void handleSelfMessage();

    virtual void macHandleGrant();

    virtual void createIntermediateGrant(unsigned int expCounter, unsigned int period, Subchannel* subch, const RbMap& rbmap);

    virtual RbMap getFullRbMap(unsigned int subch);
    /*
     * Checks RAC status
     */
    virtual void checkRAC();

    /*
     * Receives and handles RAC responses
     */
    virtual void macHandleRac(cPacket* pkt);

    void macHandleD2DModeSwitch(cPacket* pkt);

    virtual LteMacPdu* makeBsr(int size);

    virtual void handleUpperMessage(cPacket* pkt);

    /**
     * macPduMake() creates MAC PDUs (one for each CID)
     * by extracting SDUs from Real Mac Buffers according
     * to the Schedule List.
     * It sends them to H-ARQ (at the moment lower layer)
     *
     * On UE it also adds a BSR control element to the MAC PDU
     * containing the size of its buffer (for that CID)
     */
    virtual void macPduMake();

    virtual Subchannel* chooseCsrAtRandom(std::vector<Subchannel*> csrList);

    virtual void sendCsrRequest();

  public:
    LteMacVueV2X();
    virtual ~LteMacVueV2X();

    virtual bool isD2DCapable()
    {
        return true;
    }

    virtual bool isMode4Capable()
    {
        return true;
    }

    virtual void triggerBsr(MacCid cid)
    {
        if (connDesc_[cid].getDirection() == D2D_MULTI)
            bsrD2DMulticastTriggered_ = true;
        else
            bsrTriggered_ = true;
    }
    virtual void doHandover(MacNodeId targetEnb);
};



#endif /* STACK_MAC_LAYER_LTEMACVUEV2X_H_ */
