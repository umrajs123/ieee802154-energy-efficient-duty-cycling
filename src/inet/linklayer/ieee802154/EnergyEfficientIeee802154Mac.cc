#include "inet/linklayer/ieee802154/EnergyEfficientIeee802154Mac.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"

#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/packet/chunk/FieldsChunk.h"
#include "inet/linklayer/ieee802154/Ieee802154MacHeader_m.h"

namespace inet {

Define_Module(EnergyEfficientIeee802154Mac);

void EnergyEfficientIeee802154Mac::initialize(int stage)
{
    Ieee802154Mac::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        beaconOrder = par("beaconOrder");
        superframeOrder = par("superframeOrder");
        coordinator = par("coordinator");
        beaconGuardTime = par("beaconGuardTime");

        if (beaconOrder < 0 || beaconOrder > 14)
            throw cRuntimeError("beaconOrder must be between 0 and 14");

        if (superframeOrder < 0 || superframeOrder > beaconOrder)
            throw cRuntimeError(
                "superframeOrder must satisfy 0 <= SO <= BO");

        /*
         * IEEE 802.15.4:
         *
         * aBaseSuperframeDuration = 960 symbols
         * At 2.4 GHz:
         * symbol duration = 16 us
         * therefore:
         * 960 * 16 us = 15.36 ms
         */
        baseSuperframeDuration = SimTime(0.01536);

        beaconInterval =
            baseSuperframeDuration * (1 << beaconOrder);

        activeDuration =
            baseSuperframeDuration * (1 << superframeOrder);

        inactiveDuration =
            beaconInterval - activeDuration;

        beaconTimer = new cMessage("beaconTimer");
        inactiveTimer = new cMessage("inactiveTimer");
        wakeTimer = new cMessage("wakeTimer");
        deferredBeaconTimer = new cMessage("deferredBeaconTimer");

        synchronized = coordinator;
        superframeActive = true;
        radioSleeping = false;
        beaconPending = false;
        beaconTransmitting = false;
        resumeAfterBeacon = false;
        sleepPending = false;

        /*
         * Coordinator starts the beacon schedule.
         *
         * End devices start awake so that they can receive
         * the first coordinator beacon and synchronize.
         */
        if (coordinator)
            scheduleAt(simTime() + SimTime(0.000001), beaconTimer);
    }
}

void EnergyEfficientIeee802154Mac::finish()
{
    if (beaconTimer != nullptr) {
        cancelAndDelete(beaconTimer);
        beaconTimer = nullptr;
    }

    if (inactiveTimer != nullptr) {
        cancelAndDelete(inactiveTimer);
        inactiveTimer = nullptr;
    }

    if (wakeTimer != nullptr) {
        cancelAndDelete(wakeTimer);
        wakeTimer = nullptr;
    }

    if (deferredBeaconTimer != nullptr) {
        cancelAndDelete(deferredBeaconTimer);
        deferredBeaconTimer = nullptr;
    }

    Ieee802154Mac::finish();
}

void EnergyEfficientIeee802154Mac::handleSelfMessage(cMessage *msg)
{
    /*
     * Coordinator beacon timer.
     */
    if (msg == beaconTimer) {
        if (coordinator)
            sendBeacon();

        return;
    }

    /*
     * Deferred beacon transmission.
     */
    if (msg == deferredBeaconTimer) {
        if (beaconPending)
            sendBeacon();

        return;
    }

    /*
     * End of active period.
     */
    if (msg == inactiveTimer) {
        enterInactivePeriod();
        return;
    }

    /*
     * Beginning of next active period.
     */
    if (msg == wakeTimer) {
        enterActivePeriod();
        return;
    }

    /*
     * Normal IEEE 802.15.4 MAC timers.
     */
    Ieee802154Mac::handleSelfMessage(msg);
}

void EnergyEfficientIeee802154Mac::handleCanPullPacketChanged(
        const cGate *gate)
{
    Enter_Method("handleCanPullPacketChanged");

    if (gate->getId() != upperLayerInGateId)
        return;

    /*
     * Do not start transmission while the radio is sleeping
     * or the superframe is inactive.
     */
    if (radioSleeping || !superframeActive)
        return;

    /*
     * End devices must synchronize before transmitting.
     */
    if (!coordinator && !synchronized)
        return;

    /*
     * Do not compete with a beacon transmission.
     */
    if (beaconTransmitting)
        return;

    executeMac(EV_SEND_REQUEST, nullptr);
}

