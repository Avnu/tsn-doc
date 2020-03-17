Getting Started with AVB on Linux\*
===================================

Introduction
------------

Audio/Video Bridging (AVB) is a set of IEEE standards enabling time-sensitive
Audio/Video applications on Local Area Networks (LANs). AVB provides time
synchronization, bounded transmission latency, resource management, and
application interoperability. Since these features can additionally be
leveraged by non-AV systems, IEEE expanded its scope and rebranded AVB as
Time-Sensitive Networking (TSN).

For the past few years, several TSN building blocks have been developed in the
upstream Linux\* ecosystem, such as generalized Precision Time Protocol (gPTP)
support on Linux PTP, TSN Queueing Disciplines (qdiscs), device driver support,
and Libavtp project. On top of these building blocks, Audio Video Transport
Protocol (AVTP) plugins were developed for ALSA and GStreamer frameworks to
enable AVB on Linux systems. This tutorial focuses on leveraging these plugins
to implement an AVB application.

The `Advanced Linux Sound Architecture (ALSA) <https://www.alsa-project.org>`_
is a low-level framework that provides audio functionality on Linux. It
comprises sound card device drivers, kernel-user interfaces, and user-space
libraries and utility tools. `GStreamer <https://gstreamer.freedesktop.org>`_,
on the other hand, is a higher-level framework that provides multimedia
functionalities such as encoding, multiplexing, filtering, and rendering to
applications.

This tutorial discusses how to set up the system and get started with AVB
talker/listener applications. By the end of this tutorial, you will have two
endpoints, a TSN Talker and a TSN Listener, configured to transmit audio and
video streams with bounded latency, and will be able to run some AVB sample
applications.

System Requirements
-------------------

This tutorial has been validated on two desktop machines with Intel(R) Ethernet
Controller I210 connected back-to-back and Linux kernel version 4.19.

Plugins Installation
--------------------

Depending on your use case, install the plugin you need. Follow these steps to
install the ALSA and GStreamer AVTP plugins on both machines.

To complete the installation, follow the steps to get the source code and build
the plugin. This includes installing the respective dependencies. Those should
be packaged for most distros, though. For example, on Ubuntu\*, one can use to
install them:

.. code:: console

        sudo apt install build-essential git meson flex bison glib2.0 \
                libcmocka-dev autoconf libtool autopoint libncurses-dev \
                libpulse-dev gettext

In the instructions below, all plugins artifacts are installed in
``/usr/local`` so make sure your environment variables considered it.

.. code:: console

        export PATH=/usr/local/bin:/usr/local/sbin:$PATH
        export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
        export ACLOCAL_PATH=/usr/local/share/aclocal/:$ACLOCAL_PATH
        export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH

Both ALSA and GStreamer plugins depend on Libavtp so you must install it in
your system. Libavtp is an open source implementation of Audio Video Transport
Protocol (AVTP) specified in IEEE 1722-2016 spec. Libavtp source code can be
found in `github.com/AVnu/libavtp <https://github.com/AVnu/libavtp>`_. Checkout
the code and follow the instructions in the README file to get it built and
installed.

ALSA Plugin
~~~~~~~~~~~

The ALSA framework is the de facto framework that provides audio functionality
in the Linux system. While it comprises both kernel and user space components,
the AVB examples covered in this tutorial only depend on the user space
components which are the following:

* alsa-lib provides the core libraries

* alsa-utils provides utility tools for playback, capture, and mix audio
  samples

* alsa-plugins provides assorted plugins to the ALSA framework

The AVTP plugin is part of alsa-plugins since version 1.1.8.

This tutorial uses the latest version, 1.1.9. Follow these steps to get,
build, and install the ALSA artifacts provided by those projects.

Step 1: Start with the core libraries from alsa-lib project:

.. code:: console

        git clone https://github.com/alsa-project/alsa-lib.git
        cd alsa-lib/
        git checkout v1.1.9
        autoreconf -i
        ./configure --prefix=/usr/local
        make
        sudo make install

Step 2: Install the utility tools from alsa-utils project:

.. code:: console

        git clone https://github.com/alsa-project/alsa-utils.git
        cd alsa-utils/
        git checkout v1.1.9
        autoreconf -i
        ./configure --prefix=/usr/local
        make
        sudo make install

Step 3: Install the plugins from alsa-plugins project:

.. code:: console

        git clone https://github.com/alsa-project/alsa-plugins.git
        cd alsa-plugins/
        git checkout v1.1.9
        autoreconf -i
        ./configure --prefix=/usr/local
        make
        sudo make install

Step 4: Regenerate the shared library cache after manually installing
libraries:

.. code:: console

        sudo ldconfig

GStreamer Plugin
~~~~~~~~~~~~~~~~

