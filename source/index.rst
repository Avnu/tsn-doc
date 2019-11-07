TSN Documentation Project for Linux\*
=====================================

Introduction
------------

Welcome to the Time-Sensitive Networking (TSN) Documentation project for Linux\*!
This project provides a set of hands-on tutorials to help you get started with
TSN on Linux systems. This project focuses only on the TSN features provided by
mainline projects from the Linux ecosystem such as `Linux kernel
<https://www.kernel.org>`_, `Linux PTP <http://linuxptp.sourceforge.net>`_,
`ALSA <https://www.alsa-project.org>`_, and `GStreamer
<https://gstreamer.freedesktop.org>`_.

For the past few years, multiple TSN features have been enabled on the upstream
Linux ecosystem [1]_ [2]_ [3]_ [4]_ [5]_ [6]_. This project aims to help
developers and integrators on how to get started with those TSN features by
providing hands-on tutorials on how to leverage the support available in the
upstream Linux ecosystem. This documentation project stitches together the
information scattered throughout cover letters and project-specific
documentation.

Linux ecosystem supports several TSN features such as Credit-Based Shaper
(former Qav), Enhancements for Scheduled Traffic (EST, former Qbv), Generalized
Precision Time Protocol (gPTP), and Audio/Video Transport Protocol (AVTP). It
also supports the LaunchTime feature present in some NICs, such as Intel(R)
Ethernet Controller I210, which enables user applications to offload packet
transmission.

The features above are supported by different projects: CBS, EST, and
LaunchTime are supported by the Linux kernel via the Queueing Disciplines
(qdiscs), gPTP is supported by Linux PTP project, and AVTP is supported by
Libavtp project. Audio/Video Bridging (AVB) Talker/Listener use-cases are
supported by ALSA and GStreamer AVTP plugins.

Organization
------------

The documentation provided in this project is organized in self-contained
tutorials that are listed below.

.. toctree::
   :maxdepth: 1

   avb.rst
   vlan.rst
   qdiscs.rst
   timesync.rst

References
----------

.. [1] https://patchwork.ozlabs.org/cover/831420

.. [2] https://patchwork.ozlabs.org/cover/938991

.. [3] https://patchwork.ozlabs.org/cover/976513

.. [4] https://github.com/AVnu/OpenAvnu/pull/751

.. [5] https://patchwork.kernel.org/cover/10655287

.. [6] https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/merge_requests/361