void EnergyEfficientIeee802154Mac::sendBeacon()
{
    if (!coordinator)
        return;

    /*
     * Ensure that the coordinator is awake before
     * transmitting the beacon.
     */
    if (radioSleeping || !superframeActive) {
        radioSleeping = false;
        superframeActive = true;

        if (wakeTimer->isScheduled())
            cancelEvent(wakeTimer);

        radio->setRadioMode(
            physicallayer::IRadio::RADIO_MODE_TRANSCEIVER);
    }

    /*
     * Do not inject a beacon into an ongoing reception.
     */
    if (radio->getReceptionState() !=
        physicallayer::IRadio::RECEPTION_STATE_IDLE) {

        deferBeacon();
        return;
    }

    /*
     * Do not start another beacon while the previous
     * beacon is still transmitting.
     */
    if (beaconTransmitting) {
        beaconPending = true;

        if (!deferredBeaconTimer->isScheduled())
            scheduleAt(
                simTime() + beaconGuardTime,
                deferredBeaconTimer);

        return;
    }

    /*
     * Never interrupt an ordinary MAC transmission or ACK.
     */
    if (macState == TRANSMITFRAME_4 ||
        macState == WAITACK_5 ||
        macState == WAITSIFS_6 ||
        macState == TRANSMITACK_7 ||
        radio->getTransmissionState() ==
            physicallayer::IRadio::TRANSMISSION_STATE_TRANSMITTING) {

        beaconPending = true;

        if (!deferredBeaconTimer->isScheduled())
            scheduleAt(
                simTime() + beaconGuardTime,
                deferredBeaconTimer);

        return;
    }

    /*
     * Cancel a pending CSMA attempt so that the beacon
     * can occupy the beginning-of-superframe opportunity.
     */
    if (macState == BACKOFF_2 &&
        backoffTimer->isScheduled()) {

        cancelEvent(backoffTimer);
    }

    if (macState == CCA_3 &&
        ccaTimer->isScheduled()) {

        cancelEvent(ccaTimer);
    }

    /*
     * If the MAC already owns a normal frame, wait.
     */
    if (currentTxFrame != nullptr) {
        beaconPending = true;

        if (!deferredBeaconTimer->isScheduled())
            scheduleAt(
                simTime() + beaconGuardTime,
                deferredBeaconTimer);

        return;
    }

    updateMacState(IDLE_1);

    /*
     * Make sure the radio can transmit.
     */
    radio->setRadioMode(
        physicallayer::IRadio::RADIO_MODE_TRANSMITTER);

    auto header = makeShared<Ieee802154MacHeader>();

    header->setChunkLength(b(72));
    header->setSrcAddr(networkInterface->getMacAddress());
    header->setDestAddr(MacAddress::BROADCAST_ADDRESS);

    const int sequenceNumber = beaconSequenceNumber++;

    header->setSequenceId(sequenceNumber);

    auto beacon = makeShared<EnergyEfficientBeacon>();

    beacon->setBeaconOrder(beaconOrder);
    beacon->setSuperframeOrder(superframeOrder);
    beacon->setSequenceNumber(sequenceNumber);

    auto packet = new Packet("EnergyEfficientBeacon");

    packet->addTagIfAbsent<PacketProtocolTag>()
        ->setProtocol(&Protocol::ieee802154);

    packet->insertAtBack(beacon);
    packet->insertAtFront(header);

    attachSignal(
        packet,
        simTime() + aTurnaroundTime);

    beaconTransmitting = true;
    beaconPending = false;

    EV_INFO
        << "BEACON SEND: radioMode="
        << radio->getRadioMode()
        << ", txState="
        << radio->getTransmissionState()
        << ", rxState="
        << radio->getReceptionState()
        << ", t="
        << simTime()
        << endl;

    sendDelayed(
        packet,
        aTurnaroundTime,
        lowerLayerOutGateId);

    /*
     * Active period begins at the beacon.
     */
    superframeActive = true;
    radioSleeping = false;

    if (inactiveTimer->isScheduled())
        cancelEvent(inactiveTimer);

    scheduleAt(
        simTime() + activeDuration,
        inactiveTimer);

    /*
     * Re-anchor the beacon schedule to the actual
     * beacon transmission time.
     */
    if (beaconTimer->isScheduled())
        cancelEvent(beaconTimer);

    scheduleAt(
        simTime() + beaconInterval,
        beaconTimer);

    EV_INFO
        << "Energy-efficient beacon transmitted at "
        << simTime()
        << ", BI="
        << beaconInterval
        << ", SD="
        << activeDuration
        << ", inactive="
        << inactiveDuration
        << endl;
}

