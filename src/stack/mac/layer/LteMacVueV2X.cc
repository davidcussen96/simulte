#include "stack/mac/layer/LteMacUeRealisticD2D.h"
#include "stack/mac/layer/LteMacVueV2X.h"
#include "stack/mac/buffer/harq/LteHarqBufferRx.h"
#include "stack/mac/buffer/LteMacQueue.h"
#include "stack/mac/packet/LteSchedulingGrant.h"
#include "stack/mac/scheduler/LteSchedulerUeUl.h"
#include "stack/mac/layer/LteMacEnbRealistic.h"
#include "stack/mac/buffer/harq_d2d/LteHarqBufferRxD2DMirror.h"
#include "stack/d2dModeSelection/D2DModeSwitchNotification_m.h"
#include "stack/mac/packet/LteRac_m.h"
#include "stack/phy/packet/CsrMessage.h"
#include "stack/subchannel/Subchannel.h"
#include "stack/mac/packet/CsrRequest_m.h"
#include <stdlib.h>

Define_Module(LteMacVueV2X);

LteMacVueV2X::LteMacVueV2X() :
    LteMacUeRealisticD2D()
{
    racD2DMulticastRequested_ = false;
    bsrD2DMulticastTriggered_ = false;
}

LteMacVueV2X::~LteMacVueV2X()
{
}

void LteMacVueV2X::initialize(int stage)
{
    LteMacUeRealisticD2D::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL)
    {
        // check the RLC module type: if it is not "RealisticD2D", abort simulation
        std::string pdcpType = getParentModule()->par("LtePdcpRrcType").stdstringValue();
        cModule* rlc = getParentModule()->getSubmodule("rlc");
        std::string rlcUmType = rlc->par("LteRlcUmType").stdstringValue();
        //bool rlcD2dCapable = rlc->par("d2dCapable").boolValue();
        bool rlcMode4Capable = rlc->par("mode4Capable").boolValue();
        //if (rlcUmType.compare("LteRlcUmRealistic") != 0 || !rlcD2dCapable)
        if (rlcUmType.compare("LteRlcUmRealisticD2D") != 0 || !rlcMode4Capable)
            throw cRuntimeError("LteMacVueV2X::initialize - %s module found, must be LteRlcUmRealisticD2D. Aborting", rlcUmType.c_str());
        //if (pdcpType.compare("LtePdcpRrcUeD2D") != 0)
        if (pdcpType.compare("LtePdcpRrcUeD2D") != 0)
            throw cRuntimeError("LteMacVueV2X::initialize - %s module found, must be LtePdcpRrcUeD2D. Aborting", pdcpType.c_str());
    }
    if (stage == inet::INITSTAGE_NETWORK_LAYER_3)
    {
        // get parameters
        usePreconfiguredTxParams_ = par("usePreconfiguredTxParams");
        preconfiguredTxParams_ = getPreconfiguredTxParams();

        // get the reference to the eNB
        enb_ = check_and_cast<LteMacEnbRealisticD2D*>(getSimulation()->getModule(binder_->getOmnetId(getMacCellId()))->getSubmodule("lteNic")->getSubmodule("mac"));

        LteAmc *amc = check_and_cast<LteMacEnb *>(getSimulation()->getModule(binder_->getOmnetId(cellId_))->getSubmodule("lteNic")->getSubmodule("mac"))->getAmc();
        amc->attachUser(nodeId_, D2D);
    }
}

//Function for create only a BSR for the eNB
LteMacPdu* LteMacVueV2X::makeBsr(int size){

    UserControlInfo* uinfo = new UserControlInfo();
    uinfo->setSourceId(getMacNodeId());
    uinfo->setDestId(getMacCellId());
    uinfo->setDirection(UL);
    uinfo->setUserTxParams(schedulingGrant_->getUserTxParams()->dup());
    LteMacPdu* macPkt = new LteMacPdu("LteMacPdu");
    macPkt->setHeaderLength(MAC_HEADER);
    macPkt->setControlInfo(uinfo);
    macPkt->setTimestamp(NOW);
    MacBsr* bsr = new MacBsr();
    bsr->setTimestamp(simTime().dbl());
    bsr->setSize(size);
    macPkt->pushCe(bsr);
    bsrTriggered_ = false;
    EV << "LteMacUeRealisticD2D::makeBsr() - BSR with size " << size << "created" << endl;
    return macPkt;
}

