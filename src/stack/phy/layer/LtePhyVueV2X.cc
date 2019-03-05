//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include <assert.h>
#include "stack/phy/layer/LtePhyUeD2D.h"
#include "stack/phy/layer/LtePhyVueV2X.h"
#include "stack/phy/packet/LteFeedbackPkt.h"
#include "stack/d2dModeSelection/D2DModeSelectionBase.h"
#include "stack/phy/ChannelModel/LteRealisticChannelModel.h"
#include "stack/subchannel/Subchannel.h"
#include "stack/mac/packet/LteSchedulingGrant.h"
#include "stack/mac/packet/CsrRequest_m.h"
#include "stack/phy/packet/CsrMessage.h"
#include "stack/phy/packet/LteAirFrame.h"

Define_Module(LtePhyVueV2X);

LtePhyVueV2X::LtePhyVueV2X()
{
    /*
    handoverStarter_ = NULL;
    handoverTrigger_ = NULL;
    */
}

LtePhyVueV2X::~LtePhyVueV2X()
{
}

void LtePhyVueV2X::initialize(int stage)
{
    LtePhyUeD2D::initialize(stage);
    if (stage == 0)
    {
        // Initialize sensing window.
        for (int i = 0; i < 1000; i++)
        {
            std::vector<Subchannel*> temp;
            for (int j = 0; j < numSubchannels; j++)
            {
                Subchannel* s = new Subchannel();
                temp.push_back(s);
            }
            sensingWindow.push_back(temp);
        }
        averageCqiD2D_ = registerSignal("averageCqiD2D");
        d2dTxPower_ = par("d2dTxPower");
        d2dMulticastEnableCaptureEffect_ = par("d2dMulticastCaptureEffect");
        v2xDecodingTimer_ = NULL;
        updateSensingWindow_ = NULL;
        updatePointers();
    }

}

void LtePhyVueV2X::handleSelfMessage(cMessage *msg)
{
    if (msg->isName("v2xDecodingTimer"))
    {
        while (!v2xReceivedFrames_.empty())
        {
            LteAirFrame* frame = extractAirFrame();
            UserControlInfo* lteInfo = check_and_cast<UserControlInfo*>(frame->removeControlInfo());

            // decode the selected frame
            decodeAirFrame(frame, lteInfo);
        }
        // Clear MCS map.
        mcsMap.clear();
        delete msg;
        v2xDecodingTimer_ = NULL;
    } else if (msg->isName("updatePointers"))
    {
        current++;

        if (current == 1000) {current = 0;}
        pointerToEnd = current + 1;
        if (pointerToEnd == 1000) {pointerToEnd = 0;}
        std::vector<Subchannel*> subchannels(numSubchannels, new Subchannel());
        sensingWindow[current] = subchannels;
        delete msg;
        updatePointers();
    } else if (msg->isName("updateSensingWindow"))
    {
        // TODO Needs revision. currTime etc.
        // Iterate through subchannels created from received AirFrames and update the sensing window.
        bool noMatch;

        for (int i = 0; i < sciList.size(); i++)
        {
            noMatch = true;
            for (int j = 0; j < tbList.size(); j++)
            {
                if (sciList[i]->getSourceId() == tbList[j]->getSourceId())
                {
                    // Add SCI and TB to create new Subchannel.

                    Subchannel* newSubchannel = sciList[i]->add(tbList[j]);
                    subchannelList.push_back(newSubchannel);
                    noMatch = false;
                }
            }
            if (noMatch) subchannelList.push_back(sciList[i]);
        }

        sciList.clear();
        tbList.clear();
        
        int counter = 0, subchannelIndex;
        for (int k = 0; k < subchannelList.size(); k++)
        {
            int subch = subchannelList[k]->getSubchannel();
            sensingWindow[current][subch] = subchannelList[k];
        }

        delete msg;
        updateSensingWindow_ = NULL;
    }
    else
        LtePhyUe::handleSelfMessage(msg);
}

