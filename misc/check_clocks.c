/*
 * Copyright (c) 2019, Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* DISCLAIMER: This program should not be used in production environment. */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/timex.h>
#include <byteswap.h>
#include <errno.h>
#include <stdbool.h>

#define ONE_SEC 1000000000LL
/*
 * FIXME: Figure out the UTC_OFFSET programmatically so we don't have to
 * manually update it here everytime it changes.
 */
#define UTC_OFFSET 37
#define PTP_MAX_DEV_PATH 16

/* fd to clockid helpers. Copied from posix-timers.h. */
#define CLOCKFD 3

/* Borrowed from linuxptp/pmc_common.c */
#define MANAGEMENT		0xD
#define PTP_VERSION		0x2
#define CTL_MANAGEMENT		0x4

#define TLV_MANAGEMENT		0x0001
#define TLV_PORT_DATA_SET	0x2004
#define TLV_TIME_STATUS_NP	0xC000

#define TSMT_SET(TSMT, TRANSPORT_SPECIFIC, MSG_TYPE) \
		(TSMT = ((TRANSPORT_SPECIFIC << 4) | MSG_TYPE))

const char send_sock[] = "/var/run/ptp_offset";
const char ptp_sock[] = "/var/run/ptp4l";

enum port_states {
	MASTER = 6,
	PASSIVE = 7,
	UNCALIBRATED = 8,
	SLAVE = 9,
	GRAND_MASTER = 10
};

/*
 * Request and response data structures. Borrowed from linuxptp/msg.h
 * and Hector Blanco's iic-interopapp/src/ptp.h
 */
struct port_id {
	uint8_t		clock_id[8];
	uint16_t	port_num;
} __attribute__((packed));

struct ptp_hdr {
	uint8_t		tsmt;
	uint8_t		ver;
	uint16_t	msg_len;
	uint8_t 	domain_num;
	uint8_t 	reserved1;
	uint16_t 	flags;
	int64_t 	correction;
	uint32_t	reserved2;
	struct port_id	src_port_id;
	uint16_t	seq_id;
	uint8_t		control;
	int8_t		log_interval;
} __attribute__((packed));

struct mgmt_msg {
	struct ptp_hdr	hdr;
	struct port_id	dest_port_id;
	uint8_t		start_hops;
	uint8_t		boundary_hops;
	uint8_t		flags;
	uint8_t		reserved;
} __attribute__((packed));

struct get_req {
	struct mgmt_msg	mgmt;
	uint16_t 	type;
	uint16_t 	len;
	uint16_t 	req_id;
} __attribute__((packed));

struct port_ds {
	struct port_id	pid;
	uint8_t		state;
} __attribute__((packed));

struct resp_port_ds {
	struct get_req	mgt;
	struct port_ds	pds;
} __attribute__((packed));

struct resp_time_stat {
	struct get_req	mgt;
	int64_t 	master_offset;
} __attribute__((packed));

void print_usage(void)
{
	printf("Check Clocks verifies the state of local clocks to ensure"
	       " a sane TSN configuration\n");
	printf("Usage: sudo ./check_clocks -d <interface name> \n");
	printf("Options for check_clocks are\n");
	printf(" -d <TSN Ethernet interface name>. This is required\n");
	printf(" -v Verbose - Dumps ptp timestamps and deltas\n");
	printf(" -h Help - Print the usage\n");
}

static inline clockid_t make_process_cpuclock(const unsigned int pid,
					      const clockid_t clock)
{
	return ((~pid) << 3) | clock;
}

static inline clockid_t fd_to_clockid(const int fd)
{
	return make_process_cpuclock((unsigned int) fd, CLOCKFD);
}

static void open_phc_fd(int* fd_ptp, char* ifname)
{
	struct ethtool_ts_info interface_info = {0};
	char ptp_path[PTP_MAX_DEV_PATH];
	struct ifreq req = {0};
	int fd_ioctl;

	/* Get PHC index */
	interface_info.cmd = ETHTOOL_GET_TS_INFO;
	snprintf(req.ifr_name, sizeof(req.ifr_name), "%s", ifname);
	req.ifr_data = (char *) &interface_info;

	fd_ioctl = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd_ioctl < 0) {
		perror("Couldn't open socket");
		exit(EXIT_FAILURE);
	}

	if (ioctl(fd_ioctl, SIOCETHTOOL, &req) < 0) {
		perror("Couldn't issue SIOCETHTOOL ioctl");
		exit(EXIT_FAILURE);
	}

	snprintf(ptp_path, sizeof(ptp_path), "%s%d", "/dev/ptp",
		 interface_info.phc_index);

	*fd_ptp = open(ptp_path, O_RDONLY);
	if (*fd_ptp < 0) {
		perror("Couldn't open the PTP fd. Did you forget to run with sudo again?");
		exit(EXIT_FAILURE);
	}

	close(fd_ioctl);
}

