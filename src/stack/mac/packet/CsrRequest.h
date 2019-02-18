/*
 * CsrRequest.h
 *
 *  Created on: Feb 18, 2019
 *      Author: djc10
 */

#ifndef STACK_MAC_PACKET_CSRREQUEST_H_
#define STACK_MAC_PACKET_CSRREQUEST_H_

class CsrRequest : public cMessage
{
protected:
    int cResel;
    int prioTx;
    int pRsvpTx;

public:
    void setCResel(int c) {cResel = c;}
    int getCResel() {return cResel;}

    void setPrioTx(int p) {prioTx = c;}
    int getPrioTx() {return prioTx;}

    void setPRsvpTx(int r) {pRsvpTx = c;}
    int getPRsvpTx() {return pRsvpTx;}

};





#endif /* STACK_MAC_PACKET_CSRREQUEST_H_ */