void LtePhyVueV2X::chooseCsr(int prioTx, int pRsvpTx, int cResel)
{
    int pRsvpTxPrime, prioRx, k, lSubch, pRsvpRx, pStep, thresAddition,
    numSubch, numCsrs, subchIndex, numNotSensed = 0, q, msAgo, thIndex, riv, Th;
    bool notSensedChecked = false, wrapped;

    std::vector<std::vector<Subchannel*>> Sa(100, std::vector<Subchannel*>(numSubchannels, new Subchannel()));

    do
    {
        numCsrs = 300;
        pStep = 100;

        pRsvpTxPrime = pStep * pRsvpTx / 100;
        k = 1;
        q = 1;

        lSubch = 1;
        numSubch = numSubchannels;

        thresAddition = 0;
        msAgo = 1000;
        wrapped = false;

        for (int frame = pointerToEnd; frame != pointerToEnd || !wrapped; ++frame)
        {
            if (frame == 999)
            {
                frame = 0;
                wrapped = true;
            }

            subchIndex = 0;
            for (int channel = 0; channel < numSubchannels; channel++)
            {
                if (sensingWindow[frame][channel]->getNotSensed())
                {
                    if (notSensedChecked)
                    {
                        numCsrs -= numNotSensed;
                    } else {
                        for (unsigned int y = 0; y < 100; y++)
                        {
                            for (unsigned int j = 0; j < cResel; j++)
                            {
                                if (y + (j*pRsvpTxPrime) == -(msAgo) + pStep*k*q)
                                {
                                    for (int z = 0; z < numSubchannels; z++)
                                    {
                                        Sa[y][z]->setNotSensed(true);
                                    }
                                    numCsrs -= numSubchannels;
                                    numNotSensed += numSubchannels;

                                    j = cResel, y = 100, channel = numSubchannels;
                                }
                            }
                        }
                        notSensedChecked = true;
                    }
                } else if (!sensingWindow[frame][channel]->getIsFree())
                {
                    prioRx = sensingWindow[frame][channel]->getSci()->getPriority();
                    pRsvpRx = sensingWindow[frame][channel]->getSci()->getResourceRes();
                    if (msAgo - (pRsvpRx*100) < 0 || pRsvpRx == 0)
                    {
                        subchIndex++;
                        continue;
                    }
                    thIndex = ((prioTx/100)*8) + prioRx + 1;
                    //infinity: if 0 always less than, if 66 always greater than.
                    if (thIndex != 0 && thIndex != 66) Th = (-128 + (thIndex-1)*2) + thresAddition;

                    if (thIndex == 66 || sensingWindow[frame][channel]->isRsrpLessThan(Th))
                    {
                        for (unsigned int y = 0; y < 100; y++)
                        {
                            //riv
                            riv = calculateRiv(lSubch, numSubch, subchIndex);
                            for (unsigned int j = 0; j < cResel; j++)
                            {
                                if (-msAgo + q * pStep * pRsvpRx == y + j * pRsvpTxPrime)
                                {
                                    Sa[y][subchIndex]->setIsFree(false);
                                    numCsrs -= numSubchannels;
                                    break;
                                }
                            }
                        }
                    }
                }
                subchIndex++;
            }
            msAgo--;
        }
        thresAddition += 3;
    } while (numCsrs < 60);       // 0.2 * 300

    std::vector< tuple<int,int,int>> rssiValues;
    std::vector<Subchannel*> listOfCsrs;
    Subchannel* subchannel;
    int rssi;

    for (unsigned int x = 0; x < 100; x++)
    {
        for (unsigned int y = 0; y < numSubchannels; y++)
        {
            subchannel = Sa[x][y];
            subchannel->setSubframe(x+1);
            subchannel->setSubchannel(y+1);
            if (subchannel->getNotSensed())
            {
                continue;
            } else if (subchannel->getIsFree())
            {
                listOfCsrs.push_back(subchannel);
            } else
            {
                rssi = subchannel->getRssi();
                rssiValues.push_back(make_tuple(rssi, x, y));
            }
        }
    }

    sort(rssiValues.begin(), rssiValues.end());

    tuple<int,int,int> currentRssi;
    while (listOfCsrs.size() < 60)
    {
        currentRssi = rssiValues.back();
        rssiValues.pop_back();
        listOfCsrs.push_back(Sa[get<1>(currentRssi)][get<2>(currentRssi)]);
    }

    // Send list of Csrs to MAC layer.
    CsrMessage* csrMsg = new CsrMessage();
    csrMsg->setCsrList(listOfCsrs);
    send(csrMsg,upperGateOut_);
}

