# Energy-Efficient Duty Cycling for Beacon-Enabled IEEE 802.15.4

## Thesis Project

**An Energy Efficient Duty Cycling Based Technique for Beacon Enabled IEEE 802.15.4 MAC Protocols**

This repository contains the OMNeT++ simulation project, configuration files, experimental results, and analysis data used in the thesis study.

## Software

- OMNeT++ 6.4.0
- INET Framework 4.6.0
- IEEE 802.15.4 beacon-enabled MAC

## Simulation Setup

- 5 sensor nodes
- 8 lamp nodes
- 1 controller/coordinator
- Simulation duration: 100 seconds
- 10 independent runs per configuration
- Sensor traffic: 1 packet/second
- Packet size: 10 bytes

## Configurations

The experiments evaluate different Beacon Order (BO) and Superframe Order (SO) combinations:

- BO6/SO2
- BO6/SO4
- BO6/SO2 + CSMA
- BO6/SO4 + CSMA
- BO7/SO2
- BO8/SO2
- BO7/SO3
- BO8/SO3

## Evaluated Metrics

- Network-wide energy consumption
- Packet Delivery Ratio (PDR)
- Energy per successfully received packet
- Energy saving relative to BO6/SO2
- Theoretical end-device sleep duty cycle

## Results

The final simulation results are stored in:

`results100s/`

Each configuration contains 10 simulation runs.

## Reproducibility

The project configuration is provided in `omnetpp.ini`.

OMNeT++ and INET should be installed separately. The INET version used for this study is 4.6.0.

## Status

The simulation experiments and primary data analysis for the thesis have been completed.

