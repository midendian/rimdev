/* -*- Mode: ab-c -*- */
#ifndef __TCP_H__
#define __TCP_H__

int tcp_init(void);
int tcp_handler(unsigned long srcip, unsigned char *pkt, int pktlen);
void tcp_timer(void);
void tcp_dumpsockets(void);

#endif /* __TCP_H__ */