int LtePhyVueV2X::calculateRiv(int lSubch, int numSubch, int startSubchIndex)
{
    int riv;
    if (lSubch-1 <= numSubch / 2)
    {
        riv = numSubch*(lSubch-1)+startSubchIndex;
    } else
    {
        riv = numSubch*(numSubch-lSubch+1) + numSubch - 1 - startSubchIndex;
    }
    return riv;
}

void LtePhyVueV2X::handleAirFrame(cMessage* msg)
{
    UserControlInfo* lteInfo = check_and_cast<UserControlInfo*>(msg->removeControlInfo());

    //connectedNodeId_ = masterId_;
    LteAirFrame* frame = check_and_cast<LteAirFrame*>(msg);
    EV << "LtePhyUeD2D: received new LteAirFrame with ID " << frame->getId() << " from channel" << endl;

    lteInfo->setDestId(nodeId_);

    // send H-ARQ feedback up
    if (lteInfo->getFrameType() == HARQPKT || lteInfo->getFrameType() == GRANTPKT || lteInfo->getFrameType() == RACPKT || lteInfo->getFrameType() == D2DMODESWITCHPKT)
    {
        handleControlMsg(frame, lteInfo);
        return;
    }

    // this is a DATA packet.

    // if the packet is a D2D multicast one, store it and decode it at the end of the TTI.
    // if not already started, auto-send a message to signal the presence of data to be decoded.

    if (v2xDecodingTimer_ == NULL)
    {
        v2xDecodingTimer_ = new cMessage("v2xDecodingTimer");
        v2xDecodingTimer_->setSchedulingPriority(10);          // last thing to be performed in this TTI
        scheduleAt(NOW, v2xDecodingTimer_);
    }

    if (updateSensingWindow_ == NULL)
    {
        updateSensingWindow_ = new cMessage("updateSensingWindow");
        updateSensingWindow_->setSchedulingPriority(10);
        scheduleAt(NOW, updateSensingWindow_);
    }

    // store frame, together with related control info
    frame->setControlInfo(lteInfo);
    storeAirFrame(frame);            // implements the capture effect

    return;                          // exit the function, decoding will be done later
}