By its own definition, GStreamer "is a library for constructing graphs of
media-handling components". It provides a pipeline, in which elements connect
to one another and data is processed as it flows. Elements are provided by
GStreamer plugins. The AVTP plugin is provided by the gst-plugins-bad module.
As the AVTP plugin is not yet part of a GStreamer release, to build it is
necessary to also build GStreamer core and gst-plugins-base from source.

Step 1: Install GStreamer core:

.. code:: console

        git clone https://gitlab.freedesktop.org/gstreamer/gstreamer.git
        cd gstreamer
        meson build --prefix=/usr/local
        ninja -C build
        sudo ninja -C build install
        sudo setcap cap_net_raw+ep /usr/local/bin/gst-launch-1.0

Last command ensures that gst-launch-1.0 tool has permission to access network
over layer 2, as needed by TSN applications.

Step 2: Install gst-plugins-base:

.. code:: console

        git clone https://gitlab.freedesktop.org/gstreamer/gst-plugins-base.git
        cd gst-plugins-base
        meson build --prefix=/usr/local
        ninja -C build
        sudo ninja -C build install

Step 3: Install gst-plugins-bad:

.. code:: console

        git clone https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad.git
        cd gst-plugins-bad
        meson build --prefix=/usr/local
        ninja -C build
        sudo ninja -C build install

Step 4: Regenerate the shared library cache after manually installing
libraries:

.. code:: console

        sudo ldconfig

Step 5: Confirm that the GStreamer AVTP plugin has been successfully installed.

.. code:: console

        gst-inspect-1.0 avtp

The output contains standard information about the GStreamer AVTP plugin.

Step 6: Install additional GStreamer modules:

* gst-plugins-good provides basic elements used in this tutorial

* gst-plugins-ugly is needed if using software encoder x264enc

* gst-libav provides software decoders

* gst-vaapi provides VA-API encoders and decoders

These modules can be installed from your favorite distro packages. For
instance, to install all of the above on Ubuntu, run:

.. code:: console

        sudo apt install gstreamer1.0-plugins-ugly gstreamer1.0-plugins-good \
                gstreamer1.0-libav gstreamer1.0-vaapi

Step 7: To ensure GStreamer finds the plugins installed from packages or from
sources, add the system default plugin directory to the path GStreamer search
plugin on. For instance, on Ubuntu do:

.. code:: console

        export GST_PLUGIN_PATH="/usr/lib/x86_64-linux-gnu/gstreamer-1.0"


System Setup
------------

To run an AVB application, configure the following:

* VLAN interface

* Time synchronization

* Qdiscs

.. _vlan-config-label:

VLAN Configuration
~~~~~~~~~~~~~~~~~~

Since AVB streams are transmitted over Virtual LANs (VLANs), a VLAN
interface on both hosts is required. The VLAN interface is created using the
*ip-link* command from iproute2 project which is pre-installed on most Linux
distributions.

This example transmits AVB streams on VLAN ID 5 and follows the priority
mapping recommended by IEEE 802.1Q-2018. In this tutorial, the TSN-capable NIC is
represented by the *eth0* interface. Make sure to replace it with the interface
name of the TSN-capable NIC in your system.

Run the following command to create the *eth0.5* interface, which
represents the VLAN interface in this tutorial:

.. code:: console

        sudo ip link add link eth0 name eth0.5 type vlan id 5 \
                egress-qos-map 2:2 3:3

        sudo ip link set eth0.5 up

For further information regarding VLAN in Linux, refer to :doc:`vlan`.

.. _qdiscs-config-label:

Qdiscs Configuration
~~~~~~~~~~~~~~~~~~~~

The TSN control plane is implemented through the Linux Traffic Control (TC)
System. The transmission algorithms specified in Forwarding and Queuing for
Time-Sensitive Streams (FQTSS) chapter from IEEE 802.1Q-2018 are supported via
TC Queuing Disciplines (qdiscs). Three qdiscs are required to set up an AVB
system: MQPRIO, CBS and ETF.

Follow these steps to configure the qdiscs:

Step 1: Add the MQPRIO qdisc to the root qdisc to expose hardware queues in the TC
system. The command below configures MQPRIO for the Intel(R) Ethernet
Controller I210 which has 4 transmission queues:

.. code:: console

        sudo tc qdisc add dev eth0 parent root handle 6666 mqprio \
                num_tc 3 \
                map 2 2 1 0 2 2 2 2 2 2 2 2 2 2 2 2 \
                queues 1@0 1@1 2@2 \
                hw 0

Step 2: CBS qdisc configuration depends on the number of AVB streams as well as
the stream features. This tutorial uses 2 streams with the following features:

* Stream A: SR class A, AVTP Compressed Video Format, H.264 profile High,
  1920x1080, 30 fps.

* Stream B: SR class B, AVTP Audio Format, PCM 16-bit sample, 48 kHz, stereo,
  12 frames per AVTPDU.

Configure the CBS qdiscs as below to reserve bandwidth to accommodate these
streams:

.. code:: console

        sudo tc qdisc replace dev eth0 parent 6666:1 handle 7777 cbs \
                idleslope 98688 sendslope -901312 hicredit 153 locredit -1389 \
                offload 1

        sudo tc qdisc replace dev eth0 parent 6666:2 handle 8888 cbs \
                idleslope 3648 sendslope -996352 hicredit 12 locredit -113 \
                offload 1

Step 3: Configure the ETF qdiscs as children of CBS qdiscs.

.. code:: console

        sudo tc qdisc add dev eth0 parent 7777:1 etf \
                clockid CLOCK_TAI \
                delta 500000 \
                offload

        sudo tc qdisc add dev eth0 parent 8888:1 etf \
                clockid CLOCK_TAI \
                delta 500000 \
                offload

For further information regarding TSN qdiscs configuration refer
to :doc:`qdiscs`.

Time Synchronization
~~~~~~~~~~~~~~~~~~~~

Both ALSA and GStreamer plugins require the PTP Hardware Clock (PHC) from the
NIC as well as the System clock to be synchronized with gPTP Grand Master. This
is done by Linux PTP tools. To do this, follow these steps:

Step 1: Synchronize the PHC with gPTP GM clock:

.. code:: console

        sudo ptp4l -i eth0 -f <linuxptp source dir>/configs/gPTP.cfg --step_threshold=1 -m

Step 2: PHC time is set in TAI coordinate time while the system clock time is
in UTC time. To set the system clocks (CLOCK_REALTIME and CLOCK_TAI), configure
the UTC-TAI offset in the system, as below:

.. code:: console

        sudo pmc -u -b 0 -t 1 "SET GRANDMASTER_SETTINGS_NP clockClass 248 \
                clockAccuracy 0xfe offsetScaledLogVariance 0xffff \
                currentUtcOffset 37 leap61 0 leap59 0 currentUtcOffsetValid 1 \
                ptpTimescale 1 timeTraceable 1 frequencyTraceable 0 \
                timeSource 0xa0"

Step 3: Synchronize the system clock with the PHC:

.. code:: console

        sudo phc2sys -w -m -s eth0 -c CLOCK_REALTIME --step_threshold=1 \
                --transportSpecific=1

For further information regarding time synchronization, refer to
:doc:`timesync`.

AVB Audio Talker/Listener Examples
----------------------------------

With software installed and system set up, you are ready to see AVB audio
talker and listener applications in action. AVB Audio streaming is supported by
both ALSA and GStreamer plugins.

.. _example-alsa-label:

Examples using ALSA Framework
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ALSA AVTP Audio Format (AAF) plugin is a PCM plugin that uses AAF AVTPDUs
to transmit/receive audio data through a TSN network. The plugin enables any
existing ALSA-based application to operate as AVB talker or listener.

* In playback mode, the plugin reads PCM samples from the audio buffer,
  encapsulates into AVTPDUs and transmits to the network, mimicking a typical
  AVB talker.

* In capture mode, the plugin receives AVTPDUs from the network, retrieves the
  PCM samples, and presents them (at AVTP presentation time) to the application
  for rendering, mimicking a typical AVB Listener.

Step 1: Add the AAF device to the ALSA configuration file (/etc/asound.conf)
on both Talker and Listener hosts. The following configuration creates the AAF
device according to the AVB audio stream described in
:ref:`qdiscs-config-label`. For a full description of AAF device
configuration options, refer to `ALSA AAF Plugin documentation
<https://github.com/alsa-project/alsa-plugins/blob/master/doc/aaf.txt>`_.

Note: In the configuration file, replace the interface name *eth0.5* with the
VLAN interface you created in :ref:`vlan-config-label`.

.. code:: console

        pcm.aaf0 {
                type aaf
                ifname eth0.5
                addr 01:AA:AA:AA:AA:AA
                prio 2
                streamid AA:BB:CC:DD:EE:FF:000B
                mtt 50000
                time_uncertainty 1000
                frames_per_pdu 12
                ptime_tolerance 100
        }

Step 2: Run the *speaker-test* tool from alsa-utils to implement the AVB talker
application. The tool generates a tone which is transmitted through the network
as an AVTP stream by the aaf0 device.

On the Talker host run:

.. code:: console

        sudo speaker-test -p 25000 -F S16_BE -c 2 -r 48000 -D aaf0

