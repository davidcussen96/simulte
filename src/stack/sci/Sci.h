/*
 * Sci.h
 *
 *  Created on: Feb 18, 2019
 *      Author: djc10
 */
#pragma once

#include "stack/sci/Sci_m.h"

class Sci : public Sci_Base
{

public:
    Sci(unsigned int pri, unsigned int rr) {
        priority = pri;
        resourceRes = rr;
    }

    Sci();
};

