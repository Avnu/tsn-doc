#!/usr/bin/env python
#
# Copyright (c) 2019, Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

# DISCLAIMER: This program should not be used in Production environment.

import argparse
import math

AAF_OVERHEAD = 24  # AVTP stream header
CVF_H264_OVERHEAD = 30  # AVTP stream header + H264 ts field + FU-A headers
VLAN_OVERHEAD = 4  # VLAN tag
L2_OVERHEAD = 18  # Ethernet header + CRC
L1_OVERHEAD = 20  # Preamble + frame delimiter + interpacket gap


def calc_class_a_credits(
        idleslope_a,
        sendslope_a,
        link_speed,
        frame_non_sr,
        max_frame_size_a):
    # According to 802.1Q-2014 spec, Annex L, hiCredit and
    # loCredit for SR class A are calculated following the
    # equations L-10 and L-12, respectively.
    hicredit = math.ceil(idleslope_a * frame_non_sr / link_speed)
    locredit = math.ceil(sendslope_a * max_frame_size_a / link_speed)
    return hicredit, locredit


def calc_class_b_credits(
        idleslope_a,
        idleslope_b,
        sendslope_b,
        link_speed,
        frame_non_sr,
        max_frame_size_a,
        max_frame_size_b):
    # Annex L doesn't present a straightforward equation to
    # calculate hiCredit for Class B so we have to derive it
    # based on generic equations presented in that Annex.
    #
    # L-3 is the primary equation to calculate hiCredit. Section
    # L.2 states that the 'maxInterferenceSize' for SR class B
    # is the maximum burst size for SR class A plus the
    # maxInterferenceSize from SR class A (which is equal to the
    # maximum frame from non-SR traffic).
    #
    # The maximum burst size for SR class A equation is shown in
    # L-16. Merging L-16 into L-3 we get the resulting equation
    # which calculates hiCredit B (refer to section L.3 in case
    # you're not familiar with the legend):
    #
    # hiCredit B = Rb * (     Mo         Ma   )
    #                     ---------- + ------
    #                      Ro - Ra       Ro
    #
    hicredit = math.ceil(idleslope_b *
                         ((frame_non_sr / (link_speed - idleslope_a)) +
                          (max_frame_size_a / link_speed)))
    # loCredit B is calculated following equation L-2.
    locredit = math.ceil(sendslope_b * max_frame_size_b / link_speed)
    return hicredit, locredit


def calc_frame_size(stream):
    frame_size = (int(stream['psize'])
                  + VLAN_OVERHEAD
                  + L2_OVERHEAD
                  + L1_OVERHEAD)
    if stream['transport'] == 'avtp-aaf':
        frame_size += AAF_OVERHEAD
    elif stream['transport'] == 'avtp-cvf-h264':
        frame_size += CVF_H264_OVERHEAD
    return frame_size


def calc_sr_class_params(streams):
    idleslope = 0
    max_frame_size = 0
    for stream in streams:
        frame_size = calc_frame_size(stream)
        idleslope += int(stream['rate']) * frame_size * 8 / 1000
        max_frame_size = max(max_frame_size, frame_size)
    return max_frame_size, math.ceil(idleslope)


def parse_stream_params(string):
    try:
        params = dict(pair.split("=") for pair in string.split(','))
    except ValueError:
        raise argparse.ArgumentTypeError('Invalid argument format')

    if len(params) > 4:
        raise argparse.ArgumentTypeError('Too many parameters')

    if 'class' not in params:
        raise argparse.ArgumentTypeError('Missing class parameter')
    if params['class'] not in ['a', 'b']:
        msg = "Unknown SR class %s" % params['class']
        raise argparse.ArgumentTypeError(msg)

    if 'transport' not in params:
        raise argparse.ArgumentTypeError('Missing transport parameter')
    if params['transport'] not in ['avtp-aaf', 'avtp-cvf-h264']:
        msg = "Unknown transport %s" % params['transport']
        raise argparse.ArgumentTypeError(msg)

    if 'rate' not in params:
        raise argparse.ArgumentTypeError('Missing rate parameter')

    if 'psize' not in params:
        raise argparse.ArgumentTypeError('Missing psize parameter')

    return params


def main():
    parser = argparse.ArgumentParser(
        description='Script to help calculating CBS parameters based on '
        'AVB streams features such as AVTP format, payload size, and '
        'frame rate.',
        epilog='More than one --stream option can be passed to calculate'
        ' CBS parameters based on multiple streams')

    parser.add_argument(
        '--stream',
        action='append',
        metavar=('class=[a,b],transport=[avtp-aaf,avtp-cvf-h264],rate=RATE,psize=SIZE'),
        help='Stream parameters: SR class, transport protocol'
        'packet rate, payload size',
        dest='streams',
        type=parse_stream_params,
        required=True)
    parser.add_argument('-S', dest='link_speed', default=1000000, type=int,
                        help='Link speed in kbps')
    parser.add_argument('-s', dest='frame_non_sr', default=1542, type=int,
                        help='Maximum frame size from non-SR traffic')
    args = parser.parse_args()

    max_frame_size_a = 0
    idleslope_a = 0

    streams_a = (s for s in args.streams if s['class'] == 'a')
    streams_b = (s for s in args.streams if s['class'] == 'b')

    max_frame_size_a, idleslope_a = calc_sr_class_params(streams_a)
    sendslope_a = idleslope_a - args.link_speed
    hicredit_a, locredit_a = calc_class_a_credits(idleslope_a,
                                                  sendslope_a,
                                                  args.link_speed,
                                                  args.frame_non_sr,
                                                  max_frame_size_a)
    print("Class A: idleslope %d sendslope %d hicredit %d locredit %d" %
          (idleslope_a, sendslope_a, hicredit_a, locredit_a))

    max_frame_size_b, idleslope_b = calc_sr_class_params(streams_b)
    sendslope_b = idleslope_b - args.link_speed
    hicredit_b, locredit_b = calc_class_b_credits(idleslope_a,
                                                  idleslope_b,
                                                  sendslope_b,
                                                  args.link_speed,
                                                  args.frame_non_sr,
                                                  max_frame_size_a,
                                                  max_frame_size_b)
    print("Class B: idleslope %d sendslope %d hicredit %d locredit %d" %
          (idleslope_b, sendslope_b, hicredit_b, locredit_b))


if __name__ == '__main__':
    main()