void EnergyEfficientIeee802154Mac::receiveSignal(
        cComponent *source,
        simsignal_t signalID,
        intval_t value,
        cObject *details)
{
    Enter_Method(
        "%s",
        cComponent::getSignalName(signalID));

    if (signalID ==
        physicallayer::IRadio::transmissionStateChangedSignal) {

        auto newState =
            static_cast<
                physicallayer::IRadio::TransmissionState>(value);

        /*
         * Handle completion of our beacon transmission.
         */
        if (beaconTransmitting) {

            if (newState ==
                physicallayer::IRadio::TRANSMISSION_STATE_IDLE) {

                beaconTransmitting = false;
                transmissionState = newState;

                /*
                 * Coordinator remains able to receive immediately
                 * after its beacon transmission.
                 */
                radio->setRadioMode(
                    physicallayer::IRadio::RADIO_MODE_TRANSCEIVER);

                /*
                 * If the active period ended while the beacon
                 * was transmitting, enter inactive period now.
                 */
                if (sleepPending) {
                    sleepPending = false;
                    enterInactivePeriod();
                    return;
                }

                /*
                 * A later beacon request was deferred.
                 */
                if (beaconPending) {

                    if (!deferredBeaconTimer->isScheduled()) {
                        scheduleAt(
                            simTime() + SimTime(0.000001),
                            deferredBeaconTimer);
                    }

                    return;
                }

                /*
                 * Resume queued data transmission.
                 */
                if (resumeAfterBeacon) {

                    resumeAfterBeacon = false;

                    if (superframeActive &&
                        !radioSleeping) {

                        handleCanPullPacketChanged(
                            gate("upperLayerIn"));
                    }
                }
            }

            transmissionState = newState;
        }
    }

    /*
     * Let the parent MAC complete its normal transmission FSM.
     */
    Ieee802154Mac::receiveSignal(
        source,
        signalID,
        value,
        details);

    /*
     * If a sleep request was pending and the transmission
     * has now completed, consume the request.
     */
    if (signalID ==
        physicallayer::IRadio::transmissionStateChangedSignal &&
        sleepPending &&
        static_cast<
            physicallayer::IRadio::TransmissionState>(value) ==
            physicallayer::IRadio::TRANSMISSION_STATE_IDLE &&
        !beaconTransmitting) {

        enterInactivePeriod();
    }
}

void EnergyEfficientIeee802154Mac::handleLowerPacket(
        Packet *packet)
{
    /*
     * Detect our synchronization beacon before the parent
     * IEEE 802.15.4 MAC interprets it as a normal frame.
     */
    if (packet->getName() ==
            std::string("EnergyEfficientBeacon") &&
        packet->getTotalLength() >= b(72 + 32)) {

        const auto& beacon =
            packet->peekAtBack<EnergyEfficientBeacon>(B(4));

        int receivedBO =
            beacon->getBeaconOrder();

        int receivedSO =
            beacon->getSuperframeOrder();

        synchronizeToBeacon(
            receivedBO,
            receivedSO);

        delete packet;
        return;
    }

    /*
     * Ignore incoming frames while sleeping.
     */
    if (radioSleeping) {
        delete packet;
        return;
    }

    /*
     * Normal IEEE 802.15.4 traffic.
     */
    Ieee802154Mac::handleLowerPacket(packet);
}

