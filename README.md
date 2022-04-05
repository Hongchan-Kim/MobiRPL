MobiRPL
============================

This is the open-source code of "MobiRPL: Adaptive, Robust, and RSSI-based Mobile Routing in Low Power and Lossy Network," published in Journal of Communications and Networks.
MobiRPL is implemented on Contiki OS (version 3.0).

Main application files can be found in examples/ipv6/MobiRPL
1. udp-sink.c contains operations of RPL root node. It generates downlink traffic.
2. udp-sender.c contains operations of RPL non-root node. It generates uplink traffic.
3. project-conf.h introduces a lot of experimental settings and parameters.

There are three important parameters to configure MobiRPL.
* MOBIRPL_MOBILITY_DETECTION
* MOBIRPL_CONNECTIVITY_MANAGEMENT
* MOBIRPL_RH_OF

Each of the three mechanisms of MobiRPL can be turned on or off by setting these parameters to one or zero.
There are some other parameters to configure the operation of MobiRPL.
Please see project-conf.h file for more information.

When using this source code, please cite the following paper:

Hongchan Kim, Hyung-sin Kim, and Saewoong Bahk, "MobiRPL: Adaptive, Robust, and RSSI-based Mobile Routing in Low Power and Lossy Networks," to appear in Journal of Communications and Networks, 2022.

The Contiki Operating System
============================

[![Build Status](https://travis-ci.org/contiki-os/contiki.svg?branch=release-3-0)](https://travis-ci.org/contiki-os/contiki/branches)

Contiki is an open source operating system that runs on tiny low-power
microcontrollers and makes it possible to develop applications that
make efficient use of the hardware while providing standardized
low-power wireless communication for a range of hardware platforms.

Contiki is used in numerous commercial and non-commercial systems,
such as city sound monitoring, street lights, networked electrical
power meters, industrial monitoring, radiation monitoring,
construction site monitoring, alarm systems, remote house monitoring,
and so on.

For more information, see the Contiki website:

[http://contiki-os.org](http://contiki-os.org)