static void build_ptp_request(struct get_req *ptp_req, uint16_t req_id,
			      bool gptp_profile)
{
	memset(ptp_req, 0, sizeof(struct get_req));

	if (gptp_profile)
		TSMT_SET(ptp_req->mgmt.hdr.tsmt, 0x1,  MANAGEMENT);
	else
		TSMT_SET(ptp_req->mgmt.hdr.tsmt, 0, MANAGEMENT);

	ptp_req->mgmt.hdr.ver = PTP_VERSION;

	/* Management packets have ZERO in data length */
	ptp_req->mgmt.hdr.msg_len = htons(sizeof(struct get_req));

	/*
	 * FIXME: Linuxptp's pmc uses 1 for port id. At this point
	 * I am unable to find a reason in the 1588 spec. Please
	 * explain if you find the reason or otherwise fix this.
	 */
	ptp_req->mgmt.hdr.src_port_id.port_num = htons(0x1);
	ptp_req->mgmt.hdr.control = CTL_MANAGEMENT;
	ptp_req->mgmt.hdr.log_interval = 0x7F;

	/* All 1's for destination port */
	memset(&ptp_req->mgmt.dest_port_id, 0xFF, sizeof(struct port_id));

	ptp_req->type = htons(TLV_MANAGEMENT);

	/* 1588 spec says "2 + datalen". datalen is ZERO here */
	ptp_req->len = htons(0x2);
	ptp_req->req_id = htons(req_id);
}

