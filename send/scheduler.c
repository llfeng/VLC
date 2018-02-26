#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "scheduler.h"


uint32_t app_tick = 0;

#define DEFAULT_EVENT   SCH_EVENT_MAX_COUNT

void *run_scheduler(void *arg){
	uint8_t i = 0;
	scheduler_t *scheduler = (scheduler_t *)arg;
    for(;;){
        for(i = 0; i < SCH_EVENT_MAX_COUNT; i++){
            if(scheduler->sch_event[i]){
                if(scheduler->sch_event[i]->remain--){
                    printf("remain:%d\n", scheduler->sch_event[i]->remain);
                    //do nothing
                }else{                    
                    printf("will handle name:%s\n", scheduler->sch_event[i]->name);
                    scheduler->sch_event[i]->handle(scheduler->sch_event[i]->arg);
                    printf("---------step1-------\n");
                    unregSchEvent(scheduler, scheduler->sch_event[i]->name);
                    printf("---------step2-------\n");
                    break;
                }
            }
        }
        if(i == SCH_EVENT_MAX_COUNT){
            printf("no event\n");
            scheduler->sch_event[DEFAULT_EVENT]->handle(scheduler->sch_event[DEFAULT_EVENT]->arg);
        }else{
            printf("have event\n");
        }
        sleep(1);
        //usleep(100000);
        app_tick++;
    }
}

void initScheduler(scheduler_t *scheduler){
    app_tick = 0;
    uint8_t i = 0;
    for(i = 0; i < SCH_EVENT_MAX_COUNT; i++){
        scheduler->sch_event[i] = NULL;
    }   
    scheduler->cur_event_count = 0;
}


void startScheduler(scheduler_t *scheduler){
    pthread_t sch_tid;
    pthread_create(&sch_tid, NULL, run_scheduler, (void *)scheduler);
}


void regSchEvent(scheduler_t *scheduler, sch_event_t *sch_event){
	uint8_t i = 0;
    if(scheduler && sch_event){
		for(i = 0; i < SCH_EVENT_MAX_COUNT; i++){
			if(!scheduler->sch_event[i]){
				scheduler->sch_event[i] = (sch_event_t *)malloc(sizeof(sch_event_t));
				memcpy(scheduler->sch_event[i], sch_event, sizeof(sch_event_t));
				scheduler->cur_event_count++;
				break;
			}
		}
    }else{
//malloc err    
    }   
}

void regSchDefEvent(scheduler_t *scheduler, sch_event_t *sch_event){
    printf("enter %s\n", __func__);
	uint8_t i = 0;
    if(scheduler && sch_event){
        if(!scheduler->sch_event[DEFAULT_EVENT]){
            scheduler->sch_event[DEFAULT_EVENT] = (sch_event_t *)malloc(sizeof(sch_event_t));
            memcpy(scheduler->sch_event[DEFAULT_EVENT], sch_event, sizeof(sch_event_t));
//            scheduler->cur_event_count++;
        }
    }else{
//malloc err    
    }   
    printf("leave %s\n", __func__);
}


void unregSchEvent(scheduler_t *scheduler, uint8_t *name){
	uint8_t i = 0;
	for(i = 0; i < SCH_EVENT_MAX_COUNT; i++) {
        printf("[%s]---scheduler->sch_event[%d]->name:%s  name:%s\n", __func__, i, scheduler->sch_event[i]->name, name);
		if(scheduler->sch_event[i] && !strcmp(scheduler->sch_event[i]->name, name)){
			free(scheduler->sch_event[i]);
            scheduler->sch_event[i] = NULL;
			scheduler->cur_event_count--;
		}
	}
	    
}

void initSchEvent(sch_event_t *event, uint8_t *name, uint8_t repeat, uint32_t period, uint32_t remain, schEventHandle handle , void *arg){
    printf("enter %s\n", __func__);
	if(event){
		strncpy(event->name, name, strlen(name));
        printf("event->name:%s\n", event->name);
		event->repeat = repeat;
		event->period = period;
		event->remain = remain;
		event->handle = handle;
		event->arg = arg;        
	}else{
		//TODO:
	}
    printf("leave %s\n", __func__);
}