void LtePhyVueV2X::handleUpperMessage(cMessage* msg)
{
    UserControlInfo* lteInfo = check_and_cast<UserControlInfo*>(msg->removeControlInfo());
    CsrRequest* csrRequest = check_and_cast<CsrRequest*>(msg);  // TODO Should always check_and_cast because csrRequest inherits from cMessage.


    // Received a grant so send sci and data packet
    if (strcmp(msg->getName(), "scheduling grant") == 0)
    {
        LteSchedulingGrantMode4* grant = check_and_cast<LteSchedulingGrantMode4*>(msg);
        // Received a grant so we create an SCI.
        Sci* sci = new Sci(grant->getPriority(), grant->getResourceReservation());
        // Extract csr from grant.
        // Subchannel* csr = grant->getCsr();      // TODO Should the lteInfo contain on what subchannel to transmit our CSR.
        LteAirFrame* sciFrame = new LteAirFrame("SCI");
        sciFrame->encapsulate(check_and_cast<cPacket*>(sci));
        sciFrame->setSchedulingPriority(airFramePriority_);
        sciFrame->setDuration(TTI);
        // set current position
        lteInfo->setCoord(getRadioPosition());

        lteInfo->setTxPower(txPower_);
        lteInfo->setD2dTxPower(d2dTxPower_);
        sciFrame->setControlInfo(lteInfo);
        
        sendBroadcast(sciFrame);
        if (strcmp(dataFrame->getName(), "empty frame") != 0)
        {
            sendBroadcast(dataFrame);
        }
        dataFrame = new LteAirFrame("empty frame");

    } else if (lteInfo->getFrameType() == DATAPKT)
    {
        // Message received is a data packet. Store it and send it + SCI when the next grant is received.
        // If there was no data packet received when a grant is received you still send an SCI.
        // if the data packet is null then just an SCI is sent.
        // create LteAirFrame and encapsulate the received packet

        dataFrame->encapsulate(check_and_cast<cPacket*>(msg));

        // initialize frame fields
        dataFrame->setName("data frame");
        dataFrame->setSchedulingPriority(airFramePriority_);
        dataFrame->setDuration(TTI);
        // set current position
        lteInfo->setCoord(getRadioPosition());

        lteInfo->setTxPower(txPower_);
        lteInfo->setD2dTxPower(d2dTxPower_);
        // TODO Does this contain whats RBs to use?
        dataFrame->setControlInfo(lteInfo);

    } else if (strcmp(csrRequest->getName(), "csrRequest") == 0)
    {
        //timeRequested = NOW;
        chooseCsr(csrRequest->getPrioTx(), csrRequest->getPRsvpTx(), csrRequest->getCResel());
        return;
    }

}

void LtePhyVueV2X::storeAirFrame(LteAirFrame* newFrame)
{
    // TODO Determine the subchannel here. How?
    UserControlInfo* newInfo = check_and_cast<UserControlInfo*>(newFrame->getControlInfo());
    uint16_t sourceId = newInfo->getSourceId();
    Coord myCoord = getCoord();
    std::vector<double> rsrpVector;
    std::vector<double> rssiVector;
    //MacNodeId enbId = binder_->getNextHop(newInfo->getSourceId());   // eNodeB id??
    MacNodeId enbId = 1;

    rsrpVector = channelModel_->getRSRP_D2D(newFrame, newInfo, nodeId_, myCoord);
    rssiVector = channelModel_->getSINR_D2D(newFrame, newInfo, nodeId_, myCoord, enbId);
    int rsrp = 0;
    int rssi = 0;

    int subchannelIndex = -1;   // Not assigned

    // get the average RSRP on the RBs allocated for the transmission
    RbMap rbmap = newInfo->getGrantedBlocks();
    RbMap::iterator it;
    std::map<Band, unsigned int>::iterator jt;
    //for each Remote unit used to transmit the packet
    for (it = rbmap.begin(); it != rbmap.end(); ++it)
    {
        //for each logical band used to transmit the packet
        for (jt = it->second.begin(); jt != it->second.end(); ++jt)
        {
            Band band = jt->first;
            if (jt->second == 0) // this Rb is not allocated
                continue;

            // TODO Will need to calculate this.
            if (subchannelIndex == -1)
            {
                if (jt->second == 1 or jt->second == 3) subchannelIndex = 0;
                else if (jt->second == 13 or jt->second == 15) subchannelIndex = 1;
                else if (jt->second == 25 or jt->second == 27) subchannelIndex = 2;
                else subchannelIndex = 3;
            }

            rsrp += rsrpVector.at(band);
            rssi += rssiVector.at(band);
        }
    }
    
    Sci* sci = check_and_cast<Sci*>(newFrame);
    if (sci != nullptr) {
        Subchannel* subchannel = new Subchannel(sci, rsrp, rssi, sourceId, subchannelIndex);
        sciList.push_back(subchannel);
    } else {
        Subchannel* subchannel = new Subchannel(rsrp, rssi, sourceId, subchannelIndex);
        tbList.push_back(subchannel);
    }
    v2xReceivedFrames_.push_back(newFrame);
    delete newFrame;
}

void LtePhyVueV2X::updatePointers()
{
    updatePointers_ = new cMessage("updatePointers");
    updatePointers_->setSchedulingPriority(10);
    scheduleAt(NOW+TTI, updatePointers_);
}

