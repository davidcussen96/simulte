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
#include "stack/subchannel/Subchannel.h"
#include "stack/phy/layer/LtePhyBase.h"
#include "common/LteCommon.h"

class LtePhyVueV2X : public LtePhyUeD2D
{
  protected:

    // Number of subchannels per subframe -> 4 subchannels per subframe with 12 RBs per subchannel.
    int numSubchannels = 5;
    // D2D Tx Power
    double d2dTxPower_;
    std::vector<std::vector<Subchannel*>> sensingWindow;
    std::vector<Subchannel*> subchannelList;
    std::vector<LteAirFrame*> v2xReceivedFramesSCI_; // airframes received in the current TTI. Only one will be decoded
    std::vector<LteAirFrame*> v2xReceivedFramesTB_;
    cMessage* v2xDecodingTimer_;                  // timer for triggering decoding at the end of the TTI. Started
                                                  // when the first airframe is received
    cMessage* updateSensingWindow_;
    cMessage* updatePointers_;
    int pointerToEnd = 999;
    int current = 0;
    simtime_t timeRequested;
    LteAirFrame* dataFrame = new LteAirFrame("empty frame");
    UserControlInfo* lteInfoData;
    // Vectors containing sci's and tb's
    std::vector<Subchannel*> sciList;
    std::vector<Subchannel*> tbList;
    // SourceId : mcs
    std::map<uint16_t, unsigned int> mcsMap;
    // SourceId : SCI received correctly (true/false)
    std::map<uint16_t, bool> sciReceivedCorrectly;
    
    bool notSensedFlag = false;
    //LteRealisticChannelModel* channelModel_;
    //Statistics
    simsignal_t numAirFramesWithSCIsReceivedSignal_;
    simsignal_t numAirFramesWithSCIsNotReceivedSignal_;
    simsignal_t numAirFramesWithTBsReceivedSignal_;
    simsignal_t numAirFramesWithTBsNotReceivedSignal_;
    simsignal_t distance_;

    double dist = 0;
    unsigned int numAirFramesWithSCIsReceived_ = 0;
    unsigned int numAirFramesWithSCIsNotReceived_ = 0;
    unsigned int numAirFramesWithTBsReceived_ = 0;
    unsigned int numAirFramesWithTBsNotReceived_ = 0;

    void storeAirFrame(LteAirFrame* newFrame);
    LteAirFrame* extractAirFrame(bool isSci);
    void decodeAirFrame(LteAirFrame* frame, UserControlInfo* lteInfo);
    // ---------------------------------------------------------------- //

    virtual void initialize(int stage);
    virtual void finish();
    virtual void handleAirFrame(cMessage* msg);
    virtual void handleUpperMessage(cMessage* msg);
    virtual void handleSelfMessage(cMessage *msg);
    virtual void updatePointers();
    virtual int calculateRiv(int lSubch, int numSubch, int startSubchIndex);
    virtual void chooseCsr(int prioTx, int pRsvpTx, int cResel);
    virtual RbMap getRbMap(unsigned int sub, bool isSci);


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
