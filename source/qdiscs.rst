Configuring TSN Qdiscs
======================

Introduction
------------

The TSN control plane is implemented through the Linux* Traffic Control (TC)
System. The transmission algorithms specified in the Forwarding and Queuing for
Time-Sensitive Streams (FQTSS) chapter of IEEE 802.1Q-2018 are supported via TC
Queuing Disciplines (qdiscs).

Linux currently provides the following qdiscs relating to TSN:

* CBS qdisc: Implements the Credit-Based Shaper introduced by the IEEE 802.1Qav
  amendment.

* TAPRIO qdisc: Implements the Enhancements for Scheduled Traffic introduced by
  IEEE 802.1Qbv.

* ETF qdisc: While not an FQTSS feature, Linux also provides the Earliest
  TxTime First (ETF) qdisc which enables the LaunchTime feature present in some
  NICs, such as Intel(R) Ethernet Controller I210.

These qdiscs provide an offload option to leverage the hardware support (when
supported by the NIC driver) as well as a software implementation that could be
utilized as a fallback.

Note: these qdiscs enable a transmission algorithm and should be configured on
transmitting end-stations (Talker systems). They are not required on the
receiving end-stations (Listener systems).

Although this tutorial was tested with an Intel(R) Ethernet Controller I210, it
can be used as a guide to configure any Network Interface Card (NIC). This tutorial
will enable you to configure Linux Qdiscs and enable hardware offloading.

.. _cbs-config-label:

Configuring CBS Qdisc
---------------------

The CBS algorithm shapes the transmission according to the bandwidth that has
been reserved on a given outbound queue. This feature was introduced to IEEE
802.1Q to enable Audio/Video Bridging (AVB) on top of Local Area Networks
(LANs). AVB systems rely on CBS to determine the amount of buffering required
at the receiving stations. For details on how the CBS algorithm works refer to
Annex L from IEEE 802.1Q-2018 spec.

Follow these steps to configure the CBS Qdisc:

Step 1: The CBS operates on a per-queue basis. To expose the hardware
transmission queues use the MQPRIO qdisc. MQPRIO does more than just expose the
hardware transmission queues, it also defines how Linux network priorities map
into traffic classes and how traffic classes map into hardware queues. The
command-line example below shows how to configure MQPRIO qdisc for Intel(R)
Ethernet Controller I210 which has 4 transmission queues.

.. code:: console

        sudo tc qdisc add dev eth0 parent root handle 6666 mqprio \
                num_tc 3 \
                map 2 2 1 0 2 2 2 2 2 2 2 2 2 2 2 2 \
                queues 1@0 1@1 2@2 \
                hw 0

After running this command:

* MQPRIO is installed as root qdisc oneth0 interface with handle ID 6666;

* 3 different traffic classes are defined (from 0 to 2), where Linux priority 3
  maps into traffic class 0, Linux priority 2 maps into traffic class 1, and
  all other Linux priorities map into traffic class 2;

* Packets belonging to traffic class 0 go into 1 queue at offset 0 (i.e queue
  index 0 or Q0), packet from traffic class 1 go into 1 queue at offset 1 (i.e.
  queue index 1 or Q1), and packets from traffic class 2 go into 2 queues at
  offset 2 (i.e. queues index 2 and 3, or Q2 and Q3);

* No hardware offload is enabled.

Note: By configuring MQPRIO, Stream Reservation (SR) Class A (Priority 3) is
enqueued on Q0, the highest priority transmission queue in Intel(R) Ethernet
Controller I210, while SR Class B (Priority 2) is enqueued on Q1, the second
priority. All best-effort traffic goes into Q2 or Q3.

Step 2: With MQPRIO configured, now configure CBS qdisc. By default MPPRIO
installs the fq_codel qdisc in each hardware queue exposed. This step involves
replacing that qdisc by the CBS qdisc for Q0 and Q1.

CBS parameters come straight from the IEEE 802.1Q-2018 specification. They are
the following:

* idleSlope: rate credits are accumulated when queue isn't transmitting;

* sendSlope: rate credits are spent when queue is transmitting;

* hiCredit: maximum amount of credits the queue is allowed to have;