LteAirFrame* LtePhyVueV2X::extractAirFrame()
{
    // implements the capture effect
    // the vector is storing the frame received from the strongest/nearest transmitter

    return v2xReceivedFrames_.front();
}

void LtePhyVueV2X::decodeAirFrame(LteAirFrame* frame, UserControlInfo* lteInfo)
{
    EV << NOW << " LtePhyUeD2D::decodeAirFrame - Start decoding..." << endl;
    // TODO Is it a SCI or TB we are decoding?
    cPacket* pkt = frame->decapsulate();
    Sci* sci = check_and_cast<Sci*>(pkt);   // Extract SCI from frame.
    if (sci)
    {
        // if SCI decode using MCS 0 and store its MCS value
        unsigned int sciMcs = sci->getMcs();
        mcsMap.insert(std::make_pair(lteInfo->getSourceId(), sciMcs));

        // apply decider to received packet
        bool result = true;

        RemoteSet r = lteInfo->getUserTxParams()->readAntennaSet();
        if (r.size() > 1)
        {
            // DAS
            for (RemoteSet::iterator it = r.begin(); it != r.end(); it++)
            {
                EV << "LtePhyUeD2D::decodeAirFrame: Receiving Packet from antenna " << (*it) << "\n";

                RemoteUnitPhyData data;
                data.txPower=lteInfo->getTxPower();
                data.m=getRadioPosition();
                frame->addRemoteUnitPhyDataVector(data);
            }
            // apply analog models For DAS
            result=channelModel_->errorDas(frame,lteInfo);
        }
        else
        {
            unsigned int mcs = 0;
            result = channelModel_->error(frame,lteInfo, mcs);
        }

        // update statistics
        if (result)
            numAirFrameWithSCIsReceived_++;
        else
            numAirFrameWithSCIsNotReceived_++;

        EV << "Handled LteAirframe (SCI) with ID " << frame->getId() << " with result "
           << ( result ? "RECEIVED" : "NOT RECEIVED" ) << endl;

        if (getEnvir()->isGUI())
            updateDisplayString();


    } else {
        // frame is a TB
        unsigned int mcs = mcsMap[lteInfo->getSourceId()];
        // Use this mcs to decode the TB.

        // apply decider to received packet
        bool result = true;

        RemoteSet r = lteInfo->getUserTxParams()->readAntennaSet();
        if (r.size() > 1)
        {
            // DAS
            for (RemoteSet::iterator it = r.begin(); it != r.end(); it++)
            {
                EV << "LtePhyUeD2D::decodeAirFrame: Receiving Packet from antenna " << (*it) << "\n";

                RemoteUnitPhyData data;
                data.txPower=lteInfo->getTxPower();
                data.m=getRadioPosition();
                frame->addRemoteUnitPhyDataVector(data);
            }
            // apply analog models For DAS
            result=channelModel_->errorDas(frame,lteInfo);
        }
        else
        {
            result = channelModel_->error(frame,lteInfo, mcs);
        }

        // update statistics
        if (result)
            numAirFrameWithTBsReceived_++;
        else
            numAirFrameWithTBsNotReceived_++;

        EV << "Handled LteAirframe (TB) with ID " << frame->getId() << " with result "
           << ( result ? "RECEIVED" : "NOT RECEIVED" ) << endl;

        //cPacket* pkt = frame->decapsulate();

        // attach the decider result to the packet as control info
        lteInfo->setDeciderResult(result);
        pkt->setControlInfo(lteInfo);

        // send decapsulated message along with result control info to upperGateOut_
        send(pkt, upperGateOut_);

        if (getEnvir()->isGUI())
            updateDisplayString();
    }

    /*
    // apply decider to received packet
    bool result = true;

    RemoteSet r = lteInfo->getUserTxParams()->readAntennaSet();
    if (r.size() > 1)
    {
        // DAS
        for (RemoteSet::iterator it = r.begin(); it != r.end(); it++)
        {
            EV << "LtePhyUeD2D::decodeAirFrame: Receiving Packet from antenna " << (*it) << "\n";


             //* On UE set the sender position
             //* and tx power to the sender das antenna
             //

//            cc->updateHostPosition(myHostRef,das_->getAntennaCoord(*it));
            // Set position of sender
//            Move m;
//            m.setStart(das_->getAntennaCoord(*it));
            RemoteUnitPhyData data;
            data.txPower=lteInfo->getTxPower();
            data.m=getRadioPosition();
            frame->addRemoteUnitPhyDataVector(data);
        }
        // apply analog models For DAS
        result=channelModel_->errorDas(frame,lteInfo);
    }
    else
    {
        //RELAY and NORMAL
        if (lteInfo->getDirection() == D2D_MULTI)
            result = channelModel_->error_D2D(frame,lteInfo,bestRsrpVector_);
        else
            result = channelModel_->error(frame,lteInfo);
    }

    // update statistics
    if (result)
        numAirFrameReceived_++;
    else
        numAirFrameNotReceived_++;

    EV << "Handled LteAirframe with ID " << frame->getId() << " with result "
       << ( result ? "RECEIVED" : "NOT RECEIVED" ) << endl;

    cPacket* pkt = frame->decapsulate();

    // attach the decider result to the packet as control info
    lteInfo->setDeciderResult(result);
    pkt->setControlInfo(lteInfo);

    // send decapsulated message along with result control info to upperGateOut_
    send(pkt, upperGateOut_);

    if (getEnvir()->isGUI())
        updateDisplayString();
    */
}