// TODO Need to modify this maybe?
void LteMacVueV2X::macPduMake()
{
    int64 size = 0;

    macPduList_.clear();

    // We dont use BSRs?
    bool bsrAlreadyMade = false;
    // UE is in D2D-mode but it received an UL grant (for BSR)
    // Mode 4 always in D2D-mode: delete-> Wont receive BSR.
    // NEVER TRIGGERED!!
    if ((bsrTriggered_ || bsrD2DMulticastTriggered_) && schedulingGrant_->getDirection() == UL && scheduleList_->empty())
    {
        // Compute BSR size taking into account only DM flows
        int sizeBsr = 0;
        LteMacBufferMap::const_iterator itbsr;
        for (itbsr = macBuffers_.begin(); itbsr != macBuffers_.end(); itbsr++)
        {
            MacCid cid = itbsr->first;
            Direction connDir = (Direction)connDesc_[cid].getDirection();

            // if the bsr was triggered by D2D (D2D_MULTI), only account for D2D (D2D_MULTI) connections
            if (bsrTriggered_ && connDir != D2D)
                continue;
            if (bsrD2DMulticastTriggered_ && connDir != D2D_MULTI)
                continue;

            sizeBsr += itbsr->second->getQueueOccupancy();

            // take into account the RLC header size
            if (sizeBsr > 0)
            {
                if (connDesc_[cid].getRlcType() == UM)
                    sizeBsr += RLC_HEADER_UM;
                else if (connDesc_[cid].getRlcType() == AM)
                    sizeBsr += RLC_HEADER_AM;
            }
        }

        if (sizeBsr > 0)
        {
            // Call the appropriate function for make a BSR for a D2D communication
            LteMacPdu* macPktBsr = makeBsr(sizeBsr);
            UserControlInfo* info = check_and_cast<UserControlInfo*>(macPktBsr->getControlInfo());
            if (bsrD2DMulticastTriggered_)
            {
                info->setLcid(D2D_MULTI_SHORT_BSR);
                bsrD2DMulticastTriggered_ = false;
            }
            else
                info->setLcid(D2D_SHORT_BSR);

            // Add the created BSR to the PDU List
            if( macPktBsr != NULL )
            {
               macPduList_[ std::pair<MacNodeId, Codeword>( getMacCellId(), 0) ] = macPktBsr;
               bsrAlreadyMade = true;
               EV << "LteMacUeRealisticD2D::macPduMake - BSR D2D created with size " << sizeBsr << "created" << endl;
            }
        }
        else
        {
            bsrD2DMulticastTriggered_ = false;
            bsrTriggered_ = false;
        }
    }
    // ALWAYS EXECUTED!!
    if(!bsrAlreadyMade)
    {
        // In a D2D communication if BSR was created above this part isn't executed
        // Build a MAC PDU for each scheduled user on each codeword -> Build one MAC PDU for broadcast

        LteMacScheduleList::const_iterator it;
        for (it = scheduleList_->begin(); it != scheduleList_->end(); it++)     //->scheduleList is empty
        {
            LteMacPdu* macPkt;
            cPacket* pkt;

            MacCid destCid = it->first.first;   // IS THERE A BROADCAST destCid
            Codeword cw = it->first.second;     // ALWAYS 0

            // get the direction (UL/D2D/D2D_MULTI) and the corresponding destination ID
            FlowControlInfo* lteInfo = &(connDesc_.at(destCid));
            MacNodeId destId = lteInfo->getDestId();    // MULTI GROUP ID??
            Direction dir = (Direction)lteInfo->getDirection();     // ALWAYS D2D

            std::pair<MacNodeId, Codeword> pktId = std::pair<MacNodeId, Codeword>(destId, cw);
            unsigned int sduPerCid = it->second;

            MacPduList::iterator pit = macPduList_.find(pktId);
            // ??
            if (sduPerCid == 0 && !bsrTriggered_ && !bsrD2DMulticastTriggered_)
            {
                continue;
            }

            // No packets for this user on this codeword
            // Only one codeword, 0 or 1??
            if (pit == macPduList_.end())
            {
                // Always goes here because of the macPduList_.clear() at the beginning
                // Build the Control Element of the MAC PDU
                UserControlInfo* uinfo = new UserControlInfo();
                uinfo->setSourceId(getMacNodeId());
                uinfo->setDestId(destId);
                uinfo->setLcid(MacCidToLcid(destCid));
                uinfo->setDirection(dir);
                uinfo->setLcid(MacCidToLcid(SHORT_BSR));
                if (usePreconfiguredTxParams_)
                    uinfo->setUserTxParams(preconfiguredTxParams_->dup());
                else
                    uinfo->setUserTxParams(schedulingGrant_->getUserTxParams()->dup());
                // Create a PDU
                macPkt = new LteMacPdu("LteMacPdu");
                macPkt->setHeaderLength(MAC_HEADER);
                macPkt->setControlInfo(uinfo);
                macPkt->setTimestamp(NOW);
                macPduList_[pktId] = macPkt;
            }
            else
            {
                // Never goes here because of the macPduList_.clear() at the beginning
                macPkt = pit->second;
            }

            while (sduPerCid > 0)
            {
                // Add SDU to PDU
                // Find Mac Pkt
                if (mbuf_.find(destCid) == mbuf_.end())
                    throw cRuntimeError("Unable to find mac buffer for cid %d", destCid);

                if (mbuf_[destCid]->empty())
                    throw cRuntimeError("Empty buffer for cid %d, while expected SDUs were %d", destCid, sduPerCid);

                pkt = mbuf_[destCid]->popFront();

                // multicast support
                // this trick gets the group ID from the MAC SDU and sets it in the MAC PDU
                int32 groupId = check_and_cast<LteControlInfo*>(pkt->getControlInfo())->getMulticastGroupId();
                if (groupId >= 0) // for unicast, group id is -1
                    check_and_cast<LteControlInfo*>(macPkt->getControlInfo())->setMulticastGroupId(groupId);

                drop(pkt);

                macPkt->pushSdu(pkt);
                sduPerCid--;
            }

            // consider virtual buffers to compute BSR size
            size += macBuffers_[destCid]->getQueueOccupancy();

            if (size > 0)
            {
                // take into account the RLC header size
                if (connDesc_[destCid].getRlcType() == UM)
                    size += RLC_HEADER_UM;
                else if (connDesc_[destCid].getRlcType() == AM)
                    size += RLC_HEADER_AM;
            }
        }
    }
    // Just want to put one in?
    // Put MAC PDUs in H-ARQ buffers
    MacPduList::iterator pit;
    for (pit = macPduList_.begin(); pit != macPduList_.end(); pit++)
    {
        MacNodeId destId = pit->first.first;
        Codeword cw = pit->first.second;
        // Check if the HarqTx buffer already exists for the destId
        // Get a reference for the destId TXBuffer
        LteHarqBufferTx* txBuf;
        HarqTxBuffers::iterator hit = harqTxBuffers_.find(destId);
        if ( hit != harqTxBuffers_.end() )
        {
            // The tx buffer already exists
            txBuf = hit->second;
        }
        else
        {
            // The tx buffer does not exist yet for this mac node id, create one
            LteHarqBufferTx* hb;
            // FIXME: hb is never deleted
            UserControlInfo* info = check_and_cast<UserControlInfo*>(pit->second->getControlInfo());
            if (info->getDirection() == UL)
                hb = new LteHarqBufferTx((unsigned int) ENB_TX_HARQ_PROCESSES, this, (LteMacBase*) getMacByMacNodeId(destId));
            else // D2D or D2D_MULTI
                hb = new LteHarqBufferTxD2D((unsigned int) ENB_TX_HARQ_PROCESSES, this, (LteMacBase*) getMacByMacNodeId(destId));
            harqTxBuffers_[destId] = hb;
            txBuf = hb;
        }

        // search for an empty unit within current harq process
        UnitList txList = txBuf->getEmptyUnits(currentHarq_);
        EV << "LteMacUeRealisticD2D::macPduMake - [Used Acid=" << (unsigned int)txList.first << "] , [curr=" << (unsigned int)currentHarq_ << "]" << endl;

        //Get a reference of the LteMacPdu from pit pointer (extract Pdu from the MAP)
        LteMacPdu* macPkt = pit->second;

        // Attach BSR to PDU if RAC is won and wasn't already made
        if ((bsrTriggered_ || bsrD2DMulticastTriggered_) && !bsrAlreadyMade )
        {
            MacBsr* bsr = new MacBsr();
            bsr->setTimestamp(simTime().dbl());
            bsr->setSize(size);
            macPkt->pushCe(bsr);
            bsrTriggered_ = false;
            bsrD2DMulticastTriggered_ = false;
            EV << "LteMacUeRealisticD2D::macPduMake - BSR created with size " << size << endl;
        }

        EV << "LteMacUeRealisticD2D: pduMaker created PDU: " << macPkt->info() << endl;

        // TODO: harq test
        // pdu transmission here (if any)
        // txAcid has HARQ_NONE for non-fillable codeword, acid otherwise
        if (txList.second.empty())
        {
            EV << "LteMacUeRealisticD2D() : no available process for this MAC pdu in TxHarqBuffer" << endl;
            delete macPkt;
        }
        else
        {
            //Insert PDU in the Harq Tx Buffer
            //txList.first is the acid
            txBuf->insertPdu(txList.first,cw, macPkt);
        }
    }
}