* loCredit: minimum amount of credits the queue is allowed to have;

Calculating those parameters can be tricky and error-prone so this tutorial
provides a the :download:`calc-cbs-params.py <../misc/calc-cbs-params.py>`
helper script which takes as input TSN stream features, such as SR class,
transport protocol, and payload size and outputs the CBS parameters.

For example, consider 2 AVB streams with the following features:

* Stream A: SR class A, AVTP Compressed Video Format, H.264 profile High,
  1920x1080, 30fps.

* Stream B: SR class B, AVTP Audio Format, PCM 16-bit sample, 48 kHz, stereo,
  12 frames perAVTPDU.

To calculate the CBS parameters for that set of AVB streams, run the helper
script as follows:

.. code:: console

        calc-cbs-params.py \
                --stream class=a,transport=avtp-cvf-h264,rate=8000,psize=1470 \
                --stream class=b,transport=avtp-aaf,rate=4000,psize=48

Which should produce the output:

.. code:: console

        1st priority queue: idleslope 98688 sendslope -901312 hicredit 153 locredit -1389
        2nd priority queue: idleslope 3648 sendslope -996352 hicredit 12 locredit -113

With the CBS parameters, configuring the CBS qdisc is straightforward. Q0 is
the first priority queue while Q1 is the second priority so the CBS qdiscs are
installed as follows. The offload mode is enabled since the Intel(R) Ethernet
Controller I210 supports that feature.

.. code:: console

        sudo tc qdisc replace dev eth0 parent 6666:1 cbs \
                idleslope 98688 sendslope -901312 hicredit 153 locredit -1389 \
                offload 1

        sudo tc qdisc replace dev eth0 parent 6666:2 cbs \
                idleslope 3648 sendslope -996352 hicredit 12 locredit -113 \
                offload 1

For further information about MQPRIO and CBS qdiscs refer totc-mqprio(8) and
tc-cbs(8) manpages.

Configuring the ETF Qdisc
-------------------------

Intel(R) Ethernet Controller I210 and other NICs provide the LaunchTime feature
which enables frames to be transmitted at specific times. In Linux, this
hardware feature is enabled through the SO_TXTIME sockopt and ETF qdisc. The
SO_TXTIME socket option allows applications to configure the transmission time
for each frame while the ETF qdiscs ensures frames coming from multiple sockets
are sent to the hardware ordered by transmission time.

Like the CBS qdisc, the ETF qdisc operates on a per-queue basis so the MQPRIO
configuration described in :ref:`cbs-config-label` is required.

In the example below, the ETF qdisc is installed on Q0 and offload feature is
enabled since the Intel(R) Ethernet Controller I210 driver supports the LaunchTime
feature.

.. code:: console

        sudo tc qdisc add dev eth0 parent 6666:1 etf \
                clockid CLOCK_TAI \
                delta 500000 \
                offload

The ``clockid`` parameter specifies which clock is utilized to set the
transmission timestamps from frames. Only ``CLOCK_TAI`` is supported. ETF
requires the System clock to be in sync with the PTP Hardware Clock (PHC, refer
to :doc:`timesync` for more info). The ``delta`` parameter specifies the
length of time before the transmission timestamp the ETF qdisc sends the frame
to hardware. That value depends on multiple factors and can vary from system
to system. This example uses 500us.

The value to use for the delta parameter can be estimated using
`cyclictest
<https://wiki.linuxfoundation.org/realtime/documentation/howto/tools/cyclictest/start>`_,
run under similar conditions (same kind of expected system load, same
kernel configuration, etc) as the application using ETF. After running
``cyclictest`` for a reasonable amount of time (1 hour for example),
the maximum latency detected by ``cyclictest`` is a good aproximation
of the minimum value that should be used as ETF ``delta``. For
example, running ``cyclictest`` like this:

.. code:: console

        sudo cyclictest --mlockall --smp --priority=80 --interval=200 --distance=0

Which should have output:

.. code:: console

        T: 0 (11795) P:80 I:200 C: 726864 Min:          1 Act:        2 Avg:        1 Max:           6
        T: 1 (11796) P:80 I:200 C: 726861 Min:          1 Act:        1 Avg:        1 Max:          10
        T: 2 (11797) P:80 I:200 C: 726858 Min:          1 Act:        1 Avg:        1 Max:          78
        T: 3 (11798) P:80 I:200 C: 726855 Min:          1 Act:        1 Avg:        1 Max:          49
        T: 4 (11799) P:80 I:200 C: 726852 Min:          1 Act:        1 Avg:        1 Max:          43
        T: 5 (11800) P:80 I:200 C: 726831 Min:          1 Act:        1 Avg:        1 Max:          10
        T: 6 (11801) P:80 I:200 C: 726846 Min:          1 Act:        2 Avg:        1 Max:          27
        T: 7 (11802) P:80 I:200 C: 726843 Min:          1 Act:        1 Avg:        1 Max:           7
        T: 8 (11803) P:80 I:200 C: 726840 Min:          1 Act:        2 Avg:        1 Max:          94
        T: 9 (11804) P:80 I:200 C: 726838 Min:          1 Act:        1 Avg:        1 Max:          12
        T:10 (11805) P:80 I:200 C: 726835 Min:          1 Act:        1 Avg:        1 Max:          14
        T:11 (11806) P:80 I:200 C: 726832 Min:          1 Act:        1 Avg:        1 Max:          18

Would indicate that the minimum value of ``delta`` that can be used
should be greater than 94us, and in real use cases, a safety margin
should be added, making the minimum acceptable value of ``delta`` to
be around 100us for this particular system and workload combination.
Cyclictest is a good estimate because in nanosleep mode, it uses the
same mechanisms as the ETF Qdisc to suspend execution until a given
instant.For further information about ETF qdisc refer to tc-etf(8)
manpage.

Configuring TAPRIO Qdisc
------------------------

IEEE 802.1Q-2018 introduces the Enhancements for Scheduled Traffic (EST)
feature (formerly known as Qbv) which allows transmission from each queue to be
scheduled relative to a known timescale. In summary, transmission gates are
associated with each queue; the state of the transmission gate determines
whether queued frames can be selected for transmission ("Open" or "Closed"
states). Each Port is associated with a Gate Control List (GCL) which contains
an ordered list of gate operations. For further details on how this feature
works, refer to section 8.6.8.4 of IEEE 802.1Q-2018.

EST allows systems to be configured and participate in complex networks,
similar to those envisioned by IEEE 802.1Qcc-2018. In this specification, a
central entity with full knowledge of all the nodes, the traffic produced by
those nodes, and their requirements, is able to produce a schedule for the
whole network. This scenario is thought to enable primarily industrial
use-cases, as many of the concepts are similar to other field buses.

The EST feature is supported in Linux via the TAPRIO qdisc. Similar to MQPRIO,
the qdisc defines how Linux networking stack priorities map into traffic
classes and how traffic classes map into hardware queues. Besides that, it
also enables the user to configure the GCL for a given interface.

There are several NIC drivers in mainline Linux that support the EST hardware
feature. That includes:

* Intel Ethernet controllers like I210 and I225/226
* Synopsys Ethernet controllers like Ethernet QoS Controller
* NXP Ethernet controllers like NETC
* Several TSN switches from different vendors

TAPRIO qdisc has different operating modes indicated by ``flags`` parameter:

* Software only implementation: The TAPRIO qdisc will keep track of the gate
  control lists in software and pass the packets to the NIC drivers at the
  correct point in time. This mode can be set either by omitting ``flags`` or by
  using ``flags 0x0``.

* Tx-Time-assisted mode: This mode leverages the Tx Launch Time feature to
  implement EST. This is useful for hardware which lacks support for full EST
  hardware offload such as Intel I210 Ethernet controllers. This mode can be set
  by using ``flags 0x1``.

* Full hardware offload: The gate control lists will be directly programmed into
  the hardware and the NIC will take care of sending packets at the correct
  interval. This mode can be set by using ``flags 0x2``.

The following tutorial shows how to setup TAPRIO with full hardware offload and
Tx-Time-assisted mode.

For the sake of exercise, let's say we have 3 traffic classes and we want to
schedule traffic as follows:

* The first transmission window has 300 us duration and only traffic class 0 is
  transmitted;

* The second transmission window also has 300 us duration but now both traffic
  class 0 and 1 are transmitted;

* Third and last window has 400 us duration and only traffic class 2 is
  transmitted;

* The following schedule starts at 1,000,000,000 absolute time.

To achieve that, configure TAPRIO qdisc as shown below:

.. code:: console

        sudo tc qdisc replace dev eth0 parent root handle 100 taprio \
                num_tc 3 \
                map 2 2 1 0 2 2 2 2 2 2 2 2 2 2 2 2 \
                queues 1@0 1@1 2@2 \
                base-time 1000000000 \
                sched-entry S 01 300000 \
                sched-entry S 03 300000 \
                sched-entry S 04 400000 \
                flags 0x2

The parameters ``num_tc``, map and queues are identical to MQPRIO so refer to
:ref:`cbs-config-label` for details. The way TAPRIO is configured, only one
hardware queue is enabled. The other parameters are described as follows. For
further details on TAPRIO configuration, check tc-taprio(8) manpage.

* ``base-time``: specifies the start time of the schedule. If ``base-time`` is
  in the past, the schedule starts as soon as possible, aligning the start
  cycle specified by the GCL.

* ``sched-entry``: each of these specify one entry in the cycle, which is
  executed in order. Each entry has the format: ``<CMD> <GATE MASK>
  <INTERVAL>```. ``CMD`` defines the command that is executed for each
  interval, the commands defined are “S”: SetGates, which defines that the
  traffic classes defined in ``GATE MASK`` will be open for this interval “H”:
  Set-And-Hold-MAC, has the same meaning as SetGates, with the addition that
  preemption is disabled during this interval; “R”: Set-And-Release-MAC, has
  the same meaning as SetGates, with the addition that preemption is enabled
  during this interval; ``GATE MASK`` defines to which traffic classes the
  command is applied, specified as a bit mask, with bit 0 referring to traffic
  class 0 (TC 0) and bit N to traffic class N (TC N). ``INTERVAL`` defines the
  duration of each interval in nanoseconds.

* flags: control which additional flags are sent to taprio, in this case, we
  are enabling hardware offload mode.

There are NICs which do not support full hardware offload such as Intel I210
Ethernet Controller. However, EST can still be leveraged since TAPRIO provides a
TxTime-assisted implementation (available since kernel 5.3) and a pure software
implementation. In Tx-Time-assisted mode, the LaunchTime feature is used to
schedule packet transmissions, emulating the EST feature.  The NIC must support
LaunchTime to be able to use that mode. If not, use the pure software
implementation. This tutorial uses the Intel(R) Ethernet Controller I210 which
supports LaunchTime, thereby setting TAPRIO up for using the TxTime-assisted
mode.

.. code:: console

        sudo tc qdisc replace dev eth0 parent root handle 100 taprio \
                num_tc 3 \
                map 2 2 1 0 2 2 2 2 2 2 2 2 2 2 2 2 \
                queues 1@0 1@1 2@2 \
                base-time 1000000000 \
                sched-entry S 01 300000 \
                sched-entry S 03 300000 \
                sched-entry S 04 400000 \
                flags 0x1 \
                txtime-delay 500000 \
                clockid CLOCK_TAI

In this case there are two additional parameters:

* txtime-delay: this argument is only used in TxTime-assisted mode, and allows
  to control the minimum time the transmission time of a packet is set in the
  future;

* clockid: defines against which clock reference these timestamps should be
  considered.

When TxTime-assisted mode is enabled, install the ETF qdisc on the hardware
queue exposed by TAPRIO so LaunchTime is enabled on NIC and packets are ordered
by transmission time before they are delivered to the controller. The ETF qdisc
can be installed as follows:

.. code:: console

        sudo tc qdisc replace dev eth0 parent 100:1 etf \
                clockid CLOCK_TAI \
                delta 500000 \
                offload \
                skip_sock_check

Once both TAPRIO and ETF qdiscs are properly setup, the traffic generated by
all applications running on top of eth0 interface are scheduled according to
the GCL set configured.