void EnergyEfficientIeee802154Mac::synchronizeToBeacon(
        int receivedBeaconOrder,
        int receivedSuperframeOrder)
{
    beaconOrder = receivedBeaconOrder;
    superframeOrder = receivedSuperframeOrder;

    beaconInterval =
        baseSuperframeDuration *
        (1 << beaconOrder);

    activeDuration =
        baseSuperframeDuration *
        (1 << superframeOrder);

    inactiveDuration =
        beaconInterval -
        activeDuration;

    synchronized = true;
    superframeActive = true;
    radioSleeping = false;

    /*
     * Wake into normal receiver operation.
     */
    radio->setRadioMode(
        physicallayer::IRadio::RADIO_MODE_RECEIVER);

    if (inactiveTimer->isScheduled())
        cancelEvent(inactiveTimer);

    if (wakeTimer->isScheduled())
        cancelEvent(wakeTimer);

    /*
     * Remain active for SD.
     */
    scheduleAt(
        simTime() + activeDuration,
        inactiveTimer);
    /*
     * Allow queued data to enter the parent MAC.
     */
    if (!coordinator && !beaconTransmitting)
        handleCanPullPacketChanged(
            gate("upperLayerIn"));

    EV_INFO
        << "Synchronized to beacon at "
        << simTime()
        << ", BI="
        << beaconInterval
        << ", active="
        << activeDuration
        << ", inactive="
        << inactiveDuration
        << endl;
}

void EnergyEfficientIeee802154Mac::enterInactivePeriod()
{
    /*
     * ============================================================
     * COORDINATOR
     * ============================================================
     *
     * The coordinator remains awake during the inactive period.
     * Duty cycling is therefore applied only to end devices.
     *
     * The coordinator is placed in RECEIVER mode so that it can
     * safely receive traffic associated with the next active
     * period and cannot accidentally enter SLEEP while the
     * inherited IEEE 802.15.4 MAC is still active.
     */
    if (coordinator) {
        superframeActive = false;
        sleepPending = false;
        radioSleeping = false;

        radio->setRadioMode(
            physicallayer::IRadio::RADIO_MODE_TRANSCEIVER);

        if (wakeTimer->isScheduled())
            cancelEvent(wakeTimer);

        return;
    }

    /*
     * ============================================================
     * END DEVICE
     * ============================================================
     *
     * Never interrupt an ongoing transmission, ACK exchange,
     * or beacon processing.
     */
    if (beaconTransmitting ||
        radio->getTransmissionState() ==
            physicallayer::IRadio::TRANSMISSION_STATE_TRANSMITTING ||
        macState == TRANSMITFRAME_4 ||
        macState == WAITACK_5 ||
        macState == WAITSIFS_6 ||
        macState == TRANSMITACK_7) {

        sleepPending = true;
        return;
    }

    sleepPending = false;

    /*
     * Cancel pending CSMA/CCA activity before sleeping.
     */
    if (macState == BACKOFF_2 &&
        backoffTimer->isScheduled()) {

        cancelEvent(backoffTimer);
    }

    if (macState == CCA_3 &&
        ccaTimer->isScheduled()) {

        cancelEvent(ccaTimer);
    }

    updateMacState(IDLE_1);

    superframeActive = false;
    synchronized = false;

    enterSleep();

    if (wakeTimer->isScheduled())
        cancelEvent(wakeTimer);

    /*
     * Wake slightly before the expected next beacon.
     */
    simtime_t wakeLead =
        beaconGuardTime;

    if (wakeLead >= inactiveDuration)
        wakeLead =
            inactiveDuration / 2;

    scheduleAt(
        simTime() +
            inactiveDuration -
            wakeLead,
        wakeTimer);
}

void EnergyEfficientIeee802154Mac::enterActivePeriod()
{
    /*
     * Wake before the next beacon.
     *
     * The beacon itself marks the beginning of the
     * next active superframe.
     */
    wakeUp();

    /*
     * Do not permit normal transmission until the
     * beacon has been received.
     */
    superframeActive = false;

    if (inactiveTimer->isScheduled())
        cancelEvent(inactiveTimer);
}

void EnergyEfficientIeee802154Mac::enterSleep()
{
    if (beaconTransmitting)
        return;

    radioSleeping = true;

    radio->setRadioMode(
        physicallayer::IRadio::RADIO_MODE_SLEEP);

    EV_INFO
        << "Radio sleeping at "
        << simTime()
        << endl;
}

void EnergyEfficientIeee802154Mac::wakeUp()
{
    radioSleeping = false;

    radio->setRadioMode(
        physicallayer::IRadio::RADIO_MODE_RECEIVER);

    EV_INFO
        << "Radio woke at "
        << simTime()
        << " in RECEIVER mode"
        << endl;
}

void EnergyEfficientIeee802154Mac::deferBeacon()
{
    beaconPending = true;

    if (!deferredBeaconTimer->isScheduled()) {

        scheduleAt(
            simTime() + beaconGuardTime,
            deferredBeaconTimer);
    }
}

} // namespace inet