void LteMacVueV2X::handleMessage(cMessage* msg)
{
    if (msg->isSelfMessage())
    {
        LteMacUeRealisticD2D::handleMessage(msg);
        return;
    }

    cPacket* pkt = check_and_cast<cPacket *>(msg);
    cGate* incoming = pkt->getArrivalGate();

    if (incoming == down_[IN])
    {
        //UserControlInfo *userInfo = check_and_cast<UserControlInfo *>(pkt->getControlInfo());

        EV << "LteMacVueV2X::handleMessage - Received packet " << pkt->getName() <<
                    " from port " << pkt->getArrivalGate()->getName() << endl;


        if (dynamic_cast<CsrMessage*>(pkt) != nullptr)
        {
            CsrMessage* csrMsg = check_and_cast<CsrMessage*>(pkt);
            currentCsr = chooseCsrAtRandom(csrMsg->getCsrList());

            // currentCsr has a subframe and subchannel variable to determine when in the selection window it occurred and
            // in what RBs
            LteSchedulingGrantMode4* grant = check_and_cast<LteSchedulingGrantMode4*>(schedulingGrant_);
            // Set Grants Full RbMap
            grant->setCsr(currentCsr);
            const RbMap rbmap = getFullRbMap(grant->getCsr()->getSubchannel());
            grant->setGrantedBlocks(rbmap);
            grant->setTotalGrantedBlocks(10);
            periodCounter_ = currentCsr->getSubframe();
            csrReceived = true;

            return;
        }
    }

    LteMacUeRealisticD2D::handleMessage(msg);
}

RbMap LteMacVueV2X::getFullRbMap(unsigned int subch)
{
    RbMap grantedBlocks;
    std::map<Band, unsigned int> temp;
    for (unsigned short i = 0; i < 10; i++)
    {
        Band band = i+10*subch;
        temp[band] = 1;
    }
    grantedBlocks.emplace(MACRO, temp);
    return grantedBlocks;
}

