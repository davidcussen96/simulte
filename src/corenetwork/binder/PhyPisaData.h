//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _LTE_PHYPISADATA_H_
#define _LTE_PHYPISADATA_H_

#include <string.h>
#include <vector>

using namespace omnetpp;

const uint16_t XTABLE_SIZE = 3;
const uint16_t PUSCH_AWGN_SIZE = 33; //!<number of BLER values per HARQ transmission

struct TbErrorStats_t
{
  double tbler;   //!< block error rate
  double sinr;   //!< SINR value
};

class PhyPisaData
{
    double lambdaTable_[10000][3];
    double blerCurves_[3][15][49];
    std::vector<double> channel_;
    public:
    PhyPisaData();
    virtual ~PhyPisaData();
    // TODO get bler needs to be modified to use MCS.

    /**
       * List of channel models. Note that not all models are available for each physical channel
       */
    enum LteFadingModel
    {
        AWGN
    };

    //double getBler(int i, int j, int k){if (j==0) return 1; else return blerCurves_[i][j][k-1];}
    TbErrorStats_t PhyPisaData::GetBler (const double (*xtable)[XTABLE_SIZE], const double (*ytable),
            const uint16_t ysize, uint16_t mcs, uint8_t harq=1, double prevSinr, double newSinr);
    double getLambda(int i, int j){return lambdaTable_[i][j];}
    int nTxMode(){return 3;}
    int nMcs(){return 15;}
    int maxSnr(){return 49;}
    int maxChannel(){return 10000;}
    int maxChannel2(){return 1000;}
    double getChannel(unsigned int i);
    double GetSinrValue (const double (*xtable)[XTABLE_SIZE], const double (*ytable), const uint16_t ysize, uint16_t mcs, uint8_t harq, double bler);
    double GetBlerValue (const double (*xtable)[XTABLE_SIZE], const double (*ytable), const uint16_t ysize, uint16_t mcs, uint8_t harq, double sinr);
    int16_t GetColIndex (double val, double min, double max, double step);
    int16_t GetRowIndex (uint16_t mcs, uint8_t harq);
    TbErrorStats_t GetPuschBler (LteFadingModel fadingChannel, LteTxMode txmode=SISO,
            uint16_t mcs, double sinr, HarqProcessInfoList_t harqHistory);
};

#endif
