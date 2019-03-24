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
        numAirFramesWithSCIsReceivedSignal_ = registerSignal("numAirFramesWithSCIsReceivedSignal");
        numAirFramesWithSCIsNotReceivedSignal_ = registerSignal("numAirFramesWithSCIsNotReceivedSignal");
        numAirFramesWithTBsReceivedSignal_ = registerSignal("numAirFramesWithTBsReceivedSignal");
        numAirFramesWithTBsNotReceivedSignal_ = registerSignal("numAirFramesWithTBsNotReceivedSignal");
        numSCIAndTBPairsSignal_ = registerSignal("numSCIAndTBPairsSignal");
        distance_ = registerSignal("distanceSignal");
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
        // Decode SCI's before TB's because we need the MCS from the SCI to decode the TB.
        while (!v2xReceivedFramesSCI_.empty())
        {
            LteAirFrame* frame = extractAirFrame(true);
            if (strcmp(frame->getName(), "SCI") == 0)
            {
                UserControlInfo* lteInfo = check_and_cast<UserControlInfo*>(frame->removeControlInfo());
                // decode the selected frame
                decodeAirFrame(frame, lteInfo);
            }
            delete frame;
        }
        while (!v2xReceivedFramesTB_.empty())
        {
            LteAirFrame* frame = extractAirFrame(false);

            UserControlInfo* lteInfo = check_and_cast<UserControlInfo*>(frame->removeControlInfo());
            // decode the selected frame
            decodeAirFrame(frame, lteInfo);

            delete frame;
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
        // Iterate through subchannels created from received AirFrames and update the sensing window.
        bool noMatch;

        // For each SCI find the matching TB. Add them together (RSRP, RSSI, RbMap, etc.)
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
                // Is the subchannel not sensed? If it is.
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
                    Sci* sci = sensingWindow[frame][channel]->getSci();
                    prioRx = sci->getPriority();
                    pRsvpRx = sensingWindow[frame][channel]->getSci()->getResourceRes();
                    //prioRx = 1;
                    //pRsvpRx = 100;
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
            }else
            {
                rssi = subchannel->getRssi();
                rssiValues.push_back(make_tuple(rssi, x, y));
            }
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
    LteAirFrame* frame = check_and_cast<LteAirFrame*>(msg);
    UserControlInfo* lteInfo = check_and_cast<UserControlInfo*>(msg->removeControlInfo());

    //connectedNodeId_ = masterId_;
    EV << "LtePhyUeD2D: received new LteAirFrame with ID " << frame->getId() << " from channel" << endl;

    lteInfo->setDestId(nodeId_);
    // Get rx coord from lteInfo (getCoord) and myCoord_
    dist = check_and_cast<LteRealisticChannelModel*>(channelModel_)->getCoord().distance(lteInfo->getCoord());

    // send H-ARQ feedback up
    if (lteInfo->getFrameType() == HARQPKT || lteInfo->getFrameType() == GRANTPKT || lteInfo->getFrameType() == RACPKT || lteInfo->getFrameType() == D2DMODESWITCHPKT)
    {
        handleControlMsg(frame, lteInfo);
        return;
    }

    // this is a DATA packet.

    // if the packet is a D2D multicast one, store it and decode it at the end of the TTI.
    // if not already started, auto-send a message to signal the presence of data to be decoded.
    if (strcmp(frame->getName(), "data frame") == 0 || strcmp(frame->getName(), "SCI") == 0)
    {
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
    }

    return;                          // exit the function, decoding will be done later
}


