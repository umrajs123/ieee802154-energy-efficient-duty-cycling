#ifndef __INET_ENERGYEFFICIENTBEACON_H
#define __INET_ENERGYEFFICIENTBEACON_H

#include "inet/common/packet/chunk/FieldsChunk.h"

namespace inet {

class EnergyEfficientBeacon : public FieldsChunk
{
  protected:
    int beaconOrder = 6;
    int superframeOrder = 3;
    int sequenceNumber = 0;

  public:
    EnergyEfficientBeacon()
    {
        setChunkLength(B(4));
    }

    EnergyEfficientBeacon(const EnergyEfficientBeacon& other) :
        FieldsChunk(other),
        beaconOrder(other.beaconOrder),
        superframeOrder(other.superframeOrder),
        sequenceNumber(other.sequenceNumber)
    {
    }

    virtual EnergyEfficientBeacon *dup() const override
    {
        return new EnergyEfficientBeacon(*this);
    }

    int getBeaconOrder() const
    {
        return beaconOrder;
    }

    void setBeaconOrder(int value)
    {
        beaconOrder = value;
    }

    int getSuperframeOrder() const
    {
        return superframeOrder;
    }

    void setSuperframeOrder(int value)
    {
        superframeOrder = value;
    }

    int getSequenceNumber() const
    {
        return sequenceNumber;
    }

    void setSequenceNumber(int value)
    {
        sequenceNumber = value;
    }
};

} // namespace inet

#endif
