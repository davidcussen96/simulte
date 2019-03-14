//
// Generated file, do not edit! Created by nedtool 5.4 from stack/sci/Sci.msg.
//

// Disable warnings about unused variables, empty switch stmts, etc:
#ifdef _MSC_VER
#  pragma warning(disable:4101)
#  pragma warning(disable:4065)
#endif

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wc++98-compat"
#  pragma clang diagnostic ignored "-Wunreachable-code-break"
#  pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wshadow"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include <iostream>
#include <sstream>
#include "Sci_m.h"

namespace omnetpp {

// Template pack/unpack rules. They are declared *after* a1l type-specific pack functions for multiple reasons.
// They are in the omnetpp namespace, to allow them to be found by argument-dependent lookup via the cCommBuffer argument

// Packing/unpacking an std::vector
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::vector<T,A>& v)
{
    int n = v.size();
    doParsimPacking(buffer, n);
    for (int i = 0; i < n; i++)
        doParsimPacking(buffer, v[i]);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::vector<T,A>& v)
{
    int n;
    doParsimUnpacking(buffer, n);
    v.resize(n);
    for (int i = 0; i < n; i++)
        doParsimUnpacking(buffer, v[i]);
}

// Packing/unpacking an std::list
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::list<T,A>& l)
{
    doParsimPacking(buffer, (int)l.size());
    for (typename std::list<T,A>::const_iterator it = l.begin(); it != l.end(); ++it)
        doParsimPacking(buffer, (T&)*it);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::list<T,A>& l)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i=0; i<n; i++) {
        l.push_back(T());
        doParsimUnpacking(buffer, l.back());
    }
}

// Packing/unpacking an std::set
template<typename T, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::set<T,Tr,A>& s)
{
    doParsimPacking(buffer, (int)s.size());
    for (typename std::set<T,Tr,A>::const_iterator it = s.begin(); it != s.end(); ++it)
        doParsimPacking(buffer, *it);
}

template<typename T, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::set<T,Tr,A>& s)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i=0; i<n; i++) {
        T x;
        doParsimUnpacking(buffer, x);
        s.insert(x);
    }
}

// Packing/unpacking an std::map
template<typename K, typename V, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::map<K,V,Tr,A>& m)
{
    doParsimPacking(buffer, (int)m.size());
    for (typename std::map<K,V,Tr,A>::const_iterator it = m.begin(); it != m.end(); ++it) {
        doParsimPacking(buffer, it->first);
        doParsimPacking(buffer, it->second);
    }
}

template<typename K, typename V, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::map<K,V,Tr,A>& m)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i=0; i<n; i++) {
        K k; V v;
        doParsimUnpacking(buffer, k);
        doParsimUnpacking(buffer, v);
        m[k] = v;
    }
}

// Default pack/unpack function for arrays
template<typename T>
void doParsimArrayPacking(omnetpp::cCommBuffer *b, const T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimPacking(b, t[i]);
}

template<typename T>
void doParsimArrayUnpacking(omnetpp::cCommBuffer *b, T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimUnpacking(b, t[i]);
}

