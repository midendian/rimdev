/* -*- Mode: ab-c -*- */

#ifndef __RADIO_H__
#define __RADIO_H__

int radio_init(const char *radiofn);
int radio_recvmpak(int type, int hpid, const unsigned char *buf, int buflen);
void radio_decrefcount(MESSAGE *msg);

#endif /* __RADIO_H__ */