void LteMacVueV2X::sendCsrRequest()
{
    // Create a grant before sending a request for a CSR.
    // This partially configures the grant with the required fields
    // needed to generate a CSR.
    macHandleGrant();
    CsrRequest* csrRequest = new CsrRequest("csrRequest");

    csrRequest->setCResel(schedulingGrant_->getExpiration());
    LteSchedulingGrantMode4* grant = check_and_cast<LteSchedulingGrantMode4*>(schedulingGrant_);
    csrRequest->setPrioTx(grant->getPriority());
    csrRequest->setPRsvpTx(grant->getResourceReservation());

    UserControlInfo* uinfo = new UserControlInfo();
    uinfo->setSourceId(getMacNodeId());
    uinfo->setDestId(getMacCellId());
    uinfo->setDirection(DL);
    uinfo->setFrameType(CSRREQUEST);
    csrRequest->setControlInfo(uinfo);

    sendLowerPackets(csrRequest);
}

Subchannel* LteMacVueV2X::chooseCsrAtRandom(std::vector<Subchannel*> csrList)
{
    int csrListSize = csrList.size();
    int index = rand() % csrListSize; // 0 --> csrList.size()-1
    Subchannel* csr = csrList[index];
    return csr;
}

void LteMacVueV2X::macHandleGrant()
{
    EV << NOW << " LteMacUeRealisticD2D::macHandleGrant - UE [" << nodeId_ << "] - Grant received " << endl;

    // delete old grant
    LteSchedulingGrantMode4 *grant = new LteSchedulingGrantMode4();
    Direction dir = (Direction)D2D_MULTI;
    grant->setDirection(dir);
    grant->setPeriod(100);
    grant->setPriority(1);
    grant->setResourceReservation(100);
    grant->setCodewords(1);
    grant->setTotalGrantedBlocks(1);

    grant->setUserTxParams(preconfiguredTxParams_->dup());

    int exp = rand() % 11 + 5;;
    grant->setExpiration(exp);

    // Set periodic to true so that periodCounter and expirationCounter get set.
    grant->setPeriodic(true);

    UserControlInfo* uinfo = new UserControlInfo();
    uinfo->setSourceId(getMacNodeId());
    uinfo->setDestId(getMacCellId());       //?? Is this the destination ??
    uinfo->setDirection(D2D_MULTI);
    uinfo->setFrameType(GRANTPKT);
    grant->setControlInfo(uinfo);

    //Codeword cw = grant->getCodeword();

    if (schedulingGrant_!=NULL)
    {
        schedulingGrant_ = NULL;
        delete schedulingGrant_;
    }

    // store received grant
    schedulingGrant_=grant;

    if (grant->getPeriodic())
    {
        periodCounter_=grant->getPeriod();
        expirationCounter_=grant->getExpiration();
    }

    EV << NOW << "Node " << nodeId_ << " received grant of blocks " << grant->getTotalGrantedBlocks()
       << ", bytes " << grant->getGrantedCwBytes(0) <<" Direction: "<<dirToA(grant->getDirection()) << endl;

}

void LteMacVueV2X::createIntermediateGrant(unsigned int expCounter, unsigned int period, Subchannel* subch, const RbMap& rbmap)
{
    // delete old grant
        LteSchedulingGrantMode4 *grant = new LteSchedulingGrantMode4();
        Direction dir = (Direction)D2D_MULTI;
        grant->setDirection(dir);
        grant->setPeriod(100);
        grant->setPriority(1);
        grant->setResourceReservation(100);
        grant->setCodewords(1);
        grant->setCsr(subch);
        grant->setTotalGrantedBlocks(1);

        grant->setPeriod(period);
        grant->setExpiration(expCounter);

        // Set periodic to true so that periodCounter and expirationCounter get set.
        grant->setPeriodic(true);

        grant->setUserTxParams(preconfiguredTxParams_->dup());

        UserControlInfo* uinfo = new UserControlInfo();
        uinfo->setSourceId(getMacNodeId());
        uinfo->setDestId(getMacCellId());
        uinfo->setDirection(D2D_MULTI);
        uinfo->setFrameType(GRANTPKT);
        grant->setControlInfo(uinfo);

        grant->setGrantedBlocks(rbmap);
        grant->setTotalGrantedBlocks(10);
        grant->setCsr(currentCsr);
        if (dataPktReceived)
        {
            unsigned int grantedCwBytes = schedulingGrant_->getGrantedCwBytes(0);
            grant->setGrantedCwBytes(0, grantedCwBytes);
            //dataPktReceived = false;
        }


        if (schedulingGrant_!=NULL)
        {
            schedulingGrant_ = NULL;
            delete schedulingGrant_;
        }

        // store grant
        schedulingGrant_=grant;
        /*
        EV << NOW << "Node " << nodeId_ << " received grant of blocks " << grant->getTotalGrantedBlocks()
           << ", bytes " << grant->getGrantedCwBytes(0) <<" Direction: "<<dirToA(grant->getDirection()) << endl;
        */
}