// Default rule to prevent compiler from choosing base class' doParsimPacking() function
template<typename T>
void doParsimPacking(omnetpp::cCommBuffer *, const T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimPacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

template<typename T>
void doParsimUnpacking(omnetpp::cCommBuffer *, T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimUnpacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

}  // namespace omnetpp


// forward
template<typename T, typename A>
std::ostream& operator<<(std::ostream& out, const std::vector<T,A>& vec);

// Template rule which fires if a struct or class doesn't have operator<<
template<typename T>
inline std::ostream& operator<<(std::ostream& out,const T&) {return out;}

// operator<< for std::vector<T>
template<typename T, typename A>
inline std::ostream& operator<<(std::ostream& out, const std::vector<T,A>& vec)
{
    out.put('{');
    for(typename std::vector<T,A>::const_iterator it = vec.begin(); it != vec.end(); ++it)
    {
        if (it != vec.begin()) {
            out.put(','); out.put(' ');
        }
        out << *it;
    }
    out.put('}');
    
    char buf[32];
    sprintf(buf, " (size=%u)", (unsigned int)vec.size());
    out.write(buf, strlen(buf));
    return out;
}

Sci_Base::Sci_Base(const char *name, short kind) : ::omnetpp::cPacket(name,kind)
{
    this->priority = 0;
    this->resourceRes = 0;
    this->freqResource = 0;
    this->timeGap = 0;
    this->mcs = 0;
    this->retrans = 0;
    this->resInfo = 0;
}

Sci_Base::Sci_Base(const Sci_Base& other) : ::omnetpp::cPacket(other)
{
    copy(other);
}

Sci_Base::~Sci_Base()
{
}

Sci_Base& Sci_Base::operator=(const Sci_Base& other)
{
    if (this==&other) return *this;
    ::omnetpp::cPacket::operator=(other);
    copy(other);
    return *this;
}

void Sci_Base::copy(const Sci_Base& other)
{
    this->priority = other.priority;
    this->resourceRes = other.resourceRes;
    this->freqResource = other.freqResource;
    this->timeGap = other.timeGap;
    this->mcs = other.mcs;
    this->retrans = other.retrans;
    this->resInfo = other.resInfo;
}

void Sci_Base::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::omnetpp::cPacket::parsimPack(b);
    doParsimPacking(b,this->priority);
    doParsimPacking(b,this->resourceRes);
    doParsimPacking(b,this->freqResource);
    doParsimPacking(b,this->timeGap);
    doParsimPacking(b,this->mcs);
    doParsimPacking(b,this->retrans);
    doParsimPacking(b,this->resInfo);
}

void Sci_Base::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::omnetpp::cPacket::parsimUnpack(b);
    doParsimUnpacking(b,this->priority);
    doParsimUnpacking(b,this->resourceRes);
    doParsimUnpacking(b,this->freqResource);
    doParsimUnpacking(b,this->timeGap);
    doParsimUnpacking(b,this->mcs);
    doParsimUnpacking(b,this->retrans);
    doParsimUnpacking(b,this->resInfo);
}

unsigned int Sci_Base::getPriority() const
{
    return this->priority;
}

void Sci_Base::setPriority(unsigned int priority)
{
    this->priority = priority;
}

unsigned int Sci_Base::getResourceRes() const
{
    return this->resourceRes;
}

void Sci_Base::setResourceRes(unsigned int resourceRes)
{
    this->resourceRes = resourceRes;
}

unsigned int Sci_Base::getFreqResource() const
{
    return this->freqResource;
}

void Sci_Base::setFreqResource(unsigned int freqResource)
{
    this->freqResource = freqResource;
}

unsigned int Sci_Base::getTimeGap() const
{
    return this->timeGap;
}

void Sci_Base::setTimeGap(unsigned int timeGap)
{
    this->timeGap = timeGap;
}

unsigned int Sci_Base::getMcs() const
{
    return this->mcs;
}

void Sci_Base::setMcs(unsigned int mcs)
{
    this->mcs = mcs;
}

unsigned int Sci_Base::getRetrans() const
{
    return this->retrans;
}

void Sci_Base::setRetrans(unsigned int retrans)
{
    this->retrans = retrans;
}

unsigned int Sci_Base::getResInfo() const
{
    return this->resInfo;
}

void Sci_Base::setResInfo(unsigned int resInfo)
{
    this->resInfo = resInfo;
}

class SciDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertynames;
  public:
    SciDescriptor();
    virtual ~SciDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyname) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyname) const override;
    virtual int getFieldArraySize(void *object, int field) const override;

    virtual const char *getFieldDynamicTypeString(void *object, int field, int i) const override;
    virtual std::string getFieldValueAsString(void *object, int field, int i) const override;
    virtual bool setFieldValueAsString(void *object, int field, int i, const char *value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual void *getFieldStructValuePointer(void *object, int field, int i) const override;
};

Register_ClassDescriptor(SciDescriptor)

SciDescriptor::SciDescriptor() : omnetpp::cClassDescriptor("Sci", "omnetpp::cPacket")
{
    propertynames = nullptr;
}

SciDescriptor::~SciDescriptor()
{
    delete[] propertynames;
}

bool SciDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<Sci_Base *>(obj)!=nullptr;
}

