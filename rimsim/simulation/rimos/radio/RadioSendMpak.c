/* -*- Mode: ab-c -*- */

#include <rimsim.h>

int sim_RadioSendMpak(MPAK_HEADER *header, BYTE *data, int length)
{
	unsigned char *outbuf = NULL;
	int i = 0, tag;

	printf("sim: RadioSendMpak(%p, %p, %d)\n", header, data, length);

	if (!(rim_task_current->flags & RIM_TASKFLAG_WANTSRADIO))
		return RADIO_APP_NOT_REGISTERED;

	if (!data || (length < 0))
		return -1;

	if (header) {

		if (!(outbuf = malloc(12+length)))
			return -1;

		outbuf[i++] = header->Sender >> 16;
		outbuf[i++] = header->Sender >> 8;
		outbuf[i++] = header->Sender;

		outbuf[i++] = header->Destination >> 16;
		outbuf[i++] = header->Destination >> 8;
		outbuf[i++] = header->Destination;

		/* XXX do flags, traffic state,  and timestamp properly */
		outbuf[i++] = 0; 
		outbuf[i++] = header->MpakType;

		if ((header->MpakType == MPAK_TEXT) ||
			(header->MpakType == MPAK_DATA) ||
			(header->MpakType == MPAK_STATUS) ||
			(header->MpakType == MPAK_HPDATA)) {
			outbuf[i++] = header->lTime >> 16;
			outbuf[i++] = header->lTime >> 8;
			outbuf[i++] = header->lTime;
		}

		if (header->MpakType == MPAK_HPDATA)
			outbuf[i++] = header->HPID;

		memcpy(data, outbuf+i, length);
		i += length;
	}

	if (outbuf)
		tag = radio_sendmpak(rim_task_current->taskid, outbuf, i);
	else
		tag = radio_sendmpak(rim_task_current->taskid, data, length);

	free(outbuf);

	return tag;
}
