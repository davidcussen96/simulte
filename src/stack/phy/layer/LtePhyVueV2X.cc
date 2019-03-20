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
#include <ctime>

Define_Module(LtePhyVueV2X);

LtePhyVueV2X::LtePhyVueV2X()
{

    handoverStarter_ = NULL;
    handoverTrigger_ = NULL;

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
        srand(time(NULL));
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
        if (current == 0)
        {
            current = 999;
        } else
        {
            current -= 1;
        }

        if (pointerToEnd == 0)
        {
            pointerToEnd = 999;
        } else
        {
            pointerToEnd -= 1;
        }
        //std::vector<Subchannel*> subchannels(numSubchannels, new Subchannel());
        std::vector<Subchannel*> subchannels;
        for (int i = 0; i < numSubchannels; i++)
        {
            Subchannel* temp = new Subchannel();
            if (notSensedFlag) temp->setNotSensed(true);
            subchannels.push_back(temp);
        }
        sensingWindow[pointerToEnd] = subchannels;       // TODO Is it not set pointerToEnd to empty subchannels. 1000ms ago becomes 1ms ago
        // Reset notSensedFlag
        notSensedFlag = false;
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
        
        //int counter = 0, subchannelIndex;
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
    int pRsvpTxPrime = 0, prioRx = 0, k = 1, lSubch = 1, pRsvpRx, pStep = 100, thresAddition = 0,
    numSubch = numSubchannels, numCsrs, subchIndex, q = 1, msAgo = 1000, thIndex, riv, Th;
    bool wrapped;   // notSensedChecked = false,
    pRsvpTxPrime = pStep * pRsvpTx / 100;

    std::vector<std::vector<Subchannel*>> Sa;

    // Initialize set Sa.
    for (int i = 0; i < 100; i++)
    {
        std::vector<Subchannel*> temp;
        for (int j = 0; j < numSubchannels; j++)
        {
            Subchannel* s = new Subchannel();
            temp.push_back(s);
        }
        Sa.push_back(temp);
    }

    do
    {
        // Initialize set Sa. Its necessary to reinitialize Sa at every iteration.
        for (int i = 0; i < 100; i++)
        {
            std::vector<Subchannel*> temp;
            for (int j = 0; j < numSubchannels; j++)
            {
                Subchannel* s = new Subchannel();
                temp.push_back(s);
            }
            Sa.push_back(temp);
        }
        numCsrs = 400;
        wrapped = false;
        /*
         * Iterate through sensing window starting at pointerToEnd and finishing at current. (reverse)
         * Special Condition: C=0, E=999.
         *
         */

        for (int frame = pointerToEnd; frame != current || !wrapped; frame--)
        {
            subchIndex = 0;
            for (int channel = 0; channel < numSubchannels; channel++)
            {
                // Is the subchannel not sensed? If it is
                if (sensingWindow[frame][channel]->getNotSensed() == 1)
                {
                    // Condition to skip over not sensed subchannels in same subframe. If the first one is not sensed the rest are not sensed also.
                    /*
                    if (notSensedChecked)
                    {
                        numCsrs -= numNotSensed;
                    } else {*/
                    // Determine overlapping subframes which the current notSensed subframe.
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
                                j = cResel, y = 100, channel = numSubchannels;  // BREAK
                                numCsrs -= numSubchannels;      // TODO ?? Duplicated

                            }
                        }
                        //notSensedChecked = true;
                    }
                } else if (sensingWindow[frame][channel]->getIsFree() == 0)       // Subchannel is not free.
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

                    if (thIndex == 66 || sensingWindow[frame][channel]->isRsrpGreaterThan(Th))
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
                    } // else isRsrpGreaterThan sets rsrpLessThan to true
                }
                subchIndex++;
            }
            msAgo--;
            if (frame == 0)
            {
                wrapped = true;
                // Special condition: If current=0 and pointerToEnd=999, then we dont want to wraparound and reset frame to 999.
                if (current != 0) frame = 999;
                else frame = 1;
            }
        }
        thresAddition += 3;
    } while (numCsrs < 80);       // 0.2 * 400
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
            if (subchannel->getNotSensed() == 1)
            {
                continue;
            } else if (subchannel->getIsFree() == 1)
            {
                if (subchannel->getRsrp() == 1) // Subchannel has an rsrp and rssi value and is still free.
                {
                    rssi = subchannel->getRssi();
                    rssiValues.push_back(make_tuple(rssi, x, y));
                } else {
                    listOfCsrs.push_back(subchannel);   // Subchannel is free, no airframe received.
                }
            }/* else
            {
                rssi = subchannel->getRssi();
                rssiValues.push_back(make_tuple(rssi, x, y));
            }*/
        }
    }

    sort(rssiValues.begin(), rssiValues.end());

    tuple<int,int,int> currentRssi;
    int size = listOfCsrs.size();
    // While the number of CSRs available to send to the MAC layer < 80 continually add
    // CSRs with the lowest RSSI values.
    bool b;
    if (rssiValues.size() > 0) b = true;
    else b = false;     // There are no CSRs in rssiValues.
    while (size < 100 && b)
    {
        if (rssiValues.size() == 0) break;  // There are no CSRs in rssiValues.
        currentRssi = rssiValues.back();
        rssiValues.pop_back();
        listOfCsrs.push_back(Sa[get<1>(currentRssi)][get<2>(currentRssi)]);
        size = listOfCsrs.size();
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


RbMap LtePhyVueV2X::getRbMap(unsigned int sub, bool isSci)
{
    // subchannel : {0,3}
    // 1 + 10*subchannel & 2 + 10*subchannel + 3->10
    // 11&12 + 13->20
    // 21&22 + 23->30
    // 31&32 + 32->40

    //std::map<Remote, std::map<Band, unsigned int>> grantedBlocks;
    RbMap grantedBlocks;
    if (isSci)  // 2 Rb's
    {
        std::map<Band, unsigned int> tempSci;
        for (unsigned short i = 1; i < 3; i++)
        {
            Band band = i+10*sub;
            tempSci[band] = 1;
            //grantedBlocks[MACRO][i+10*sub] = 1;
        }
        //grantedBlocks[0] = temp;
        grantedBlocks.emplace(MACRO, tempSci);
    } else // 8 Rb's
    {
        std::map<Band, unsigned int> temp;
        for (unsigned short j = 3; j < 11; j++)
        {
            Band band = j+10*sub;
            temp[band] = 1;
            //grantedBlocks[MACRO][j+10*sub] = 1;
        }
        //grantedBlocks[0] = temp;
        grantedBlocks.emplace(MACRO, temp);
    }

    return grantedBlocks;
}

void LtePhyVueV2X::handleUpperMessage(cMessage* msg)
{
    UserControlInfo* lteInfo = check_and_cast<UserControlInfo*>(msg->removeControlInfo());      // This needs to contain RbMap

    // Received a grant so send sci and data packet (if received).
    // TODO We need to update the sensing window with the CSR we've chosen.
    if (lteInfo->getFrameType() == GRANTPKT)
    {

        LteSchedulingGrantMode4* grant = check_and_cast<LteSchedulingGrantMode4*>(msg);
        // Update sensing window. Set a flag
        notSensedFlag = true;
        // Received a grant so we create an SCI.
        Sci* sci = new Sci(grant->getPriority(), grant->getResourceReservation());
        // Extract csr from grant.
        // Subchannel* csr = grant->getCsr();      // TODO Should the lteInfo contain on what subchannel to transmit our CSR.
        LteAirFrame* sciFrame = new LteAirFrame("SCI");
        sciFrame->encapsulate(sci);
        sciFrame->setSchedulingPriority(airFramePriority_);
        sciFrame->setDuration(TTI);
        // set current position
        lteInfo->setCoord(getRadioPosition());
        int s = grant->getCsr()->getSubchannel();
        RbMap rbmap = getRbMap(s-1, true);
        lteInfo->setGrantedBlocks(rbmap);
        lteInfo->setTxPower(txPower_);
        lteInfo->setD2dTxPower(d2dTxPower_);
        lteInfo->setIsBroadcast(true);
        lteInfo->setTxNumber(1);
        lteInfo->setUserTxParams(grant->getUserTxParams()->dup());
        //lteInfo->setSourceId(getMacNodeId());
        //lteInfo->setDestId(getMacCellId());       //?? Is this the destination ??
        lteInfo->setDirection(D2D_MULTI);
        lteInfo->setFrameType(UNKNOWN_TYPE);
        sciFrame->setControlInfo(lteInfo);
        
        sendBroadcast(sciFrame);
        if (strcmp(dataFrame->getName(), "empty frame") != 0)
        {
            lteInfoData->setGrantedBlocks(getRbMap(grant->getCsr()->getSubchannel()-1, false));
            lteInfoData->setUserTxParams(grant->getUserTxParams()->dup());
            dataFrame->setControlInfo(lteInfoData);
            sendBroadcast(dataFrame);
        }
        dataFrame = new LteAirFrame("empty frame");

    } else if (lteInfo->getFrameType() == CSRREQUEST)
    {
        CsrRequest* csrRequest = check_and_cast<CsrRequest*>(msg);
        chooseCsr(csrRequest->getPrioTx(), csrRequest->getPRsvpTx(), csrRequest->getCResel());
    } else if (lteInfo->getFrameType() == DATAPKT and strcmp(dataFrame->getName(), "empty frame") == 0)
    {
        // Message received is a data packet. Store it and send it + SCI when the next grant is received.
        // If there was no data packet received when a grant is received you still send an SCI.
        // if the data packet is null then just an SCI is sent.
        // create LteAirFrame and encapsulate the received packet

        dataFrame->encapsulate(check_and_cast<cPacket*>(msg));
        lteInfoData = lteInfo;
        lteInfoData->setCoord(getRadioPosition());
        lteInfoData->setTxPower(txPower_);
        lteInfoData->setD2dTxPower(d2dTxPower_);
        //lteInfoData->setSourceId(getMacNodeId());
        lteInfoData->setIsBroadcast(true);
        //lteInfoData->setDestId(getMacCellId());       //?? Is this the destination ??
        lteInfoData->setDirection(D2D_MULTI);
        lteInfoData->setTxNumber(1);
        lteInfoData->setFrameType(UNKNOWN_TYPE);
        // initialize frame fields
        dataFrame->setName("data frame");
        dataFrame->setSchedulingPriority(airFramePriority_);
        dataFrame->setDuration(TTI);
        // set current position
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
    MacNodeId enbId = binder_->getNextHop(newInfo->getSourceId());   // eNodeB id??
    //MacNodeId enbId = 1;

    if (strcmp(par("d2dMulticastCaptureEffectFactor"), "RSRP") == 0
            && (strcmp(newFrame->getName(), "data frame") == 0 || strcmp(newFrame->getName(), "SCI") == 0))
    {
        rssiVector = check_and_cast<LteRealisticChannelModel*>(channelModel_)->getSINR_D2D(newFrame, newInfo, nodeId_, myCoord, enbId);
        rsrpVector = check_and_cast<LteRealisticChannelModel*>(channelModel_)->getRSRP_D2D(newFrame, newInfo, nodeId_, myCoord);
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
                    if (band == 1 or band == 3) subchannelIndex = 0;
                    else if (band == 11 or band == 13) subchannelIndex = 1;
                    else if (band == 21 or band == 23) subchannelIndex = 2;
                    else if (band == 31 or band == 33) subchannelIndex = 3;
                    else if (band == 41 or band == 43) subchannelIndex = 4;
                }

                rsrp += rsrpVector.at(band);
                rssi += rssiVector.at(band);
            }
        }
        cPacket* pkt = newFrame->getEncapsulatedPacket();
        if (dynamic_cast<Sci*>(pkt) == nullptr) {

            Subchannel* subchannel = new Subchannel(rsrp, rssi, sourceId, subchannelIndex);
            tbList.push_back(subchannel);
        } else {
            Sci* sci = check_and_cast<Sci*>(pkt);
            Subchannel* subchannel = new Subchannel(sci, rsrp, rssi, sourceId, subchannelIndex);
            sciList.push_back(subchannel);
        }
        v2xReceivedFrames_.push_back(newFrame);
    }
    
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
            result=check_and_cast<LteRealisticChannelModel*>(channelModel_)->errorDas(frame,lteInfo);
        }
        else
        {
            unsigned int mcs = 0;
            result = check_and_cast<LteRealisticChannelModel*>(channelModel_)->error(frame,lteInfo, mcs);
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
            result=check_and_cast<LteRealisticChannelModel*>(channelModel_)->errorDas(frame,lteInfo);
        }
        else
        {
            result = check_and_cast<LteRealisticChannelModel*>(channelModel_)->error(frame,lteInfo, mcs);
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