Quick explanation about speaker-test arguments: ``-p`` configures ALSA period
size, ``-F`` sets the sample format, ``-c`` the number of channels, ``-r`` the
sampling rate, and ``-D`` the ALSA device. For more details check
speaker-test(1) manpage.

Step 3: While the AVB stream is being transmitted through the network, run the
listener and play it back, using *aplay* and *arecord* tools from alsa-utils.
These tools create a PCM loopback between two ALSA devices. In this case, the
capture device is *aaf0* and the playback device is *default* (usually, this is
the main sound card in the system).

On the listener host run:

.. code:: console

        sudo arecord -F 25000 -t raw -f S16_BE -c 2 -r 48000 -D aaf0 | \
                aplay -F 25000 -t raw -f S16_BE -c 2 -r 48000 -D default

Result: You can hear the tone transmitted by the Talker in the speakers (or
headphones) attached to the Listener host.

Troubleshooting
^^^^^^^^^^^^^^^

If no sound is heard:

#. Ensure the volume is high enough.

If aplay fails with "Sample format non available":

#. Some sound cards do not support big endian formats. It’s necessary to
   convert the PCM samples to little endian before pushing them to your
   soundcard. This can be done by defining a converter device in
   /etc/asound.conf, on Listener, as shown below.

.. code:: console

        pcm.converter0 {
                type linear
                slave {
                        pcm default
                        format S16_LE
                }
        }

Use *converter0* as playback device instead of *default*.

.. _example-gstreamer-label:

Examples using GStreamer Framework
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The GStreamer AVTP plugin provides a set of elements that are arranged in a
GStreamer pipeline to implement AVB talker and listener applications. These
elements can be categorized as:

* *payloaders*: elements that encapsulate/decapsulate audio and video data
  into/from AVTPDUs. The plugin provides a pair of
  payloader/depayloader elements for each AVTP format supported;

* *sink*: element receives AVTPDUs from upstream and sends them to the network;

* *source*: element that receives AVTPDUs from the network and send them
  upstream in the pipeline.

This example uses the *gst-launch-1.0* tool to implement the AVB talker and
listener applications.

Step 1: At the AVB talker host, run the following command to generate the AAF
stream:

On the AVB talker:

.. code:: console

        gst-launch-1.0 clockselect. \( clock-id=realtime \
            audiotestsrc samplesperbuffer=12 is-live=true ! \
            audio/x-raw,format=S16BE,channels=2,rate=48000 ! \
            avtpaafpay mtt=50000000 tu=1000000 streamid=0xAABBCCDDEEFF000B processing-deadline=0 ! \
            avtpsink ifname=eth0.5 address=01:AA:AA:AA:AA:AA priority=2 processing-deadline=0 \)

In this command the *clockselect* defines a special GStreamer pipeline to be
used, that enables you to select a clock for the pipeline, using its *clock-id*
switch. Setting it to *realtime* sets CLOCK_REALTIME as the pipeline clock
while the rest of the command between the parenthesis describes the pipeline.
The *!* sign refers to connecting two elements. Let’s check what each element
in the pipeline does:

* *audiotestsrc* generates a tone.

* *audio/x-raw,format=S16BE,channels=2,rate=48000* is not a true element but a
  filter that defines the audio sample features *audiotestsrc* generates.

* *avtpaafpay* encapsulates audio samples into AAF AVTPDUs.

* *avtpsink* sends AVTPDs to the network.

Note that AVTP-specific features, such as maximum transit time, time
uncertainty, and stream ID, are set via the *avtpaafpay* element
properties while network-specific features such as network interface and
traffic priority are set via *avtpsink* element properties.

The *processing-deadline* property set above defines an overall processing
latency for the pipeline. The payloader element takes it into consideration
when calculating the AVTP presentation time. Note that the processing-deadline
property from the payloader and sink elements should have the same value.

To learn about a specific element utilized in the pipeline above, run:

.. code:: console

        gst-inspect-1.0 <ELEMENT>

Step 2: While the AVB stream is being transmitted through the network, run
the listener application to receive the stream and play it back.

On the AVB listener:

.. code:: console

        gst-launch-1.0 clockselect. \( clock-id=realtime \
            avtpsrc ifname=eth0.5 address=01:AA:AA:AA:AA:AA ! \
            queue max-size-buffers=0 max-size-time=0 ! \
            avtpaafdepay streamid=0xAABBCCDDEEFF000B ! audioconvert ! autoaudiosink \)

The *avtpsrc* element receives AVTPDUs from the network
and push them to the *avtpaafdepay* element which extracts the audio samples.
The *autoaudiosink* automatically detects the default audio sink in the system
and plays it back.

In the pipeline above:

#. Using the *queue* element after *avtpsrc* ensures packet reception is not
   blocked in case any downstream element blocks the pipeline.

#. Using the *audioconvert* element before *autoaudiosink* ensures the audio
   stream is automatically converted to a compatible stream configuration in
   case the playback device doesn’t support S16BE, stereo, 48 kHz.

Result: You hear the tone transmitted by the Talker in the speakers (or
headphones) attached to the Listener host.

Troubleshooting
^^^^^^^^^^^^^^^

If no sound is heard, make sure the volume is high enough.

AVB Video Talker/Listener Example
---------------------------------

AVB video is only supported by the GStreamer AVTP plugin. Similar to the
GStreamer audio example, the AVB Video example also uses *gst-launch-1.0* tool
to implement AVB video talker and listener applications.

Step 1: Run the following command to generate the CVF stream on the AVB talker:

.. code:: console

        gst-launch-1.0 clockselect. \( clock-id=realtime \
            videotestsrc is-live=true ! video/x-raw,width=720,height=480,framerate=30/1 ! \
            clockoverlay ! vaapih264enc ! h264parse config-interval=-1 ! \
            avtpcvfpay processing-deadline=20000000 mtt=2000000 tu=125000 streamid=0xAABBCCDDEEFF000A ! \
            avtpsink ifname=eth0.5 priority=3 processing-deadline=20000000 \)

Similar to the audio talker pipeline, the *videotestsrc* element generates the
video stream to transmit over AVTP. The *clockoverlay* element adds a
wall-clock time on the top-left corner of the video (we use this information to
check playback synchronization, more on this later). The *vaapih264enc* element
encodes the stream into H.264 and the *h264parse* element parses it so the
output capabilities are set correctly. The *avtpcvfpay* element then
encapsulates it into CVF AVTPDUs which are finally transmitted by the
*avtpsink* element. If *vaapih264enc* isn't available in your system, you may
use another H.264 encoder instead, such as *x264enc*.

Note that we set the ``config-interval=-1`` property from *h264parse* to ensure
H.264 stream metadata is in-band so the H.264 decoder running by the AVB
listener application is able to actually decode it. Also note we use a
*processing-deadline* of 20ms as opposed to 0ms used on audio pipeline. We
chose this value due this pipeline being more "heavy" on processing -
generating and encoding video, adding overlays, etc. The correct value for this
property depends on the pipeline and the system it runs on.

Step 2: While the AVB stream is being transmitted through the network, run the
listener and play it back.

On the AVB listener:

.. code:: console

        gst-launch-1.0 clockselect. \( clock-id=realtime \
            avtpsrc ifname=eth0.5 ! avtpcvfdepay streamid=0xAABBCCDDEEFF000A ! \
            queue max-size-bytes=0 max-size-buffers=0 max-size-time=0 ! \
            vaapih264dec ! videoconvert ! clockoverlay halignment=right ! autovideosink \)

*avtpsrc* receives AVTPDUs from the network, *avtpcvfdepay* extracts the H.264
NAL units, *vaapih264dec* decodes the stream, *clockoveraly* adds a wall clock
to the top-right corner of the video, and *autovideosink* automatically detects
a video sync (e.g. X server) and renders the video stream. If *vaapih264dec*
isn't available in your system, you may use another H.264 decoder instead, such
as *avdec_h264*.

Results: The video is streamed by the talker and displayed on the listener
screen.

Note that clocks on top left (talker clock) and right (listener clock) may not
be in perfect sync, due network and pipeline latencies.

Troubleshooting
~~~~~~~~~~~~~~~

#. If there is a delay when video playback starts on listener, try starting
   the listener after the talker application. This usually happens due to the
   fact that video can only be decoded when a keyframe is present on stream.
   This effect won’t happen if talker side is started after listener, as first
   frame will be a keyframe already. Should this be an issue, check your
   encoder options to control keyframe frequency.

.. _stream-from-file-label:

Streaming From a File
---------------------

The examples above use helper tools to synthetize the stream contents.
However, implementing real use-cases involves reading stream content from a
file, such as a WAV file for audio or MP4 file for video.

For the audio examples, use the `WAV file <https://kozco.com/tech/piano2.wav>`_
which has the PCM features from the AVB Stream B (16-bit sample, stereo, and
48kHz).

The ALSA Way
~~~~~~~~~~~~

Follow these steps to stream contents from a file, using the ALSA Framework:

Step 1: Convert the PCM samples within that file from little endian into big
endian format, before pushing them to the AAF device. To achieve that, define
a converter device and add it to the */etc/asound.conf* file:

.. code:: console

        pcm.converter1 {
                type linear
                slave {
                        pcm aaf0
                        format S16_BE
                }
        }