RbMap LtePhyVueV2X::getRbMap(unsigned int sub, bool isSci)
{
    /*
     * 0->1: 2->9       Sub-channel 0
     * 10->11: 12->19   Sub-channel 1
     * 20->21: 22->29   Sub-channel 2
     * 30->31: 32->39   Sub-channel 3
     * 40->41: 42->49   Sub-channel 4
     */
    RbMap grantedBlocks;
    if (isSci)  // 2 Rb's
    {
        std::map<Band, unsigned int> tempSci;
        for (unsigned short i = 0; i < 2; i++)
        {
            Band band = i+10*sub;
            tempSci[band] = 1;
        }
        grantedBlocks.emplace(MACRO, tempSci);
    } else // 8 Rb's
    {
        std::map<Band, unsigned int> temp;
        for (unsigned short j = 2; j < 10; j++)
        {
            Band band = j+10*sub;
            temp[band] = 1;
        }
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
        lteInfo->setDirection(D2D_MULTI);
        lteInfo->setFrameType(UNKNOWN_TYPE);
        sciFrame->setControlInfo(lteInfo);
        
        sendBroadcast(sciFrame);
        if (strcmp(dataFrame->getName(), "data frame") == 0)
        {
            int sd = grant->getCsr()->getSubchannel();
            lteInfoData->setGrantedBlocks(getRbMap(sd-1, false));
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
        lteInfoData->setFrameType(DATAPKT);
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
        //for each Remote unit used to transmit the packet
        for (const auto &myRemote : rbmap)
        {
            //for each logical band used to transmit the packet
            for ( const auto &myBand : myRemote.second ) {
                Band band = myBand.first;

                if (myBand.second == 0) continue;

                if (subchannelIndex == -1)
                {
                    if (band == 0 or band == 2) subchannelIndex = 0;
                    else if (band == 10 or band == 12) subchannelIndex = 1;
                    else if (band == 20 or band == 22) subchannelIndex = 2;
                    else if (band == 30 or band == 32) subchannelIndex = 3;
                    else if (band == 40 or band == 42) subchannelIndex = 4;
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

        // If it doesnt have control info dont add it to v2xReceivedFrames
        if (dynamic_cast<UserControlInfo*>(newFrame->getControlInfo()) == nullptr)
        {
            newFrame->setControlInfo(newInfo);
        }

        if (strcmp(newFrame->getName(), "SCI") == 0)
        {
            v2xReceivedFramesSCI_.push_back(newFrame);
        }
        else if (strcmp(newFrame->getName(), "data frame") == 0)
        {
            v2xReceivedFramesTB_.push_back(newFrame);
        }
    }
    //delete newFrame;
}

void LtePhyVueV2X::updatePointers()
{
    updatePointers_ = new cMessage("updatePointers");
    updatePointers_->setSchedulingPriority(10);
    scheduleAt(NOW+TTI, updatePointers_);
}

LteAirFrame* LtePhyVueV2X::extractAirFrame(bool isSci)
{
    // implements the capture effect
    // the vector is storing the frame received from the strongest/nearest transmitter
    LteAirFrame* af;
    if (isSci)
    {
        af = v2xReceivedFramesSCI_.back();
        v2xReceivedFramesSCI_.pop_back();
    } else
    {
        af = v2xReceivedFramesTB_.back();
        v2xReceivedFramesTB_.pop_back();
    }

    return af;
}

void LtePhyVueV2X::decodeAirFrame(LteAirFrame* frame, UserControlInfo* lteInfo)
{
    EV << NOW << " LtePhyUeD2D::decodeAirFrame - Start decoding..." << endl;
    cPacket* pkt = frame->decapsulate();
    // SCI has to be decoded first before decoding TB's.
    if (strcmp(frame->getName(), "SCI") == 0)
    {
        Sci* sci = check_and_cast<Sci*>(pkt);   // Extract SCI from frame.
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
        {
            numAirFramesWithSCIsReceived_++;
            emit(numAirFramesWithSCIsReceivedSignal_, numAirFramesWithSCIsReceived_);
            sciReceivedCorrectly[lteInfo->getSourceId()] = true;
        }
        else
        {
            numAirFramesWithSCIsNotReceived_++;
            emit(numAirFramesWithSCIsNotReceivedSignal_, numAirFramesWithSCIsNotReceived_);
            sciReceivedCorrectly[lteInfo->getSourceId()] = false;
        }
        // Emit distance between TX node and RX node.
        emit(distance_, dist);

        EV << "Handled LteAirframe (SCI) with ID " << frame->getId() << " with result "
           << ( result ? "RECEIVED" : "NOT RECEIVED" ) << endl;

        if (getEnvir()->isGUI())
            updateDisplayString();


    } else if (strcmp(frame->getName(), "data frame") == 0){


        if ( mcsMap.find(lteInfo->getSourceId()) == mcsMap.end())    // No SCI for the TB.
        {
            return;
        }

        // Use this mcs to decode the TB.
        unsigned int mcs = mcsMap[lteInfo->getSourceId()];


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
        {
            numAirFramesWithTBsReceived_++;
            emit(numAirFramesWithTBsReceivedSignal_, numAirFramesWithTBsReceived_);
            if (sciReceivedCorrectly[lteInfo->getSourceId()] == 1)
            {
                numSCIAndTBPairs_++;
            }
        }
        else
        {
            numAirFramesWithTBsNotReceived_++;
            emit(numAirFramesWithTBsNotReceivedSignal_, numAirFramesWithTBsNotReceived_);
        }
        emit(numSCIAndTBPairsSignal_, numSCIAndTBPairs_);
        sciReceivedCorrectly.clear();

        EV << "Handled LteAirframe (TB) with ID " << frame->getId() << " with result "
           << ( result ? "RECEIVED" : "NOT RECEIVED" ) << endl;

        // attach the decider result to the packet as control info
        lteInfo->setDeciderResult(result);
        pkt->setControlInfo(lteInfo);

        // send decapsulated message along with result control info to upperGateOut_
        send(pkt, upperGateOut_);

        if (getEnvir()->isGUI())
            updateDisplayString();
    }
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