void LteMacVueV2X::checkRAC()
{
    EV << NOW << " LteMacUeRealisticD2D::checkRAC , Ue  " << nodeId_ << ", racTimer : " << racBackoffTimer_ << " maxRacTryOuts : " << maxRacTryouts_
       << ", raRespTimer:" << raRespTimer_ << endl;

    if (racBackoffTimer_>0)
    {
        racBackoffTimer_--;
        return;
    }

    if(raRespTimer_>0)
    {
        // decrease RAC response timer
        raRespTimer_--;
        EV << NOW << " LteMacUeRealisticD2D::checkRAC - waiting for previous RAC requests to complete (timer=" << raRespTimer_ << ")" << endl;
        return;
    }

    // Avoids double requests whithin same TTI window
    if (racRequested_)
    {
        EV << NOW << " LteMacUeRealisticD2D::checkRAC - double RAC request" << endl;
        racRequested_=false;
        return;
    }
    if (racD2DMulticastRequested_)
    {
        EV << NOW << " LteMacUeRealisticD2D::checkRAC - double RAC request" << endl;
        racD2DMulticastRequested_=false;
        return;
    }

    bool trigger=false;
    bool triggerD2DMulticast=false;

    LteMacBufferMap::const_iterator it;

    for (it = macBuffers_.begin(); it!=macBuffers_.end();++it)
    {
        if (!(it->second->isEmpty()))
        {
            MacCid cid = it->first;
            if (connDesc_.at(cid).getDirection() == D2D_MULTI)
                triggerD2DMulticast = true;
            else
                trigger = true;
            break;
        }
    }

    if (!trigger && !triggerD2DMulticast)
        EV << NOW << "Ue " << nodeId_ << ",RAC aborted, no data in queues " << endl;

    if ((racRequested_=trigger) || (racD2DMulticastRequested_=triggerD2DMulticast))
    {
        LteRac* racReq = new LteRac("RacRequest");
        UserControlInfo* uinfo = new UserControlInfo();
        uinfo->setSourceId(getMacNodeId());
        uinfo->setDestId(getMacCellId());
        uinfo->setDirection(UL);
        uinfo->setFrameType(RACPKT);
        racReq->setControlInfo(uinfo);

        sendLowerPackets(racReq);

        EV << NOW << " Ue  " << nodeId_ << " cell " << cellId_ << " ,RAC request sent to PHY " << endl;

        // wait at least  "raRespWinStart_" TTIs before another RAC request
        raRespTimer_ = raRespWinStart_;
    }
}

void LteMacVueV2X::macHandleRac(cPacket* pkt)
{
    LteRac* racPkt = check_and_cast<LteRac*>(pkt);

    if (racPkt->getSuccess())
    {
        EV << "LteMacUeRealisticD2D::macHandleRac - Ue " << nodeId_ << " won RAC" << endl;
        // is RAC is won, BSR has to be sent
        if (racD2DMulticastRequested_)
            bsrD2DMulticastTriggered_=true;
        else
            bsrTriggered_ = true;

        // reset RAC counter
        currentRacTry_=0;
        //reset RAC backoff timer
        racBackoffTimer_=0;
    }
    else
    {
        // RAC has failed
        if (++currentRacTry_ >= maxRacTryouts_)
        {
            EV << NOW << " Ue " << nodeId_ << ", RAC reached max attempts : " << currentRacTry_ << endl;
            // no more RAC allowed
            //! TODO flush all buffers here
            //reset RAC counter
            currentRacTry_=0;
            //reset RAC backoff timer
            racBackoffTimer_=0;
        }
        else
        {
            // recompute backoff timer
            racBackoffTimer_= uniform(minRacBackoff_,maxRacBackoff_);
            EV << NOW << " Ue " << nodeId_ << " RAC attempt failed, backoff extracted : " << racBackoffTimer_ << endl;
        }
    }
    delete racPkt;
}


void LteMacVueV2X::handleUpperMessage(cPacket* pkt)
{
    FlowControlInfo* lteInfo = check_and_cast<FlowControlInfo*>(pkt->getControlInfo());
    MacCid cid = idToMacCid(lteInfo->getDestId(), lteInfo->getLcid());

    // Before we bufferize packet we need to determine size of packet and then set scheduling grants message size.
    if (strcmp(pkt->getName(), "newDataPkt") == 0)
    {
        pktSize = pkt->getByteLength();
        // Now we need to set the scheduling grant. What method is used ?? setGrantedCwBytes() ??
        dataPktReceived = true;
    }

    if (schedulingGrant_ != NULL and dataPktReceived)
    {
        schedulingGrant_->setGrantedCwBytes(0, pktSize);
        dataPktReceived = false;
        pktSize = 0;
    }

    // bufferize packet
    bufferizePacket(pkt);

    if (strcmp(pkt->getName(), "lteRlcFragment") == 0)
    {
        // new MAC SDU has been received
        if (pkt->getByteLength() == 0)
            delete pkt;

        // creates pdus from schedule list and puts them in harq buffers.
        macPduMake();

        EV << NOW << " LteMacUeRealistic::handleUpperMessage - incrementing counter for HARQ processes " << (unsigned int)currentHarq_ << " --> " << (currentHarq_+1)%harqProcesses_ << endl;
        currentHarq_ = (currentHarq_+1)%harqProcesses_;
    }
    else
    {
        delete pkt;
    }
}


