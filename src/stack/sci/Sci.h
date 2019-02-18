/*
 * Sci.h
 *
 *  Created on: Feb 18, 2019
 *      Author: djc10
 */

#ifndef STACK_SCI_SCI_H_
#define STACK_SCI_SCI_H_

class Sci
{
private:
    unsigned int priority;
    unsigned int resourceRes;
    unsigned int freqResource;
    unsigned int timeGap;
    unsigned int mcs;
    unsigned int retransIndex;

public:
    Sci(unsigned int pri, unsigned int rr) {
        priority = pri;
        resourceRes = rr;
    }
};





#endif /* STACK_SCI_SCI_H_ */
