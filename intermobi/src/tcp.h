/* -*- Mode: ab-c -*- */
#ifndef __TCP_H__
#define __TCP_H__

int tcp_init(void);
int tcp_handler(unsigned long srcip, unsigned char *pkt, int pktlen);
void tcp_timer(void);
void tcp_dumpsockets(void);


int intermobi_tcp_socket(int skid);
int intermobi_tcp_listen(int skid);
int intermobi_tcp_write(int skid, const unsigned char *buf, int n);
int intermobi_tcp_read(int skid, unsigned char *buf, int n);

#endif /* __TCP_H__ */