void LteMacVueV2X::handleSelfMessage()
{
    EV << "----- UE MAIN LOOP -----" << endl;

    // extract pdus from all harqrxbuffers and pass them to unmaker
    HarqRxBuffers::iterator hit = harqRxBuffers_.begin();
    HarqRxBuffers::iterator het = harqRxBuffers_.end();
    LteMacPdu *pdu = NULL;
    std::list<LteMacPdu*> pduList;

    for (; hit != het; ++hit)
    {
        pduList=hit->second->extractCorrectPdus();
        while (! pduList.empty())
        {
            pdu=pduList.front();
            pduList.pop_front();
            macPduUnmake(pdu);
        }
    }

    // For each D2D communication, the status of the HARQRxBuffer must be known to the eNB
    // For each HARQ-RX Buffer corresponding to a D2D communication, store "mirror" buffer at the eNB
    HarqRxBuffers::iterator buffIt = harqRxBuffers_.begin();
    for (; buffIt != harqRxBuffers_.end(); ++buffIt)
    {
        MacNodeId senderId = buffIt->first;
        LteHarqBufferRx* buff = buffIt->second;

        // skip the H-ARQ buffer corresponding to DL transmissions
        if (senderId == cellId_)
            continue;

        // skip the H-ARQ buffers corresponding to D2D_MULTI transmissions
        if (buff->isMulticast())
            continue;

        //The constructor "extracts" all the useful information from the harqRxBuffer and put them in a LteHarqBufferRxD2DMirror object
        //That object resides in enB. Because this operation is done after the enb main loop the enb is 1 TTI backward respect to the Receiver
        //This means that enb will check the buffer for retransmission 3 TTI before
        LteHarqBufferRxD2DMirror* mirbuff = new LteHarqBufferRxD2DMirror(buff, (unsigned char)this->par("maxHarqRtx"), senderId);
        enb_->storeRxHarqBufferMirror(nodeId_, mirbuff);
    }

    EV << NOW << "LteMacUeRealisticD2D::handleSelfMessage " << nodeId_ << " - HARQ process " << (unsigned int)currentHarq_ << endl;
    // updating current HARQ process for next TTI

    //unsigned char currentHarq = currentHarq_;

    // no grant available - if user has backlogged data, it will trigger scheduling request
    // no harq counter is updated since no transmission is sent.


    // INITIALIZATION STAGE?
    if (schedulingGrant_==NULL)
    {
        EV << NOW << " LteMacUeRealisticD2D::handleSelfMessage " << nodeId_ << " NO configured grant" << endl;

        sendCsrRequest();   // Will this all complete before the next self message is called.
                            // That is partially configure grant, csrRequest to Phy, Phy chooses CSR
                            // and sends back to MAC, MAC attaches CSR to grant.

    } else if (csrReceived) // We only want to start the expCounter when we first send our grant at time indicated by the CSR.
    {
        if (schedulingGrant_->getPeriodic())
        {
            if (periodCounter_ > 0)
            {
                periodCounter_ -= 1;
            } else // equal to zero
            {
                LteSchedulingGrantMode4* grant = check_and_cast<LteSchedulingGrantMode4*>(schedulingGrant_);
                createIntermediateGrant(schedulingGrant_->getExpiration(), schedulingGrant_->getPeriod(), grant->getCsr(), grant->getGrantedBlocks());
                // Creates a new grant with control info because control info is removed in PHY layer.
                sendLowerPackets(schedulingGrant_);
                if (--expirationCounter_ == 0)
                {
                    int p = rand() % 5 + 1;
                    if (p == 1) {
                        expirationCounter_ = rand() % 11 + 5;
                    } else {
                        // Periodic grant is expired
                        csrReceived = false;
                        sendCsrRequest();
                    }
                }
                else    // expiration counter is NOT equal to 0
                {
                    // resetting grant period
                    periodCounter_=schedulingGrant_->getPeriod();
                }
            }
        }
    }


    bool requestSdu = false;
    // TODO Should this not be done every TTI, seems wasteful
    // if a grant is configured. A grant can be partially configured
    //if (schedulingGrant_!=NULL and csrReceived)
    if (schedulingGrant_!=NULL)
    {                                           // But only when a CSR is received is it fully configured.
        if(!firstTx)
        {
            EV << "\t currentHarq_ counter initialized " << endl;
            firstTx=true;
            // the eNb will receive the first pdu in 2 TTI, thus initializing acid to 0
//            currentHarq_ = harqRxBuffers_.begin()->second->getProcesses() - 2;
            currentHarq_ = UE_TX_HARQ_PROCESSES - 2;
        }

        EV << "\t " << "schedulingGrant_" << endl;

        EV << NOW << " LteMacUeRealisticD2D::handleSelfMessage " << nodeId_ << " entered scheduling" << endl;

        bool retx = false;

        HarqTxBuffers::iterator it2;
        LteHarqBufferTx * currHarq;
        for(it2 = harqTxBuffers_.begin(); it2 != harqTxBuffers_.end(); it2++)
        {
            EV << "\t Looking for retx in acid " << (unsigned int)currentHarq_ << endl;
            currHarq = it2->second;

            // check if the current process has unit ready for retx
            bool ready = currHarq->getProcess(currentHarq_)->hasReadyUnits();
            CwList cwListRetx = currHarq->getProcess(currentHarq_)->readyUnitsIds();

            EV << "\t [process=" << (unsigned int)currentHarq_ << "] , [retx=" << ((retx)?"true":"false")
               << "] , [n=" << cwListRetx.size() << "]" << endl;

            // if a retransmission is needed
            if(ready)
            {
                UnitList signal;
                signal.first=currentHarq_;
                signal.second = cwListRetx;
                currHarq->markSelected(signal,schedulingGrant_->getUserTxParams()->getLayers().size());
                retx = true;
            }
        }
        // if no retx is needed, proceed with normal scheduling
        if(!retx)
        {
            scheduleList_ = lcgScheduler_->schedule();      // Always returns an empty schedule list??
            bool requestSdu = macSduRequest(); // return a bool -> Never called
            if (requestSdu and scheduleList_->empty())
            {
                // no connection scheduled, but we can use this grant to send a BSR to the eNB
                macPduMake();       // Always called.
            }
        }

        // Message that triggers flushing of Tx H-ARQ buffers for all users
        // This way, flushing is performed after the (possible) reception of new MAC PDUs
        cMessage* flushHarqMsg = new cMessage("flushHarqMsg");
        flushHarqMsg->setSchedulingPriority(1);        // after other messages
        scheduleAt(NOW, flushHarqMsg);
    }

    //============================ DEBUG ==========================
    HarqTxBuffers::iterator it;

    EV << "\n htxbuf.size " << harqTxBuffers_.size() << endl;

    int cntOuter = 0;
    int cntInner = 0;
    for(it = harqTxBuffers_.begin(); it != harqTxBuffers_.end(); it++)
    {
        LteHarqBufferTx* currHarq = it->second;
        BufferStatus harqStatus = currHarq->getBufferStatus();
        BufferStatus::iterator jt = harqStatus.begin(), jet= harqStatus.end();

        EV << "\t cicloOuter " << cntOuter << " - bufferStatus.size=" << harqStatus.size() << endl;
        for(; jt != jet; ++jt)
        {
            EV << "\t\t cicloInner " << cntInner << " - jt->size=" << jt->size()
               << " - statusCw(0/1)=" << jt->at(0).second << "/" << jt->at(1).second << endl;
        }
    }
    //======================== END DEBUG ==========================

    unsigned int purged =0;
    // purge from corrupted PDUs all Rx H-HARQ buffers
    for (hit= harqRxBuffers_.begin(); hit != het; ++hit)
    {
        // purge corrupted PDUs only if this buffer is for a DL transmission. Otherwise, if you
        // purge PDUs for D2D communication, also "mirror" buffers will be purged
        if (hit->first == cellId_)
            purged += hit->second->purgeCorruptedPdus();
    }
    EV << NOW << " LteMacUeRealisticD2D::handleSelfMessage Purged " << purged << " PDUS" << endl;

    if (requestSdu == false)
    {
        // update current harq process id
        currentHarq_ = (currentHarq_+1) % harqProcesses_;
    }

    EV << "--- END UE MAIN LOOP ---" << endl;
}


