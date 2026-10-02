#ifndef __INET_ENERGYEFFICIENTIEEE802154MAC_H
#define __INET_ENERGYEFFICIENTIEEE802154MAC_H

#include "inet/linklayer/ieee802154/Ieee802154Mac.h"
#include "inet/linklayer/ieee802154/EnergyEfficientBeacon.h"

namespace inet {

class INET_API EnergyEfficientIeee802154Mac : public Ieee802154Mac
{
  protected:
    int beaconOrder = 6;
    int superframeOrder = 3;
    bool coordinator = false;
    simtime_t beaconGuardTime;

    simtime_t baseSuperframeDuration;
    simtime_t beaconInterval;
    simtime_t activeDuration;
    simtime_t inactiveDuration;

    int beaconSequenceNumber = 0;

    cMessage *beaconTimer = nullptr;
    cMessage *inactiveTimer = nullptr;
    cMessage *wakeTimer = nullptr;
    cMessage *deferredBeaconTimer = nullptr;

    bool synchronized = false;
    bool superframeActive = true;
    bool radioSleeping = false;

    bool beaconPending = false;
    bool beaconTransmitting = false;
    bool resumeAfterBeacon = false;
    bool sleepPending = false;

    virtual void initialize(int stage) override;
    virtual void finish() override;
    virtual void handleSelfMessage(cMessage *msg) override;
    virtual void handleLowerPacket(Packet *packet) override;
    virtual void handleCanPullPacketChanged(const cGate *gate) override;

    virtual void receiveSignal(cComponent *source, simsignal_t signalID,
                               intval_t value, cObject *details) override;

    virtual void sendBeacon();
    virtual void enterInactivePeriod();
    virtual void enterActivePeriod();
    virtual void enterSleep();
    virtual void wakeUp();

    virtual void synchronizeToBeacon(int receivedBeaconOrder,
                                     int receivedSuperframeOrder);

    virtual void deferBeacon();
};

} // namespace inet

#endif