Step 2: Use *aplay* to read PCM samples from the file and play them back in the
converter device:

.. code:: console

        sudo aplay -F 12500 -D converter1 piano2.wav

Troubleshooting
^^^^^^^^^^^^^^^

If you try another WAV file, and it does not work, make sure the CBS qdisc is
adjusted accordingly to accommodate the stream features from this another file.

The GStreamer Way
~~~~~~~~~~~~~~~~~

From a WAV File
^^^^^^^^^^^^^^^

To stream contents from a WAV file, use the *filesrc* element and run the
command:

.. code:: console

        gst-launch-1.0 clockselect. \( clock-id=realtime \
            filesrc location=piano2.wav ! wavparse ! audioconvert ! \
            audiobuffersplit output-buffer-duration=12/48000 ! \
            avtpaafpay mtt=50000000 tu=1000000 streamid=0xAABBCCDDEEFF000B processing-deadline=0 ! \
            avtpsink ifname=eth0.5 address=01:AA:AA:AA:AA:AA priority=2 processing-deadline=0 \)

This example uses the *wavparse* element to demux the WAV file, the
*audioconvert* element to handle any audio conversion needed, and the
*audiosplitbuffer* element to generate GstBuffers with 12 samples.

From an MP4 File
^^^^^^^^^^^^^^^^

Run this command to generate a CVF stream from an MP4 file. The file used in
this example can be downloaded `here
<https://download.blender.org/durian/trailer/sintel_trailer-480p.mp4>`_.

.. code:: console

        gst-launch-1.0 clockselect. \( clock-id=realtime \
            filesrc location=sintel_trailer-480p.mp4 ! qtdemux ! h264parse config-interval=-1 ! \
            avtpcvfpay processing-deadline=20000000 mtt=2000000 tu=125000 streamid=0xAABBCCDDEEFF000A ! \
            avtpsink ifname=eth0.5 priority=3 processing-deadline=20000000 \)

This example uses *qtdemux* element to demultiplex the MP4 container and access
the video data.