UserTxParams* LteMacVueV2X::getPreconfiguredTxParams()
{
    UserTxParams* txParams = new UserTxParams();

    // default parameters for D2D
    txParams->isSet() = true;
    txParams->writeTxMode(TRANSMIT_DIVERSITY);
    Rank ri = 1;                                              // rank for TxD is one
    txParams->writeRank(ri);
    txParams->writePmi(intuniform(1, pow(ri, (double) 2)));   // taken from LteFeedbackComputationRealistic::computeFeedback

    Cqi cqi = par("d2dCqi");
    if (cqi < 0 || cqi > 15)
        throw cRuntimeError("LteMacUeRealisticD2D::getPreconfiguredTxParams - CQI %s is not a valid value. Aborting", cqi);
    txParams->writeCqi(std::vector<Cqi>(1,cqi));

    BandSet b;
    for (Band i = 0; i < getDeployer(nodeId_)->getNumBands(); ++i) b.insert(i);

    RemoteSet antennas;
    antennas.insert(MACRO);
    txParams->writeAntennas(antennas);

    return txParams;
}

void LteMacVueV2X::macHandleD2DModeSwitch(cPacket* pkt)
{
    EV << NOW << " LteMacUeRealisticD2D::macHandleD2DModeSwitch - Start" << endl;

    // all data in the MAC buffers of the connection to be switched are deleted

    D2DModeSwitchNotification* switchPkt = check_and_cast<D2DModeSwitchNotification*>(pkt);
    bool txSide = switchPkt->getTxSide();
    MacNodeId peerId = switchPkt->getPeerId();
    LteD2DMode newMode = switchPkt->getNewMode();
    LteD2DMode oldMode = switchPkt->getOldMode();
    UserControlInfo* uInfo = check_and_cast<UserControlInfo*>(pkt->removeControlInfo());

    if (txSide)
    {
        Direction newDirection = (newMode == DM) ? D2D : UL;
        Direction oldDirection = (oldMode == DM) ? D2D : UL;

        // find the correct connection involved in the mode switch
        MacCid cid;
        FlowControlInfo* lteInfo = NULL;
        std::map<MacCid, FlowControlInfo>::iterator it = connDesc_.begin();
        for (; it != connDesc_.end(); ++it)
        {
            cid = it->first;
            lteInfo = check_and_cast<FlowControlInfo*>(&(it->second));

            if (lteInfo->getD2dRxPeerId() == peerId && (Direction)lteInfo->getDirection() == oldDirection)
            {
                EV << NOW << " LteMacUeRealisticD2D::macHandleD2DModeSwitch - found old connection with cid " << cid << ", erasing buffered data" << endl;
                if (oldDirection != newDirection)
                {
                    // empty virtual buffer for the selected cid
                    LteMacBufferMap::iterator macBuff_it = macBuffers_.find(cid);
                    if (macBuff_it != macBuffers_.end())
                    {
                        while (!(macBuff_it->second->isEmpty()))
                            macBuff_it->second->popFront();
                        delete macBuff_it->second;
                        macBuffers_.erase(macBuff_it);
                    }

                    // empty real buffer for the selected cid (they should be already empty)
                    LteMacBuffers::iterator mBuf_it = mbuf_.find(cid);
                    if (mBuf_it != mbuf_.end())
                    {
                        while (mBuf_it->second->getQueueLength() > 0)
                        {
                            cPacket* pdu = mBuf_it->second->popFront();
                            delete pdu;
                        }
                        delete mBuf_it->second;
                        mbuf_.erase(mBuf_it);
                    }

                    // interrupt H-ARQ processes for SL
                    unsigned int id = peerId;
                    HarqTxBuffers::iterator hit = harqTxBuffers_.find(id);
                    if (hit != harqTxBuffers_.end())
                    {
                        for (int proc = 0; proc < (unsigned int) UE_TX_HARQ_PROCESSES; proc++)
                        {
                            hit->second->forceDropProcess(proc);
                        }
                    }

                    // interrupt H-ARQ processes for UL
                    id = getMacCellId();
                    hit = harqTxBuffers_.find(id);
                    if (hit != harqTxBuffers_.end())
                    {
                        for (int proc = 0; proc < (unsigned int) UE_TX_HARQ_PROCESSES; proc++)
                        {
                            hit->second->forceDropProcess(proc);
                        }
                    }
                }

                // abort BSR requests
                bsrTriggered_ = false;

                D2DModeSwitchNotification* switchPkt_dup = switchPkt->dup();
                switchPkt_dup->setControlInfo(lteInfo->dup());
                switchPkt_dup->setOldConnection(true);
                sendUpperPackets(switchPkt_dup);

                if (oldDirection != newDirection)
                {
                    // remove entry from lcgMap
                    LcgMap::iterator lt = lcgMap_.begin();
                    for (; lt != lcgMap_.end(); )
                    {
                        if (lt->second.first == cid)
                        {
                            lcgMap_.erase(lt++);
                        }
                        else
                        {
                            ++lt;
                        }
                    }
                }
                EV << NOW << " LteMacUeRealisticD2D::macHandleD2DModeSwitch - send switch signal to the RLC TX entity corresponding to the old mode, cid " << cid << endl;
            }
            else if (lteInfo->getD2dRxPeerId() == peerId && (Direction)lteInfo->getDirection() == newDirection)
            {
                EV << NOW << " LteMacUeRealisticD2D::macHandleD2DModeSwitch - send switch signal to the RLC TX entity corresponding to the new mode, cid " << cid << endl;
                if (oldDirection != newDirection)
                {
                    D2DModeSwitchNotification* switchPkt_dup = switchPkt->dup();
                    switchPkt_dup->setOldConnection(false);
                    switchPkt_dup->setControlInfo(lteInfo->dup());
                    sendUpperPackets(switchPkt_dup);
                }
            }
        }
    }
    else   // rx side
    {
        Direction newDirection = (newMode == DM) ? D2D : DL;
        Direction oldDirection = (oldMode == DM) ? D2D : DL;

        // find the correct connection involved in the mode switch
        MacCid cid;
        FlowControlInfo* lteInfo = NULL;
        std::map<MacCid, FlowControlInfo>::iterator it = connDescIn_.begin();
        for (; it != connDescIn_.end(); ++it)
        {
            cid = it->first;
            lteInfo = check_and_cast<FlowControlInfo*>(&(it->second));
            if (lteInfo->getD2dTxPeerId() == peerId && (Direction)lteInfo->getDirection() == oldDirection)
            {
                EV << NOW << " LteMacUeRealisticD2D::macHandleD2DModeSwitch - found old connection with cid " << cid << ", send signal to the RLC RX entity" << endl;
                if (oldDirection != newDirection)
                {
                    // interrupt H-ARQ processes for SL
                    unsigned int id = peerId;
                    HarqRxBuffers::iterator hit = harqRxBuffers_.find(id);
                    if (hit != harqRxBuffers_.end())
                    {
                        for (unsigned int proc = 0; proc < (unsigned int) UE_RX_HARQ_PROCESSES; proc++)
                        {
                            unsigned int numUnits = hit->second->getProcess(proc)->getNumHarqUnits();
                            for (unsigned int i=0; i < numUnits; i++)
                            {

                                hit->second->getProcess(proc)->purgeCorruptedPdu(i); // delete contained PDU
                                hit->second->getProcess(proc)->resetCodeword(i);     // reset unit
                            }
                        }
                    }
                    enb_->deleteRxHarqBufferMirror(nodeId_);

                    // notify that this UE is switching during this TTI
                    resetHarq_[peerId] = NOW;

                    D2DModeSwitchNotification* switchPkt_dup = switchPkt->dup();
                    switchPkt_dup->setControlInfo(lteInfo->dup());
                    switchPkt_dup->setOldConnection(true);
                    sendUpperPackets(switchPkt_dup);
                }
            }
            else if (lteInfo->getD2dTxPeerId() == peerId && (Direction)lteInfo->getDirection() == newDirection)
            {
                EV << NOW << " LteMacUeRealisticD2D::macHandleD2DModeSwitch - found new connection with cid " << cid << ", send signal to the RLC RX entity" << endl;
                if (oldDirection != newDirection)
                {
                    D2DModeSwitchNotification* switchPkt_dup = switchPkt->dup();
                    switchPkt_dup->setOldConnection(false);
                    switchPkt_dup->setControlInfo(lteInfo->dup());
                    sendUpperPackets(switchPkt_dup);
                }
            }
        }
    }
    delete uInfo;
    delete pkt;
}


void LteMacVueV2X::doHandover(MacNodeId targetEnb)
{
    enb_ = check_and_cast<LteMacEnbRealisticD2D*>(getMacByMacNodeId(targetEnb));
    LteMacUeRealistic::doHandover(targetEnb);
}