const char **SciDescriptor::getPropertyNames() const
{
    if (!propertynames) {
        static const char *names[] = { "customize",  nullptr };
        omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
        const char **basenames = basedesc ? basedesc->getPropertyNames() : nullptr;
        propertynames = mergeLists(basenames, names);
    }
    return propertynames;
}

const char *SciDescriptor::getProperty(const char *propertyname) const
{
    if (!strcmp(propertyname,"customize")) return "true";
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? basedesc->getProperty(propertyname) : nullptr;
}

int SciDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? 7+basedesc->getFieldCount() : 7;
}

unsigned int SciDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeFlags(field);
        field -= basedesc->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
        FD_ISEDITABLE,
    };
    return (field>=0 && field<7) ? fieldTypeFlags[field] : 0;
}

const char *SciDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldName(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldNames[] = {
        "priority",
        "resourceRes",
        "freqResource",
        "timeGap",
        "mcs",
        "retrans",
        "resInfo",
    };
    return (field>=0 && field<7) ? fieldNames[field] : nullptr;
}

int SciDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    int base = basedesc ? basedesc->getFieldCount() : 0;
    if (fieldName[0]=='p' && strcmp(fieldName, "priority")==0) return base+0;
    if (fieldName[0]=='r' && strcmp(fieldName, "resourceRes")==0) return base+1;
    if (fieldName[0]=='f' && strcmp(fieldName, "freqResource")==0) return base+2;
    if (fieldName[0]=='t' && strcmp(fieldName, "timeGap")==0) return base+3;
    if (fieldName[0]=='m' && strcmp(fieldName, "mcs")==0) return base+4;
    if (fieldName[0]=='r' && strcmp(fieldName, "retrans")==0) return base+5;
    if (fieldName[0]=='r' && strcmp(fieldName, "resInfo")==0) return base+6;
    return basedesc ? basedesc->findField(fieldName) : -1;
}

const char *SciDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeString(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "unsigned int",
        "unsigned int",
        "unsigned int",
        "unsigned int",
        "unsigned int",
        "unsigned int",
        "unsigned int",
    };
    return (field>=0 && field<7) ? fieldTypeStrings[field] : nullptr;
}

const char **SciDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldPropertyNames(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *SciDescriptor::getFieldProperty(int field, const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldProperty(field, propertyname);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int SciDescriptor::getFieldArraySize(void *object, int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldArraySize(object, field);
        field -= basedesc->getFieldCount();
    }
    Sci_Base *pp = (Sci_Base *)object; (void)pp;
    switch (field) {
        default: return 0;
    }
}

const char *SciDescriptor::getFieldDynamicTypeString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldDynamicTypeString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    Sci_Base *pp = (Sci_Base *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string SciDescriptor::getFieldValueAsString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldValueAsString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    Sci_Base *pp = (Sci_Base *)object; (void)pp;
    switch (field) {
        case 0: return ulong2string(pp->getPriority());
        case 1: return ulong2string(pp->getResourceRes());
        case 2: return ulong2string(pp->getFreqResource());
        case 3: return ulong2string(pp->getTimeGap());
        case 4: return ulong2string(pp->getMcs());
        case 5: return ulong2string(pp->getRetrans());
        case 6: return ulong2string(pp->getResInfo());
        default: return "";
    }
}

bool SciDescriptor::setFieldValueAsString(void *object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->setFieldValueAsString(object,field,i,value);
        field -= basedesc->getFieldCount();
    }
    Sci_Base *pp = (Sci_Base *)object; (void)pp;
    switch (field) {
        case 0: pp->setPriority(string2ulong(value)); return true;
        case 1: pp->setResourceRes(string2ulong(value)); return true;
        case 2: pp->setFreqResource(string2ulong(value)); return true;
        case 3: pp->setTimeGap(string2ulong(value)); return true;
        case 4: pp->setMcs(string2ulong(value)); return true;
        case 5: pp->setRetrans(string2ulong(value)); return true;
        case 6: pp->setResInfo(string2ulong(value)); return true;
        default: return false;
    }
}

const char *SciDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructName(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

void *SciDescriptor::getFieldStructValuePointer(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructValuePointer(object, field, i);
        field -= basedesc->getFieldCount();
    }
    Sci_Base *pp = (Sci_Base *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

