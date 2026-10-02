package inet.linklayer.ieee802154;

module EnergyEfficientIeee802154Mac extends Ieee802154NarrowbandMac
{
    parameters:
        @class(EnergyEfficientIeee802154Mac);

        int beaconOrder = default(6);
        int superframeOrder = default(3);
        double beaconGuardTime @unit(s) = default(1ms);

        bool coordinator = default(false);
}
