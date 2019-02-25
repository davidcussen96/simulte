//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _LTE_AIRPHYVUEV2X_H_
#define _LTE_AIRPHYVUEV2X_H_

#include "stack/phy/layer/LtePhyVueV2X.h"
#include "stack/subchannel/Subchannel/h"

class LtePhyVueV2X : public LtePhyUeD2D
{
  protected:

    // D2D Tx Power
    double d2dTxPower_;

    std::vector<std::vector<Subchannel*>> sensingWindow{1000, std::vector<Subchannel*>(3,1)};
    std::vector<Subchannel*> subchannelList;
    std::vector<LteAirFrame*> v2xReceivedFrames_; // airframes received in the current TTI. Only one will be decoded
    cMessage* v2xDecodingTimer_;                  // timer for triggering decoding at the end of the TTI. Started
                                                  // when the first airframe is received
    cMessage* updateSensingWindow_;
    cMessage* updatePointers_;
    int pointerToEnd = 999;
    int current = 0;
    simtime_t timeRequested;
    LteAirFrame* dataFrame = new LteAirFrame("empty frame");
    std::vector<Subchannel*> sciList;
    std::vector<Subchannel*> tbList;
    
    void storeAirFrame(LteAirFrame* newFrame);
    LteAirFrame* extractAirFrame();
    void decodeAirFrame(LteAirFrame* frame, UserControlInfo* lteInfo);
    // ---------------------------------------------------------------- //

    virtual void initialize(int stage);
    virtual void finish();
    virtual void handleAirFrame(cMessage* msg);
    virtual void handleUpperMessage(cMessage* msg);
    virtual void handleSelfMessage(cMessage *msg);
    virtual void updatePointers();
    virtual int calculateRiv(int lSubch, int numSubch, int startSubchIndex);
    virtual void chooseCsr(int t2, int prioTx, int pRsvpTx, int cResel);


  public:
    LtePhyVueV2X();
    virtual ~LtePhyVueV2X();

    virtual void sendFeedback(LteFeedbackDoubleVector fbDl, LteFeedbackDoubleVector fbUl, FeedbackRequest req);
    virtual double getTxPwr(Direction dir = UNKNOWN_DIRECTION)
    {
        if (dir == D2D)
            return d2dTxPower_;
        return txPower_;
    }
};

#endif  /* _LTE_AIRPHYVUEV2X_H_ */
