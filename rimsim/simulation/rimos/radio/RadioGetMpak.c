/* -*- Mode: ab-c -*- */

#include <rimsim.h>

/*
 * mpakTag  (input) Tag of requested MPAK.
 * header  (output) Destination buffer for header information (optional)
 * data    (output) Destination buffer for payload (optional)
 * [return]         Number of bytes written to [data], or size of MPAK.
 *
 */
int sim_RadioGetMpak(int mpakTag, MPAK_HEADER *header, BYTE *data)
{
	unsigned char fullbuf[RIM_MAXMOBITEX_BUFLEN];
	int fullbuflen = RIM_MAXMOBITEX_BUFLEN;
	int i = 0;

	printf("sim: RadioGetMpak(%d, %p, %p)\n", mpakTag, header, data);

	if ((fullbuflen = radio_retrievempak(mpakTag, fullbuf, fullbuflen)) == -1)
		return -1;

	if (!header && !data)
		return fullbuflen;

	if (header) {

		header->Sender = 
			(fullbuf[i] << 16) | 
			(fullbuf[i+1] << 8) | 
			(fullbuf[i+2]);
		i += 3;

		header->Destination = 
			(fullbuf[i] << 16) | 
			(fullbuf[i+1] << 8) | 
			(fullbuf[i+2]);
		i += 3;

		header->Flags = fullbuf[i];
		i++;

		header->MpakType = fullbuf[i];
		i++;

		header->lTime =
			(fullbuf[i] << 16) | 
			(fullbuf[i+1] << 8) | 
			(fullbuf[i+2]);
		i += 3;

		/* XXX handle flags, traffic state, and timestamp properly */
		header->TrafficState = 0;
		memset(&header->Timestamp, 0, sizeof(header->Timestamp));

		if (header->MpakType == MPAK_HPDATA) {
			header->HPID = fullbuf[i];
			i++;
		} else
			header->HPID = 0;

	}

	if (data)
		memcpy(data, fullbuf+i, fullbuflen-i);

	return fullbuflen - i;
}
