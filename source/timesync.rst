Synchronizing Time with Linux\* PTP
===================================

Introduction
------------

Time synchronization is one of the core functionalities of TSN, and it is
specified by IEEE 802.1AS, also known as Generalized Precision Time Protocol
(gPTP). gPTP is a profile from IEEE 1588, also known as Precision Time Protocol
(PTP). gPTP consists of simplifications and constraints to PTP to optimize it
to time-sensitive applications. gPTP also introduces the Grand Master (GM)
role which can be played by any node in the gPTP domain (such as end-stations
or bridges) and is determined by the Best Master Clock Algorithm (BMCA). The GM
provides clocking information to all other nodes in the gPTP domain.

In the Linux\* ecosystem, `Linux PTP <http://linuxptp.sourceforge.net>`_ is the
most popular implementation of PTP. It supports several profiles including gPTP
and AVNU automotive profile. Linux PTP provides some tools to carry out time
synchronization:

* ptp4l: daemon that synchronizes the PTP Hardware Clock (PHC) from the NIC;

* phc2sys: daemon that synchronizes the PHC and the System clock;

* pmc: utility tool to configure ptp4l in run-time.

Although this tutorial was tested with an Intel(R) Ethernet Controller I210 NIC,
it is hardware-agnostic and applies to any 802.1AS-capable NIC with proper
device driver support. By following this tutorial, the reader will be able to
synchronize time using Linux PTP.

Installing Linux PTP
--------------------

Several Linux distributions provide a package for Linux PTP. This section
discusses how to get Linuxptp source, build, and install the tools.

.. code:: console

        git clone http://git.code.sf.net/p/linuxptp/code linuxptp
        cd linuxptp/
        make
        sudo make install

By default, Linux PTP artifacts are installed into /usr/local/sbin/.

.. _phc-sync-label:

Synchronizing the PHC
---------------------

The PHC synchronization step is mandatory for all TSN systems. It guarantees
the PHC from the NIC is in sync with the GM clock from the gPTP domain. This is
achieved by the ptp4l daemon.

To synchronize PHC with the GM clock, run the command below. Make sure to
replace ``eth0`` by the interface name corresponding to the TSN-capable NIC in
the system.

.. code:: console

        sudo ptp4l -i eth0 -f configs/gPTP.cfg --step_threshold=1 -m

The file ``gPTP.cfg`` (available in configs folder of Linux PTP source)
specified by the ``-f`` option contains the configuration options to required
to run ptp4l in gPTP mode while the ``-i`` option specifies the network
interface this instance of ptp4l is controlling. The ``--step_threshold`` is
set so ptp4l converges faster when "time jumps" occur (more on this later). The
``-m`` option enables log messages on standard output.

By default, ptp4l triggers the BMCA to determine if the PHC can be elected
GM. To force a particular role, check the ``masterOnly`` and ``slaveOnly``
configuration options from ptp4l. For further details, see ptp4l(8) manpage.

Run this step in all end-points of the network.

Synchronizing the System Clock
------------------------------

The System clock synchronization step is mandatory only for those systems where
applications rely on system clock to schedule traffic or present data (such as
AVTP plugins from ALSA and GStreamer frameworks).

PHC time is set in TAI coordinate [1]_ while System clock time is set in UTC
coordinate [2]_. To ensure System clocks (``CLOCK_REALTIME`` and ``CLOCK_TAI``)
are properly set, configure the UTC-TAI offset in the system. This is done by a
run-time option from ptp4l that is set via pmc utility tool as shown below.

.. code:: console

        sudo pmc -u -b 0 -t 1 "SET GRANDMASTER_SETTINGS_NP clockClass 248 \
                clockAccuracy 0xfe offsetScaledLogVariance 0xffff \
                currentUtcOffset 37 leap61 0 leap59 0 currentUtcOffsetValid 1 \
                ptpTimescale 1 timeTraceable 1 frequencyTraceable 0 \
                timeSource 0xa0"

