/* -*- Mode: ab-c -*- */
#ifndef __CORE_H__
#define __CORE_H__

#include <Rim.h>

#define APPNAME "intermobi"

unsigned short csum_partial(void *buf, unsigned int len);

#define PKTHANDLER_REJECT 0
#define PKTHANDLER_ACCEPT 1

#define IPPROTO_ICMP 0x01
#define IPPROTO_TCP  0x06
#define IPPROTO_UDP  0x11

#if 1
#define INTERMOBI_OURIP      0x0a000402
#else
#define INTERMOBI_OURIP      0xc0a80afa
#endif
#define INTERMOBI_GATEWAYMAN 102009

int ip_send(unsigned long destip, unsigned char prot, unsigned char *buf, int buflen);

#endif /* __CORE_H__ */