void LtePhyVueV2X::sendFeedback(LteFeedbackDoubleVector fbDl, LteFeedbackDoubleVector fbUl, FeedbackRequest req)
{
    Enter_Method("SendFeedback");
    EV << "LtePhyUeD2D: feedback from Feedback Generator" << endl;

    //Create a feedback packet
    LteFeedbackPkt* fbPkt = new LteFeedbackPkt();
    //Set the feedback
    fbPkt->setLteFeedbackDoubleVectorDl(fbDl);
    fbPkt->setLteFeedbackDoubleVectorDl(fbUl);
    fbPkt->setSourceNodeId(nodeId_);
    UserControlInfo* uinfo = new UserControlInfo();
    uinfo->setSourceId(nodeId_);
    uinfo->setDestId(masterId_);
    uinfo->setFrameType(FEEDBACKPKT);
    uinfo->setIsCorruptible(false);
    // create LteAirFrame and encapsulate a feedback packet
    LteAirFrame* frame = new LteAirFrame("feedback_pkt");
    frame->encapsulate(check_and_cast<cPacket*>(fbPkt));
    uinfo->feedbackReq = req;
    uinfo->setDirection(UL);
    simtime_t signalLength = TTI;
    uinfo->setTxPower(txPower_);
    uinfo->setD2dTxPower(d2dTxPower_);
    // initialize frame fields

    frame->setSchedulingPriority(airFramePriority_);
    frame->setDuration(signalLength);

    uinfo->setCoord(getRadioPosition());

    frame->setControlInfo(uinfo);
    //TODO access speed data Update channel index
//    if (coherenceTime(move.getSpeed())<(NOW-lastFeedback_)){
//        deployer_->channelIncrease(nodeId_);
//        deployer_->lambdaIncrease(nodeId_,1);
//    }
    lastFeedback_ = NOW;
    EV << "LtePhy: " << nodeTypeToA(nodeType_) << " with id "
       << nodeId_ << " sending feedback to the air channel" << endl;
    sendUnicast(frame);
}

void LtePhyVueV2X::finish()
{
    if (getSimulation()->getSimulationStage() != CTX_FINISH)
    {
        // do this only at deletion of the module during the simulation

        // amc calls
        LteAmc *amc = getAmcModule(masterId_);
        if (amc != NULL)
            amc->detachUser(nodeId_, D2D);

        LtePhyUe::finish();
    }
}
