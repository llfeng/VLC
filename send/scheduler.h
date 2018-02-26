#ifndef _SCHEDULE_H
#define _SCHEDULE_H

#include <stdint.h>

#define SCH_EVENT_MAX_COUNT	10


extern uint32_t app_tick;


typedef void (*schEventHandle)(void *arg);

typedef struct{
	uint8_t name[16];
	uint8_t repeat;
	uint32_t period;
	uint32_t remain;
	void (*handle)(void *arg);
	void *arg;
}sch_event_t;

typedef struct{
	sch_event_t *sch_event[SCH_EVENT_MAX_COUNT+1];
	uint8_t cur_event_count;
}scheduler_t;

void initScheduler(scheduler_t *scheduler);

void startScheduler(scheduler_t *scheduler);

void regSchEvent(scheduler_t *scheduler, sch_event_t *sch_event);

void regSchDefEvent(scheduler_t *scheduler, sch_event_t *sch_event);

void unregSchEvent(scheduler_t *scheduler, uint8_t *name);

void initSchEvent(sch_event_t *event, uint8_t *name, uint8_t repeat, uint32_t period, uint32_t remain, schEventHandle handle, void *arg);

#endif