Troubleshooting
"""""""""""""""

Both pipelines above have one caveat: *gst-launch-1.0* does not provide a way
to disable prerolling [#]_ so the timestamp from the first AVTPDU isn't set
correctly. While this should not usually be an issue, it could cause hiccups
and delays when starting playback on listener side. Writing an *appsrc* element
may allow sourcing the file without the preroll step.

Streaming From a Live Source
----------------------------

Generating an AVB stream from a live source, such as microphones or cameras, is
another use case to consider.

The ALSA Way
~~~~~~~~~~~~

Follow these steps to stream contents from a live source, using the ALSA
Framework:

Step 1: To determine what device is the microphone in your system, run:

.. code:: console

        arecord -l

This command lists the capture devices detected in the system. It provides
information about the devices alongside a ``card X (...) device Y``.
Considering ``card 1 (...) device 0`` is the microphone device, move to Step 2.

Step 2: Use the *arecord* and *aplay* pair to get PCM samples from the
microphone device loop them into the AAF device as shown:

.. code:: console

        arecord -F 25000 -t raw -f S16_LE -c 2 -r 48000 -D hw:1,0 | \
                sudo aplay -F 25000 -t raw -f S16_BE -c 2 -r 48000 -D aaf0

Troubleshooting
^^^^^^^^^^^^^^^

Some microphones do not supply big endian formats. In this case, convert the
PCM samples to big endian before pushing them to the AAF plugin. This can be
done as described in the :ref:`stream-from-file-label`. Remember to use the
convert device as playback device instead of *aaf0*.

The GStreamer Way
~~~~~~~~~~~~~~~~~

Follow these steps to stream contents from a live source, using the GStreamer
Framework:

Step 1: To check which devices are known to GStreamer and its properties,
including the kind of output, use the *gst-device-monitor-1.0* tool:

.. code:: console

        gst-device-monitor-1.0 Video/Source Audio/Source

This generates a list of all audio and video source devices GStreamer knows
about, along with their properties. It includes brief tips on using them, such
as ``gst-launch-1.0 v4l2src ! ...``. Use this information to create an
appropriate pipeline.

Step 2.A: To use a microphone as the source of a pipeline stream, use *alsasrc*
element as shown:

.. code:: console

        gst-launch-1.0 clockselect. \( clock-id=realtime \
            alsasrc device=hw:1,0 ! audioconvert ! \
            audio/x-raw,format=S16BE,channels=2,rate=48000 ! \
            audiobuffersplit output-buffer-duration=12/48000 ! \
            avtpaafpay mtt=50000000 tu=1000000 streamid=0xAABBCCDDEEFF000B processing-deadline=0 ! \
            avtpsink ifname=eth0.5 address=01:AA:AA:AA:AA:AA priority=2 processing-deadline=0 \)

Step 2.B: To use a camera as the source of a pipeline stream for video, use
the *v4l2src* element as shown:

.. code:: console

        gst-launch-1.0 clockselect. \( clock-id=realtime \
            v4l2src ! videoconvert ! video/x-raw,width=720,height=480,framerate=30/1 ! \
            vaapih264enc ! h264parse config-interval=1 ! \
            avtpcvfpay processing-deadline=20000000 mtt=2000000 tu=125000 streamid=0xAABBCCDDEEFF000A ! \
            avtpsink ifname=eth0.5 priority=3 processing-deadline=20000000 \)

Here, *videoconvert* element converts output from *v4l2src* element to the
video/x-raw filter specified, creating a video stream with the features
expected by the H.264 encoder.

Running Multiple Talker Applications on the Same Host
-----------------------------------------------------

This section describes how to run multiple talker applications on the same
host. In addition to the streams described in :ref:`qdiscs-config-label`, two
more streams are included as follows:

* Stream C: SR class B, AVTP Audio Format, PCM 16-bit sample, 8 kHz, mono, 2
  frames per AVTPDU;

* Stream D: SR class B, AVTP Audio Format, PCM 16-bit sample, 48 kHz, 6
  channels, 12 frames per AVTPDU.

First, reconfigure CBS to accommodate the two new streams. Configure the qdiscs
as shown.

.. code:: console

        sudo tc qdisc replace dev eth0 parent 6666:2 cbs \
                idleslope 12608 sendslope -987392 hicredit 41 locredit -207 \
                offload 1

The ALSA Way
~~~~~~~~~~~~

To run multiple streams you need to add new AAF devices to ALSA configuration
file (one for each stream). We could do the same thing done in
:ref:`example-alsa-label`, but instead we're going to leverage the ALSA plugin
runtime configuration. Instead of defining AAF devices statically, you can do
it dynamically by the time you specify the device.

Follow these steps to run multiple talker applications on one host using the
ALSA Framework:

Step 1: Replace the *pcm.aaf0* device in the /etc/asound.conf file by the
device shown below:

.. code:: console

        pcm.aaf {
                @args [ IFNAME ADDR PRIO STREAMID MTT UNCERTAINTY FRAMES TOLERANCE ]
                @args.IFNAME {
                        type string
                }
                @args.ADDR {
                        type string
                }
                @args.PRIO {
                        type integer
                }
                @args.STREAMID {
                        type string
                }
                @args.MTT {
                        type integer
                }
                @args.UNCERTAINTY {
                        type integer
                }
                @args.FRAMES {
                        type integer
                }
                @args.TOLERANCE {
                        type integer
                }

                type aaf
                ifname $IFNAME
                addr $ADDR
                prio $PRIO
                streamid $STREAMID
                mtt $MTT
                time_uncertainty $UNCERTAINTY
                frames_per_pdu $FRAMES
                ptime_tolerance $TOLERANCE
        }

Step 2: Run multiple instances of *speaker-test* (one for each AVB audio
stream), varying the AAF device parameters and the PCM features according to
the features from streams B, C, and D.

Stream B:

.. code:: console

        sudo speaker-test -p 12500 -F S16_BE -c 2 -r 48000 \
                -D aaf:eth0.5,01:AA:AA:AA:AA:AA,2,AA:BB:CC:DD:EE:FF:000B,50000,1000,12,100

Stream C:

.. code:: console

        sudo speaker-test -p 12500 -F S16_BE -c 1 -r 8000 \
                -D aaf:eth0.5,01:AA:AA:AA:AA:AA,2,AA:BB:CC:DD:EE:FF:000C,50000,1000,2,100

Stream D:

.. code:: console

        sudo speaker-test -p 12500 -F S16_BE -c 6 -r 48000 \
                -D aaf:eth0.5,01:AA:AA:AA:AA:AA,2,AA:BB:CC:DD:EE:FF:000D,50000,1000,12,100

Note that you can check if each stream is running properly on listener, by
adapting listener sample command shown in :ref:`example-alsa-label` (remember
to account for streamid, frequency and number of channels differences). Note
that by default, ALSA will not mix different audio streams, so you will only
be able to listen to one audio stream each time. You can use a mixer plugin if
you want to mix.

The GStreamer Way
~~~~~~~~~~~~~~~~~

For GStreamer, running several streams at the same time involves creating
several pipelines, each one with the right stream parameters:

Stream B:

.. code:: console

        gst-launch-1.0 clockselect. \( clock-id=realtime \
            audiotestsrc samplesperbuffer=12 is-live=true ! \
            audio/x-raw,format=S16BE,channels=2,rate=48000 ! \
            avtpaafpay mtt=50000000 tu=1000000 streamid=0xAABBCCDDEEFF000B processing-deadline=0 ! \
            avtpsink ifname=eth0.5 address=01:AA:AA:AA:AA:AA priority=2 processing-deadline=0 \)

Stream C:

.. code:: console

        gst-launch-1.0 clockselect. \( clock-id=realtime \
            audiotestsrc samplesperbuffer=2 is-live=true ! \
            audio/x-raw,format=S16BE,channels=1,rate=8000 ! \
            avtpaafpay mtt=50000000 tu=1000000 streamid=0xAABBCCDDEEFF000C processing-deadline=0 ! \
            avtpsink ifname=eth0.5 address=01:AA:AA:AA:AA:AA priority=2 processing-deadline=0 \)

Stream D:

.. code:: console

        gst-launch-1.0 clockselect. \( clock-id=realtime \
            audiotestsrc samplesperbuffer=12 is-live=true ! \
            audio/x-raw,format=S16BE,channels=6,rate=48000 ! \
            avtpaafpay mtt=50000000 tu=1000000 streamid=0xAABBCCDDEEFF000D processing-deadline=0 ! \
            avtpsink ifname=eth0.5 address=01:AA:AA:AA:AA:AA priority=2 processing-deadline=0 \)

Note that you can check if each stream is running properly on listener, by
adapting listener sample command shown in :ref:`example-gstreamer-label`
(remember to account for streamid, frequency and number of channels
differences).

Troubleshooting
---------------

ALSA AVB talker eventually fails when I set 'time_uncertainty' to 125
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

According to Table 4 from IEEE 1722-2016, the Max Timing Uncertainty for Class
A streams is 125 us so the 'time_uncertainty' configuration from AAF device
should be set to 125. However, *speaker-test* or *aplay* eventually fail
transmitting when such value is set. This is a known issue and should be fixed
soon. This tutorial will be updated once the issue is fixed. As a workaround
set the 'time_uncertainty' to a greater value. 500 has worked consistently when
running empirical tests.

Video doesn’t work when using *x264enc* as encoder and *avdec_h264* as decoder
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Using *x264enc* and *avdec_h264* together was found to have issues on some
systems. One workaround is to set *bframes* as in ``... ! x264enc bframes=0 !
...`` on talker.


When streaming a higher resolution video, such as HD or Full HD, video doesn’t work or work with awful quality on Listener
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

While audio samples are usually small in size, videos can be much bigger,
especially with big resolutions and framerate. As videos commonly eat up more
resources, one must make sure that more resources are available to them.

One of the resources that can be a limitation to big videos is the socket
buffer size. If socket buffer fills up, packets may be dropped, compromising
video playback. To avoid transmit buffer filling up, one can increase its size:

.. code:: console

        sudo sysctl -w net.core.wmem_max=21299200

        sudo sysctl -w net.core.wmem_default=21299200

These commands will increase socket buffer size to approximately 20 MB. It
depends on video characteristics how big this buffer needs to be, but with
suggested size, it should work well for Full HD videos.

On the listener side, *queue* element after depayloader is usually enough to
bufferize packets received.

I get "Unknown qdisc etf" when trying to set up ETF Qdisc on Ubuntu Disco (19.04)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

While Ubuntu Disco has kernel 5.0, it has iproute2 4.18. Update iproute2
package. You can install it from sources following `these instructions
<https://git.kernel.org/pub/scm/network/iproute2/iproute2.git/about>`_.
Alternatively, you can install it from Ubuntu Eoan (19.10) following `these
other instructions
<https://help.ubuntu.com/community/PinningHowto%23Recommended_alternative_to_pinning>`_.

ALSA talker application fails while sending AVTPDUs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Especially when system load is high, AVB applications can take too long to be
scheduled in and AVTPDU transmission deadlines could be lost. This can be
addressed by using RT Linux and assigning schedule priorities properly. On
regular Linux, though, empirical tests have shown that changing the schedule
policy and priority of the AVB application process mitigates the issue.

The command example below runs *speaker-test* with FIFO scheduling policy
and priority 98.

.. code:: console

        sudo chrt --fifo 98 speaker-test -p 12500 -F S16_BE -c 2 -r 48000 -D aaf0

.. [#] Prerolling is a technique GStreamer uses to ensure smooth playing. The first
       frame is processed by the pipeline, but is not played by the sink until
       pipeline state changes to “playing”. While this allows for smooth transition
       when user clicks the play button on a normal media player, for the AVTP plugin,
       the first frame does not have timing information, as GStreamer is unaware when
       it will be played. However, as *gst-launch-1.0* tool starts playing right
       after preroll, any disruption should be minimal.