Once UTC-TAI offset is properly set, synchronize the System clock with PHC.

.. code:: console

        sudo phc2sys -s eth0 -c CLOCK_REALTIME --step_threshold=1 \
                --transportSpecific=1 -w -m

The ``-s`` option specifies the PHC from ``eth0`` interface as the master clock
while the ``-c`` option specifies the System clock as subordinate (AKA "slave"
defined by IEEE Std. 801.1AS\ :sup:`TM`\ )clock. In the
command above PHC disciplines the System clock, that is, the system clock is
adjusted. The ``--transportSpecific`` option is required when running phc2sys
in a gPTP domain. The ``--step_threshold`` is set so phc2sys converges faster
when "time jumps" occurs. Finally, the ``-w`` option makes phc2sys wait until
ptp4l is synchronized and the ``-m`` option enables log messages on standard
output. For more information about phc2sys configuration option refer to
phc2sys(8) manpage.

Checking Clocks Synchronization
-------------------------------

On ptp4l, the subordinate devices report out the time offset calculated from the
master. This information can be used to determine whether the systems have been
synchronized. The output for ptp4l is:

.. code:: console

        ptp4l[5374018.735]: rms  787 max 1208 freq -38601 +/- 1071 delay  -14 +/-   0
        ptp4l[5374019.735]: rms 1314 max 1380 freq -36204 +/- 346 delay   -14 +/-   0
        ptp4l[5374020.735]: rms  836 max 1106 freq -35734 +/-  31 delay   -14 +/-   0
        ptp4l[5374021.736]: rms  273 max  450 freq -35984 +/-  97 delay   -14 +/-   0
        ptp4l[5374022.736]: rms   50 max   82 freq -36271 +/-  64 delay   -14 +/-   0
        ptp4l[5374023.736]: rms   81 max   86 freq -36413 +/-  17 delay   -14 +/-   0

The ``rms`` value reported by ptp4l once the subordinate has locked with the GM shows
the root mean square of the time offset between the PHC and the GM clock. If
ptp4l consistently reports ``rms`` lower than 100 ns, the PHC is synchronized.

Like ptp4l, phc2sys reports the time offset between PHC and System Clock, which
determines if the clocks are synchronized.

.. code:: console

        phc2sys[5374168.545]: CLOCK_REALTIME phc offset   -372582 s0 freq    +246 delay   6649
        phc2sys[5374169.545]: CLOCK_REALTIME phc offset   -372832 s1 freq      -4 delay   6673
        phc2sys[5374170.547]: CLOCK_REALTIME phc offset        68 s2 freq     +64 delay   6640
        phc2sys[5374171.547]: CLOCK_REALTIME phc offset       -20 s2 freq      -3 delay   6687
        phc2sys[5374172.547]: CLOCK_REALTIME phc offset        47 s2 freq     +58 delay   6619
        phc2sys[5374173.548]: CLOCK_REALTIME phc offset       -40 s2 freq     -15 delay   6680

The ``offset`` information reported by phc2sys shows the time offset between
the PHC and the System clock. If phc2sys consistently reports ``offset`` lower
than 100 ns, the System clock is synchronized.

To verify the TAI offset set by the pmc command above has been correctly
propagated to the kernel, read this offset value using the ``adjtimex()``
system call. For more information about the ``adjtimex()`` system call, see
adjtimex(2) manpage.

To automate this process, this tutorial includes the :download:`check_clocks
<../misc/check_clocks.c>` utility tool to verify whether Linux PTP daemons (ptp4l
and phc2sys) have been properly configured and the clocks have been
synchronized. Run the following to compile and run the utility:

.. code:: console

        gcc -o check_clocks check_clocks.c
        sudo check_clocks -d eth0

The exptected output from ``check_clocks`` is: 

.. code:: console

        Clocks on this system are synchronized :)

Avnu Automotive Profile
-----------------------

