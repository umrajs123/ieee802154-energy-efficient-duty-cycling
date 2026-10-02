package inet.linklayer.ieee802154;

import inet.linklayer.contract.IWirelessInterface;
import inet.networklayer.common.NetworkInterface;
import inet.physicallayer.wireless.common.contract.packetlevel.IRadio;
import inet.queueing.contract.IPacketQueue;
import inet.linklayer.contract.IMacProtocol;

module EnergyEfficientIeee802154Interface extends NetworkInterface like IWirelessInterface
{
    parameters:
        @class(NetworkInterface);
        string interfaceTableModule;
        string energySourceModule = default("");
        *.interfaceTableModule = default(absPath(this.interfaceTableModule));
        *.energySourceModule = default(absPath(this.energySourceModule));

    gates:
        input upperLayerIn;
        output upperLayerOut;
        input radioIn @labels(IWirelessSignal);

    submodules:
        queue: <default("DropTailQueue")> like IPacketQueue {
            parameters:
                packetCapacity = default(100);
                @display("p=200,100;q=l2queue");
        }

        mac: <default("EnergyEfficientIeee802154Mac")> like IMacProtocol {
            parameters:
                @display("p=300,100");
        }

        radio: <default("Ieee802154NarrowbandRadio")> like IRadio {
            parameters:
                @display("p=300,200");
        }

    connections:
        upperLayerIn --> { @display("m=n"); } --> queue.in;
        queue.out --> mac.upperLayerIn;
        mac.lowerLayerOut --> radio.upperLayerIn;

        radioIn --> { @display("m=s"); } --> radio.radioIn;
        radio.upperLayerOut --> mac.lowerLayerIn;
        mac.upperLayerOut --> { @display("m=n"); } --> upperLayerOut;
}
