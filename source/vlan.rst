Configuring VLAN Interfaces
===========================

Introduction
------------

TSN streams are transmitted over Virtual LANs (VLANs). Bridges use the VLAN
priority information (PCP) to identify Stream Reservation (SR) traffic classes
which are handled according to the Forward and Queuing Enhancements for
Time-Sensitive Streams (FQTSS) mechanisms described in Chapter 34 of the `IEEE
802.1Q standard
<https://standards.ieee.org/standard/802_1Q-2018.html&sa=D&ust=1573162068591000>`_.
This tutorial will cover how to setup a VLAN interface for TSN application.

VLAN is supported in Linux\* via virtual networking interface. Any packet sent
through the VLAN interface is automatically tagged with the VLAN information
(such as  ID and PCP). Any packet received from that interface belongs to the
VLAN.

Configuring the Interface
-------------------------

The VLAN interface is created using the ip-link command from iproute2 project
which is pre-installed by the majority of Linux distributions. The following
example creates a VLAN interface for TSN usage. The example assumes *eth0* is the
physical interface.

.. code:: console

   sudo ip link add link eth0 name eth0.5 type vlan id 5 egress-qos-map 2:2 3:3

The egress-qos-map argument defines a mapping of Linux internal packet priority
(SO_PRORITY) to VLAN header PCP field for outgoing frames. The format is
FROM:TO with multiple mappings separated by spaces. This example maps
SO_PRIORITY 2 into PCP 2 and SO_PRIORITY 3 into PCP 3. For further information
about command arguments, see ip-link(8) manpage.

VLAN configuration is required on Talkers, Listeners, and Bridges.
