/* -*- Mode: ab-c -*- */

#include <rimsim.h>
#include "radio.h"

static int radio_state = 0; /* off */
static int radio_rssi = RSSI_NO_COVERAGE;

struct mpak_slot {
	int occupied;
	int slotnum;
	TASK sender;
	unsigned char *buf;
	int buflen;
};

static struct mpak_slot mpak_slots[MAX_QUEUED_MPAKS];
static pthread_mutex_t mpak_slot_lock = PTHREAD_MUTEX_INITIALIZER;

static void *radio_thread(void *arg)
{
	return NULL;
}

int radio_init(const char *radiofn)
{
	pthread_t radiotid;
	int i;

	for (i = 0; i < MAX_QUEUED_MPAKS; i++) {
		mpak_slots[i].occupied = 0;
		mpak_slots[i].slotnum = i;
	}

	pthread_create(&radiotid, NULL, radio_thread, NULL);

	return 0;
}

int radio_setstate(int enabled)
{
	return -1;
}

int radio_getstate(void)
{
	return radio_state ? 1 : 0;
}

int radio_getrssi(void)
{
	return radio_state ? radio_rssi : RSSI_NO_COVERAGE;
}

unsigned long radio_getman(void)
{
	return 123456;
}

unsigned long radio_getesn(void)
{
	return 2342134;
}

static struct mpak_slot *findemptyslot(void)
{
	int i;

	for (i = 0; i < MAX_QUEUED_MPAKS; i++) {
		if (mpak_slots[i].occupied == 0)
			return mpak_slots+i;
	}

	return NULL;
}

int radio_sendmpak(TASK sender, const unsigned char *buf, int buflen)
{
	int tag;
	struct mpak_slot *slot;

	if (!buf || (buflen <= 0))
		return -1;

	pthread_mutex_lock(&mpak_slot_lock);

	if (!(slot = findemptyslot())) {
		pthread_mutex_unlock(&mpak_slot_lock);
		return -1;
	}

	if (!(slot->buf = malloc(buflen)))
		return -1;
	memcpy(slot->buf, buf, buflen);
	slot->buflen = buflen;
	slot->sender = sender;
	slot->occupied = 1;

	tag = slot->slotnum;

	pthread_mutex_unlock(&mpak_slot_lock);

	return tag;
}
