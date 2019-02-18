//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//
cplusplus {{
    #include "stack/mac/packet/LteSchedulingGrant_m.h"
    #include "stack/subchannel/Subchannel.h"
}}

packet LteSchedulingGrantMode4 extends LteSchedulingGrant
{
    @customize(true);  // see the generated C++ header for more info
    bool periodic;
       unsigned int period;
       unsigned int expiration;
       // blocks granted on all Remotes, all Bands
    unsigned int totalGrantedBlocks;
    // number of codewords
    unsigned int codewords;
    // granted bytes per codeword
    abstract unsigned int grantedCwBytes[];

    //#  in LteSchedulingGrant.h
    //# const UserTxParams* userTxParams;
    //# RbMap grantedBlocks;
    //#
    
    unsigned int priority;
    unsigned int resourceRes;
    unsigned int freqResource;
    unsigned int timeGap;
    unsigned int mcs;
    unsigned int retransIndex;
    unsigned int reservedInfo;
    Subchannel* csr;
}
