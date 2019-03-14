//
// Generated file, do not edit! Created by nedtool 5.4 from stack/phy/packet/CsrMessage.msg.
//

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif
#ifndef __CSRMESSAGE_M_H
#define __CSRMESSAGE_M_H

#include <omnetpp.h>

// nedtool version check
#define MSGC_VERSION 0x0504
#if (MSGC_VERSION!=OMNETPP_VERSION)
#    error Version mismatch! Probably this file was generated by an earlier version of nedtool: 'make clean' should help.
#endif



// cplusplus {{
    #include "stack/subchannel/Subchannel.h"
// }}

/**
 * Class generated from <tt>stack/phy/packet/CsrMessage.msg:20</tt> by nedtool.
 * <pre>
 * packet CsrMessage
 * {
 *     \@customize(true);
 * }
 * </pre>
 *
 * CsrMessage_Base is only useful if it gets subclassed, and CsrMessage is derived from it.
 * The minimum code to be written for CsrMessage is the following:
 *
 * <pre>
 * class CsrMessage : public CsrMessage_Base
 * {
 *   private:
 *     void copy(const CsrMessage& other) { ... }

 *   public:
 *     CsrMessage(const char *name=nullptr, short kind=0) : CsrMessage_Base(name,kind) {}
 *     CsrMessage(const CsrMessage& other) : CsrMessage_Base(other) {copy(other);}
 *     CsrMessage& operator=(const CsrMessage& other) {if (this==&other) return *this; CsrMessage_Base::operator=(other); copy(other); return *this;}
 *     virtual CsrMessage *dup() const override {return new CsrMessage(*this);}
 *     // ADD CODE HERE to redefine and implement pure virtual functions from CsrMessage_Base
 * };
 * </pre>
 *
 * The following should go into a .cc (.cpp) file:
 *
 * <pre>
 * Register_Class(CsrMessage)
 * </pre>
 */
class CsrMessage_Base : public ::omnetpp::cPacket
{
  protected:

  private:
    void copy(const CsrMessage_Base& other);

  protected:
    // protected and unimplemented operator==(), to prevent accidental usage
    bool operator==(const CsrMessage_Base&);
    // make constructors protected to avoid instantiation
    CsrMessage_Base(const char *name=nullptr, short kind=0);
    CsrMessage_Base(const CsrMessage_Base& other);
    // make assignment operator protected to force the user override it
    CsrMessage_Base& operator=(const CsrMessage_Base& other);

  public:
    virtual ~CsrMessage_Base();
    virtual CsrMessage_Base *dup() const override {throw omnetpp::cRuntimeError("You forgot to manually add a dup() function to class CsrMessage");}
    virtual void parsimPack(omnetpp::cCommBuffer *b) const override;
    virtual void parsimUnpack(omnetpp::cCommBuffer *b) override;

    // field getter/setter methods
};


#endif // ifndef __CSRMESSAGE_M_H