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
#include "stack/mac/packet/LteSchedulingGrantMode4.h"
#include "stack/mac/packet/CsrRequest.h"

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
        while (!d2dReceivedFrames_.empty())
        {
            LteAirFrame* frame = extractAirFrame();
            UserControlInfo* lteInfo = check_and_cast<UserControlInfo*>(frame->removeControlInfo());

            // decode the selected frame
            decodeAirFrame(frame, lteInfo);
        }

        delete msg;
        v2xDecodingTimer_ = NULL;
    } else if (msg->isName("updatePointers"))
    {
        current++;

        if (current == 1000) {current = 0;}
        pointerToEnd = current + 1;
        if (pointerToEnd == 1000) {pointerToEnd = 0;}
        std::vector<Subchannel*> subchannels = {new Subchannel(), new Subchannel(), new Subchannel()};
        sensingWindow[current] = subchannels;
        delete msg;
        updatePointers();
    } else if (msg->isName("updateSensingWindow"))
    {
        // TODO Needs revision. currTime etc.
        // Iterate through subchannels created from received AirFrames and update the sensing window.
        simtime_t currTime = NOW;
        float msAgo;
        int counter = 0, subchannelIndex, frameIndex;
        for (std::vector<Subchannel*>::iterator it = subchannelList.begin();
                (it != subchannelList.end() || counter == 3); it++)
        {
            msAgo = currTime - it->getTime();   // Gets how many ms ago the AirFrame was received.
                                                // eg. 0.001ms ago * 1000 = 1; 1-1 to get index in sensing window.
            frameIndex = static_cast<int>(msAgo);
            RbMap rbmap = it->getRbMap();
            RbMap::iterator it;
            it = rbmap.begin();
            if (rbmap.begin() == 0) subchannelIndex = 0;
            else if (rbmap.begin() == 9) subchannelIndex = 1;
            else subchannelIndex = 2;
            sensingWindow[frameIndex].insert(sensingWindow[frameIndex].begin()+subchannelIndex, it);
            counter++;
        }
        delete msg;
        updateSensingWindow_ = NULL;
    }
    else
        LtePhyUe::handleSelfMessage(msg);
}

void LtePhyVueV2X::chooseCsr(int t2, int prioTx, int pRsvpTx, int cResel)
{
    int pRsvpPrime, prioRx, k, lSubch, pRsvpRx, pStep, thresAddition,
    numSubch, numCsrs, subchIndex, numNotSensed = 0;
    bool notSensedChecked = false;

    std::vector<std::vector<Subchannel*>> Sa(t2, std::vector<Subchannel*>(3, new Subchannel()));

    std::vector< std::vector<Subchannel*>>::iterator frame;
    std::vector< std::vector<Subchannel*>>::iterator channel;

    do
    {
        numCsrs = 300;
        pStep = 100;

        pRsvpTxPrime = pStep * pRsvpTx / 100;
        k = 1;
        q = 1;

        lSubch = 1;
        numSubch = 3;

        thresAddition = 0;
        msAgo = 1000;
        wrapped = false;

        for (auto frame = sensingWindow.begin() + pointerToEnd;
                (frame != sensingWindow.begin() + pointerToEnd) || !wrapped; ++frame)
        {
            if (frame == sensingWindow.end())
            {
                frame = sensingWindow.begin();
                wrapped = true;
            }

            subchIndex = 0;
            for (channel = frame->begin(); channel != frame->end(); channel++)
            {
                if (channel->notSensed())
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
                                    Sa[y][0]->setNotSensed(true);
                                    Sa[y][1]->setNotSensed(true);
                                    Sa[y][2]->setNotSensed(true);
                                    numCsrs -= 3;
                                    numNotSensed += 3;

                                    j = cResel, y = 100, channel = frame->end();
                                }
                            }
                        }
                        notSensedChecked = true;
                    }
                } else if (!channel->isFree())
                {
                    prioRx = channel->getPriority();
                    pRsvpRx = channel->getResourceReservation();
                    if (msAgo - (pRsvpRx*100) < 0 || pRsvpRx == 0)
                    {
                        subchIndex++;
                        continue;
                    }
                    i = ((prioTx/100)*8) + prioRx + 1;
                    //infinity
                    if (i == 0 || i == 66) Th = nullptr; else Th = (-128 + (i-1)*2) + thresAddition;

                    if (channel->isRsrpGreaterThan(Th))
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
                                    numCsrs -= 3;
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
    } while (numCsrs < 0.2(300));

    std::vector< tuple<int,int,int>> rssiValues;
    std::vector<Subchannel*> listOfCsrs;
    Subchannel* channel;

    for (unsigned int x = 0; x < t2; x++)
    {
        for (unsigned int y = 0; y < 3; y++)
        {
            channel = Sa[x][y];
            channel->setSubframe(x+1);
            channel->setSubchannel(y+1);
            if (channel->notSensed())
            {
                continue;
            } else if (channel->isFree())
            {
                listOfCsrs.push_back(channel);
            } else
            {
                rssi = channel->getRssi();
                rssiValues.push_back(make_tuple(rssi, x, y));
            }
        }
    }

    sort(rssiValues.begin(), rssiValues.end());

    tuple<int,int,int> currentRssi;
    while (listOfCsrs.length() < 0.2(300))
    {
        currentRssi = rssiValues.pop_back();
        listOfCsrs.push_back(Sa[get<1>(currentRssi)][get<2>(currentRssi)]);
    }

    // Send list of Csrs to MAC layer.
    CsrMessage* csrMsg = new CsrMessage("CsrList");
    csrMsg->setCsrList(listOfCsrs);
    send(csrMsg,upperGateOut_);
}

