/* -*- Mode: ab-c -*- */

#include <rimsim.h>

static MESSAGE *clonemsg(MESSAGE *msg)
{
	MESSAGE *newmsg;

	if (!msg)
		return NULL;

	if (!(newmsg = malloc(sizeof(MESSAGE))))
		return NULL;

	memcpy(newmsg, msg, sizeof(MESSAGE));

	return newmsg;
}

static void enqueuemsg(rim_task_t *task, MESSAGE *msg, TASK poster)
{
	rim_msg_t *newmsg;

	if (!task || !msg)
		return;

	if (!(newmsg = malloc(sizeof(rim_msg_t))))
		return;

	if (!(newmsg->msg = clonemsg(msg))) {
		free(newmsg);
		return;
	}
	newmsg->poster = poster;

	pthread_mutex_lock(&task->msglock);
	newmsg->next = task->msgqueue;
	task->msgqueue = newmsg;
	pthread_mutex_unlock(&task->msglock);

	return;
}

int sendmessage(MESSAGE *msg, TASK poster)
{
	rim_task_t *cur;

	if (!msg)
		return -1;

	for (cur = rim_task_list; cur; cur = cur->next)
		enqueuemsg(cur, msg, poster);

	return 0;
}

int sendmessageto(rim_task_t *target, MESSAGE *msg, TASK poster)
{

	if (!msg || !target)
		return -1;

	enqueuemsg(target, msg, poster);

	return 0;
}

/* Must be called within an application context */
MESSAGE *getmessage(TASK *taskret)
{
	rim_msg_t *msg;
	MESSAGE *ret;

	pthread_mutex_lock(&rim_task_current->msglock);
	if ((msg = rim_task_current->msgqueue))
		rim_task_current->msgqueue = rim_task_current->msgqueue->next;
	pthread_mutex_unlock(&rim_task_current->msglock);

	if (!msg)
		return NULL;

	if (taskret)
		*taskret = msg->poster;
	ret = msg->msg;

	free(msg);

	return ret;
}

void freemessage(MESSAGE *msg)
{

	free(msg);

	return;
}