static int init_sockets(int *uds_fd, struct sockaddr_un *dest_addr)
{
	struct sockaddr_un src_addr;

	*uds_fd = socket(PF_LOCAL, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	if (*uds_fd < 0) {
		fprintf(stderr, "Failed to open the socket\n");
		return EXIT_FAILURE;
	}

	unlink(send_sock);

	memset(&src_addr, 0, sizeof(struct sockaddr_un));
	src_addr.sun_family = AF_LOCAL;
	strncpy(src_addr.sun_path, send_sock, strlen(send_sock));

	if (bind(*uds_fd, (struct sockaddr *) &src_addr, sizeof(struct sockaddr_un)) < 0) {
		fprintf(stderr, "Failed to bind\n");
		close(*uds_fd);
		return EXIT_FAILURE;
	}

	/* PTP Socket init */
	memset(dest_addr, 0, sizeof(struct sockaddr_un));
	dest_addr->sun_family = AF_LOCAL;
	strncpy(dest_addr->sun_path, ptp_sock, strlen(ptp_sock));

	return EXIT_SUCCESS;
}

static int send_wait_recv(int uds_fd, struct sockaddr_un *dest_addr, void *req,
			  ssize_t send_len, uint8_t *rec_buf, ssize_t recv_len)
{
	ssize_t count;
	struct pollfd pollfd;

	pollfd.fd = uds_fd;
	pollfd.events = POLLIN | POLLERR;

	count = sendto(uds_fd, req, send_len, 0,
		       (struct sockaddr *) dest_addr, sizeof(struct sockaddr_un));

	if (count == send_len) {
		int n = poll(&pollfd, 1, 1000);

		if (n != 1) {
			if (n == -1)
				fprintf(stderr, "poll() failed: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}

		count = read(uds_fd, rec_buf, recv_len);

		if (count >= sizeof(struct resp_time_stat)) {
			return EXIT_SUCCESS;
		} else {
			fprintf(stderr, "Incomplete response received\n");
			return EXIT_FAILURE;
		}
	} else {
		fprintf(stderr, "Failed to send PTP management packet - %s\n",
			strerror(errno));
		return EXIT_FAILURE;
	}
}

static inline bool is_port_sane(struct resp_port_ds *resp_port)
{
	return ((resp_port->pds.state == MASTER) ||
		(resp_port->pds.state == SLAVE) ||
		(resp_port->pds.state == GRAND_MASTER));
}

static bool get_port_status(uint8_t *rec_buf, int *port_state)
{
	bool sane_port_state = false;
	struct get_req *rec_hdr = (struct get_req *) rec_buf;

	if (ntohs(rec_hdr->type) == TLV_MANAGEMENT &&
	    ntohs(rec_hdr->req_id) == TLV_PORT_DATA_SET) {

		struct resp_port_ds *resp_port = (struct resp_port_ds *) rec_buf;

		sane_port_state = is_port_sane(resp_port);
		*port_state = resp_port->pds.state;
	}

	return sane_port_state;
}

static int64_t get_master_offset(uint8_t *rec_buf)
{
	int64_t offset = 0;
	struct get_req *rec_hdr = (struct get_req *) rec_buf;

	if (ntohs(rec_hdr->type) == TLV_MANAGEMENT &&
		  ntohs(rec_hdr->req_id) == TLV_TIME_STATUS_NP) {

		struct resp_time_stat *resp_time = (struct resp_time_stat *) rec_buf;
		offset = llabs((int64_t) bswap_64(resp_time->master_offset));
	}

	return offset;
}

static int check_ptp_offset(void)
{
	struct sockaddr_un dest_addr;
	struct get_req port_req, time_req;
	uint8_t rec_buf[128] = {};
	bool sane_port_state = false;
	int uds_fd, ret = EXIT_FAILURE;
	bool gptp_profile = false;
	int port_state;
	int64_t offset = 0;

	if (init_sockets(&uds_fd, &dest_addr) != EXIT_SUCCESS)
		return ret;

	build_ptp_request(&port_req, TLV_PORT_DATA_SET, gptp_profile);
	if (send_wait_recv(uds_fd, &dest_addr, (void *) &port_req, sizeof(struct get_req),
			   rec_buf, sizeof(rec_buf)) == EXIT_FAILURE) {

		/*
		 * Send the same request again with the transportSpecific field
		 * set to 0x1. This is needed when running the 802.1AS (or
		 * gPTP) profile. (For details see,  IEEE 802.1AS-2011
		 * 10.5.2.2.1)
		 */
		gptp_profile = true;
		build_ptp_request(&port_req, TLV_PORT_DATA_SET, gptp_profile);
		if (send_wait_recv(uds_fd, &dest_addr, (void *) &port_req, sizeof(struct get_req),
				   rec_buf, sizeof(rec_buf)) == EXIT_FAILURE) {
			goto close_socket;
		}
	}

	sane_port_state = get_port_status(rec_buf, &port_state);

	if (port_state == SLAVE) {
		/* Set request id to TIME_STATUS_NP */
		build_ptp_request(&time_req, TLV_TIME_STATUS_NP, gptp_profile);
		if (send_wait_recv(uds_fd, &dest_addr, (void *) &time_req, sizeof(struct get_req),
				   rec_buf, sizeof(rec_buf)) == EXIT_FAILURE) {
			goto close_socket;
		}

		offset = get_master_offset(rec_buf);
	}

	if (sane_port_state && offset <= 100)
		ret = EXIT_SUCCESS;
	else
		fprintf(stderr, "PTP peer port state and/or offset are messed up !\n");

close_socket:
	close(uds_fd);
	return ret;
}

static int check_local_clock(char *ifname, int verbose)
{
	struct timespec ts_rt1, ts_rt2, ts_ptp1, ts_ptp2, ts_tai1, ts_tai2;
	uint64_t rt, tai, ptp, lat_rt, lat_tai, lat_ptp;
	int64_t phc_rt, phc_tai;
	struct timex t = { 0 };
	int fd_ptp, err;
	int ret = EXIT_SUCCESS;

	open_phc_fd(&fd_ptp, ifname);

	/* Fetch timestamps for each clock. */
	clock_gettime(CLOCK_REALTIME, &ts_rt1);
	clock_gettime(CLOCK_TAI, &ts_tai1);
	clock_gettime(fd_to_clockid(fd_ptp), &ts_ptp1);
	rt = (ts_rt1.tv_sec * ONE_SEC) + ts_rt1.tv_nsec;
	tai = (ts_tai1.tv_sec * ONE_SEC) + ts_tai1.tv_nsec;
	ptp = (ts_ptp1.tv_sec * ONE_SEC) + ts_ptp1.tv_nsec;

	/* Compute clocks read latency. */
	clock_gettime(CLOCK_REALTIME, &ts_rt1);
	clock_gettime(CLOCK_REALTIME, &ts_rt2);
	lat_rt = ((ts_rt2.tv_sec * ONE_SEC) + ts_rt2.tv_nsec)
		   - ((ts_rt1.tv_sec * ONE_SEC) + ts_rt1.tv_nsec);

	clock_gettime(CLOCK_TAI, &ts_tai1);
	clock_gettime(CLOCK_TAI, &ts_tai2);
	lat_tai = ((ts_tai2.tv_sec * ONE_SEC) + ts_tai2.tv_nsec)
		   - ((ts_tai1.tv_sec * ONE_SEC) + ts_tai1.tv_nsec);

	clock_gettime(fd_to_clockid(fd_ptp), &ts_ptp1);
	clock_gettime(fd_to_clockid(fd_ptp), &ts_ptp2);
	lat_ptp = ((ts_ptp2.tv_sec * ONE_SEC) + ts_ptp2.tv_nsec)
		   - ((ts_ptp1.tv_sec * ONE_SEC) + ts_ptp1.tv_nsec);

	phc_rt = ptp - rt;
	phc_tai = ptp - tai;

	if (verbose) {
		printf("rt tstamp:\t%lu\n", rt);
		printf("tai tstamp:\t%lu\n", tai);
		printf("phc tstamp:\t%lu\n", ptp);
		printf("rt latency:\t%lu\n", lat_rt);
		printf("tai latency:\t%lu\n", lat_tai);
		printf("phc latency:\t%lu\n", lat_ptp);
		printf("phc-rt delta:\t%ld\n", phc_rt);
		printf("phc-tai delta:\t%ld\n\n", phc_tai);
	}

	if (llabs(phc_rt - UTC_OFFSET * ONE_SEC) >= 50000) {
		fprintf(stderr, "phc-rt delta is not %d sec !\n", UTC_OFFSET);
		ret = EXIT_FAILURE;
	}

	if (llabs(phc_tai) > 50000) {
		fprintf(stderr, "phc-tai delta is greater than 50 usec !\n");
		ret = EXIT_FAILURE;
	}

	/* check UTC-TAI offset */
	if (adjtimex(&t) == -1) {
		fprintf(stderr, "adjtimex() failed: %s\n", strerror(errno));
		ret = EXIT_FAILURE;
	}
	if (t.tai != UTC_OFFSET) {
		fprintf(stderr, "TAI offset set in kernel is not correct !\n");
		ret = EXIT_FAILURE;
	}

	close(fd_ptp);

	return ret;
}

int main(int argc, char** argv)
{
	char ifname[IFNAMSIZ] = { };
	int ret, verbose = 0;

	struct option longopts[] = {
		{ "dev",        required_argument,      NULL,           'd' },
		{ "verbose",    no_argument,            &verbose,        1  },
		{ "help",       no_argument,            NULL,           'h' },
		{ 0 }
	};

	while ((ret = getopt_long(argc, argv, ":d:hv", longopts, NULL)) != -1) {
		switch (ret) {
		case 'd':
			strncpy(ifname, optarg, sizeof(ifname) - 1);
			break;
		case 'v':
			verbose = 1;
			printf("Dumping timestamps and deltas\n\n");
			break;
		case 'h':
			print_usage();
			return EXIT_SUCCESS;
		case 0:
			break;
		case ':':
			fprintf(stderr, "option -%c requires an argument !\n",
				optopt);
			return EXIT_FAILURE;
		case '?':
		default:
			fprintf(stderr, "option '-%c' is invalid !\n", optopt);
			print_usage();
			return EXIT_FAILURE;
		}
	}

	if (ifname[0] == '\0') {
		fprintf(stderr, "Interface name is required\n");
		print_usage();
		return EXIT_FAILURE;
	}

	/* The Bitwise OR of the return values is intentional */
	ret = check_local_clock(ifname, verbose) | check_ptp_offset();

	if (ret == EXIT_SUCCESS)
		printf("Clocks on this system are synchronized :)\n");
	else
		fprintf(stderr, "Please verify ptp4l and phc config and restart "
			"them if necessary to synchronize the clocks !\n");

	return ret;
}