Due to the static nature of the automotive network, AVnu has specified the
`Automotive Profile
<http://avnu.org/wp-content/uploads/2014/05/Automotive-Ethernet-AVB-Func-Interop-Spec-v1.5-Public.pdf>`_
which does some optimizations to gPTP to improve startup time and reduce
network load. The main difference from gPTP is that the BMCA is disabled so
each device is statically assigned as master or subordinate.

Linux PTP also supports the Automotive Profile. To run ptp4l in that mode, the
command-line is the same as presented in :ref:`phc-sync-label` but with a
different configuration file. In the systems playing the master role, use the
``automotive-master.cfg`` file. In all other systems, use the
``automotive-slave.cfg`` file. For illustration, see the following command-line
examples:

.. code:: console

        sudo ptp4l -i eth0 -f configs/automotive-master.cfg --step_threshold=1 -m
        sudo ptp4l -i eth0 -f configs/automotive-slave.cfg --step_threshold=1 -m

Both these config files are available in the configs folder in Linux PTP
source code. The subordinate devices should also configure the
``servo_offset_threshold`` and ``servo_num_offset_values`` config options. More
information about the config options is available in ptp4l(8) manpage.

Time Jumps
----------

When a jump in time occurs in the gPTP domain, the System clock can take a
considerable amount of time to converge to the new time. This happens because
the clock is synchronized by adjusting the frequency. For example, if the PHC
time jumps by 1 second, empirical tests have shown that System clock can take
up to 30 seconds to synchronize (considering offset less than 100 ns). The
phc2sys daemon provides the ``--step_threshold=n`` option which sets a
threshold. If the time jump is greater than ``n`` seconds, time is adjusted by
stepping the clock (that means to adjust current time) instead of changing the
frequency.

However, stepping the clock has its own downsides as well. All timers set to
expire between the current time and the new time expire once the time is set.
This can affect the real-time behavior of the systems. So, use clock stepping
carefully.

Troubleshooting
---------------

In this section we discuss some issues that we have faced when trying to
synchronize time using Linux PTP in different systems

System time isn't synchronized with PHC
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If PHC offset never goes below hundreds (of nanoseconds)- or if it suddenly
spikes (as seen on phc2sys log) - leaving system time out of sync, this section
provides some hints on what to do.

Confirm NTP is not running
~~~~~~~~~~~~~~~~~~~~~~~~~~

An NTP service may be running and changing the system clock. On systems with
systemd, run:

.. code:: console

        timedatectl | grep NTP

If the output shows ``NTP service: active``, disable it:

.. code:: console

        timedatectl set-ntp false

Check if NTP has been disabled and run the clock synchronization steps again
and verify that the clocks are in sync.

Check NetworkManager is not messing with the NIC
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When NetworkManager is running, it may reset the NIC after the qdisc setup. In
this situation, PHC and the system clock may be out of sync. Do not allow the
NetworkManager to manage the TSN capable NIC. Add the following to the
``/etc/NetworkManager/NetworkManager.conf`` file:

.. code:: console

        [main]
        plugins=keyfile

        [keyfile]
        unmanaged-devices=interface-name:eth0

Restart NetworkManager. Run the clock synchronization steps again and verify
the clocks are in sync.

Ensure qdisc setup is done before clock synchronization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Qdisc setup resets the NIC, and that can make ptp4l out of sync. If any qdisc
setup needs to be done after clocks are already in sync, repeat clock
synchronization steps again and verify that the clocks are still in sync.

Confirm only one instance of ptp4l or phc2sys is running
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Multiple instances of ptp4l or phc2sys adjusting a single clocksource or
sending out Sync messages can put the clocks out of sync. So, ensure only a
single instance (per network interface) of both the daemons is running at a
time. ``pgrep`` can be useful to ensure only one instance of a particular
process is running. Look at pgrep(1) manpage for more details.

References
----------

.. [1] https://en.wikipedia.org/wiki/International_Atomic_Time

.. [2] https://en.wikipedia.org/wiki/Coordinated_Universal_Time