int LtePhyVueV2X::calculateRiv(int lSubch, int numSubch, int startSubchIndex)
{
    int riv;
    if (lSubch-1 <= numSubch / 2)
    {
        riv = numSubch(lSubch-1)+startSubchIndex;
    } else
    {
        riv = numSubch(numSubch-lSubch+1) + numSubch - 1 - startSubchIndex;
    }
    return riv;
}

// TODO: ***reorganize*** method
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

    // this is a DATA packet

    // TODO Always broadcast
    // if the packet is a D2D multicast one, store it and decode it at the end of the TTI
    if (d2dMulticastEnableCaptureEffect_ && binder_->isInMulticastGroup(nodeId_,lteInfo->getMulticastGroupId()))
    {
        // if not already started, auto-send a message to signal the presence of data to be decoded
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

    // TODO replace with MCS?
    if ((lteInfo->getUserTxParams()) != NULL)
    {
        int cw = lteInfo->getCw();
        if (lteInfo->getUserTxParams()->readCqiVector().size() == 1)
            cw = 0;
        double cqi = lteInfo->getUserTxParams()->readCqiVector()[cw];
        if (lteInfo->getDirection() == DL)
            emit(averageCqiDl_, cqi);
    }
    // apply decider to received packet
    bool result = true;
    RemoteSet r = lteInfo->getUserTxParams()->readAntennaSet();
    if (r.size() > 1)
    {
        // DAS
        for (RemoteSet::iterator it = r.begin(); it != r.end(); it++)
        {
            EV << "LtePhyUeD2D: Receiving Packet from antenna " << (*it) << "\n";

            /*
             * On UE set the sender position
             * and tx power to the sender das antenna
             */

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

    // here frame has to be destroyed since it is no more useful
    delete frame;

    // attach the decider result to the packet as control info
    lteInfo->setDeciderResult(result);
    pkt->setControlInfo(lteInfo);

    // send decapsulated message along with result control info to upperGateOut_
    send(pkt, upperGateOut_);

    if (getEnvir()->isGUI())
        updateDisplayString();
}

void LtePhyVueV2X::handleUpperMessage(cMessage* msg)
{
    LteAirFrame* frame = NULL;
    UserControlInfo* lteInfo = check_and_cast<UserControlInfo*>(msg->removeControlInfo());
    CsrRequest* csrRequest = check_and_cast<CsrRequest*>(msg);

    // Grant contains info needed to create an SCI
    LteSchedulingGrantMode4* grant = check_and_cast<LteSchedulingGrantMode4*>(msg->remove_reference_t<LteSchedulingGrantMode4*>());
    if (lteInfo->getFrameType() == SCHEDGRANT)
    {
        // Received a grant so we create an SCI.
        Sci* sci = new Sci(grant->getPriority(), grant->getResourceReservation());

        // Extract csr from grant.
        Subchannel* csr = grant->getCsr();

        RbMap rbMap = lteInfo->getGrantedBlocks();
        UsedRBs info;
        info.time_ = NOW;
        info.rbMap_ = rbMap;

        //info.time_ = timeRequested + (csr->getSubframe()/1000);

        // Grant is received so send the SCI and data packet.
        sendBroadcast(frame);   // How do I add the SCI?? Do I encapsulate it in its own AirFrame.

        //After sending data packet, set data packet = null

        LteAirFrame* frame = NULL;

    } else if (csrRequest->isName() == "csrRequest")
    {
        timeRequested = NOW;
        chooseCsr(csrRequest->getPrioTx(), csrRequest->getRsvpTx(), csrRequest->getCResel());
        return;

    } else if (lteInfo->getFrameType() == DATAPKT)
    {
        // Message received is a data packet. Store it and send it + SCI when the next grant is received.
        // If there was no data packet received when a grant is received you still send an SCI.
        // if the data packet is null then just an SCI is sent.
        // create LteAirFrame and encapsulate the received packet

        frame = new LteAirFrame("airframe");

        frame->encapsulate(check_and_cast<cPacket*>(msg));

        // initialize frame fields

        frame->setSchedulingPriority(airFramePriority_);
        frame->setDuration(TTI);
        // set current position
        lteInfo->setCoord(getRadioPosition());

        lteInfo->setTxPower(txPower_);
        lteInfo->setD2dTxPower(d2dTxPower_);
        frame->setControlInfo(lteInfo);

    }

    /*
    // Store the RBs used for transmission. For interference computation
    RbMap rbMap = lteInfo->getGrantedBlocks();
    //UsedRBs info;
    //info.time_ = NOW;
    info.rbMap_ = rbMap;

    usedRbs_.push_back(info);

    std::vector<UsedRBs>::iterator it = usedRbs_.begin();
    while (it != usedRbs_.end())  // purge old allocations
    {
        if (it->time_ < NOW - 0.002)
            usedRbs_.erase(it++);
        else
            ++it;
    }
    lastActive_ = NOW;

    if (lteInfo->getFrameType() == DATAPKT && lteInfo->getUserTxParams() != NULL)
    {
        double cqi = lteInfo->getUserTxParams()->readCqiVector()[lteInfo->getCw()];
        if (lteInfo->getDirection() == UL)
            emit(averageCqiUl_, cqi);
        else if (lteInfo->getDirection() == D2D)
            emit(averageCqiD2D_, cqi);
    }

    EV << NOW << " LtePhyUeD2D::handleUpperMessage - message from stack" << endl;
    LteAirFrame* frame = NULL;

    if (lteInfo->getFrameType() == HARQPKT || lteInfo->getFrameType() == GRANTPKT || lteInfo->getFrameType() == RACPKT)
    {
        frame = new LteAirFrame("harqFeedback-grant");
    }
    else
    {
        // create LteAirFrame and encapsulate the received packet
        frame = new LteAirFrame("airframe");
    }

    frame->encapsulate(check_and_cast<cPacket*>(msg));

    // initialize frame fields

    frame->setSchedulingPriority(airFramePriority_);
    frame->setDuration(TTI);
    // set current position
    lteInfo->setCoord(getRadioPosition());

    lteInfo->setTxPower(txPower_);
    lteInfo->setD2dTxPower(d2dTxPower_);
    frame->setControlInfo(lteInfo);

    EV << "LtePhyUeD2D::handleUpperMessage - " << nodeTypeToA(nodeType_) << " with id " << nodeId_
       << " sending message to the air channel. Dest=" << lteInfo->getDestId() << endl;

    // Always broadcast message
    sendBroadcast(frame);
    */
}

void LtePhyVueV2X::storeAirFrame(LteAirFrame* newFrame, simtime_t time)
{
    // implements the capture effect
    // store the frame received from the nearest transmitter
    UserControlInfo* newInfo = check_and_cast<UserControlInfo*>(newFrame->getControlInfo());
    Coord myCoord = getCoord();
    std::vector<double> rsrpVector;
    std::vector<double> rssiVector;

    rsrpVector = channelModel_->getRSRP_D2D(newFrame, newInfo, nodeId_, myCoord);
    rssiVector = channelModel_->getSINR_D2D(newFrame, newInfo, nodeId_, myCoord);
    int rsrp = 0;
    int rssi = 0;

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

            rsrp += rsrpVector.at(band);
            rssi += rssiVector.at(band);
        }
    }

    Subchannel* subchannel = new Subchannel(sci, rbmap, rsrp, rssi, time);
    subchannelList.push_front(subchannel);
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

    // apply decider to received packet
    bool result = true;

    RemoteSet r = lteInfo->getUserTxParams()->readAntennaSet();
    if (r.size() > 1)
    {
        // DAS
        for (RemoteSet::iterator it = r.begin(); it != r.end(); it++)
        {
            EV << "LtePhyUeD2D::decodeAirFrame: Receiving Packet from antenna " << (*it) << "\n";

            /*
             * On UE set the sender position
             * and tx power to the sender das antenna
             */

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
