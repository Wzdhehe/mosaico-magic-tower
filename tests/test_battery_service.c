/* Real host threads exercise initialization without blocking the UI caller. */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "game_battery_service.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

typedef struct { pthread_mutex_t m; pthread_cond_t c; unsigned count; } sync_t;
typedef struct { sync_t sync; pthread_t thread; TaskFunction_t fn; void *arg; atomic_bool retired; } task_t;
typedef struct { pthread_mutex_t m; bool full; size_t size; unsigned char data[32]; } queue_t;
static _Thread_local task_t *current;
static task_t *last_task;
static atomic_uint calls, deleted, polls;
static unsigned failures;
static sync_t gate = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0};
static bool entered, released;
static atomic_bool stopped;
static void sync_init(sync_t *s) { pthread_mutex_init(&s->m, NULL); pthread_cond_init(&s->c, NULL); }
static void sync_destroy(sync_t *s) { pthread_mutex_destroy(&s->m); pthread_cond_destroy(&s->c); }
static void signal_sync(sync_t *s) { pthread_mutex_lock(&s->m); ++s->count; pthread_cond_signal(&s->c); pthread_mutex_unlock(&s->m); }
static void short_wait(void) { struct timespec t={0,1000000}; nanosleep(&t,NULL); }
static void *thread_main(void *p) { current=p; current->fn(current->arg); return NULL; }
BaseType_t xTaskCreate(TaskFunction_t fn,const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *out)
{
    (void)name; assert(stack>=4096 && priority==1);
    task_t *t=calloc(1,sizeof(*t)); assert(t); sync_init(&t->sync);
    atomic_init(&t->retired,false);t->fn=fn; t->arg=arg; *out=t; last_task=t;
    assert(pthread_create(&t->thread,NULL,thread_main,t)==0); return pdPASS;
}
void xTaskNotifyGive(TaskHandle_t h) { signal_sync(&((task_t *)h)->sync); }
unsigned ulTaskNotifyTake(BaseType_t clear,TickType_t ticks)
{
    sync_t *s=&current->sync; pthread_mutex_lock(&s->m);
    if(ticks==portMAX_DELAY) while(!s->count) pthread_cond_wait(&s->c,&s->m);
    else if(!s->count) {
        assert(ticks>=2000 && ticks<=30000);
        struct timespec t; timespec_get(&t,TIME_UTC); t.tv_nsec+=10000000;
        if(t.tv_nsec>=1000000000) { ++t.tv_sec; t.tv_nsec-=1000000000; }
        (void)pthread_cond_timedwait(&s->c,&s->m,&t);
    }
    unsigned n=s->count; if(clear)s->count=0; else if(n)--s->count;
    bool retired=atomic_load(&current->retired);
    pthread_mutex_unlock(&s->m);
    if(retired)pthread_exit(NULL);
    return n;
}
void vTaskDelete(TaskHandle_t h)
{ assert(h);task_t *t=h;atomic_store(&t->retired,true);signal_sync(&t->sync); }
QueueHandle_t xQueueCreate(unsigned length,unsigned size)
{ assert(length==1 && size<=32); queue_t *q=calloc(1,sizeof(*q)); assert(q); pthread_mutex_init(&q->m,NULL); q->size=size; return q; }
BaseType_t xQueueSend(QueueHandle_t h,const void *p,TickType_t ticks)
{ assert(ticks==0); queue_t *q=h; pthread_mutex_lock(&q->m); bool ok=!q->full; if(ok){memcpy(q->data,p,q->size);q->full=true;} pthread_mutex_unlock(&q->m);return ok; }
BaseType_t xQueueReceive(QueueHandle_t h,void *p,TickType_t ticks)
{ assert(ticks==0); queue_t *q=h; pthread_mutex_lock(&q->m); bool ok=q->full; if(ok){memcpy(p,q->data,q->size);q->full=false;} pthread_mutex_unlock(&q->m);return ok; }
void vQueueDelete(QueueHandle_t h) { queue_t *q=h; pthread_mutex_destroy(&q->m);free(q); }
SemaphoreHandle_t xSemaphoreCreateBinary(void) { sync_t *s=calloc(1,sizeof(*s));assert(s);sync_init(s);return s; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t h) { signal_sync(h);return pdTRUE; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t h,TickType_t ticks)
{ assert(ticks==portMAX_DELAY);sync_t *s=h;pthread_mutex_lock(&s->m);while(!s->count)pthread_cond_wait(&s->c,&s->m);--s->count;pthread_mutex_unlock(&s->m);return pdTRUE; }
void vSemaphoreDelete(SemaphoreHandle_t h) { sync_destroy(h);free(h); }
esp_err_t game_battery_create(game_battery_handle_t *out)
{
    *out=NULL; unsigned n=atomic_fetch_add(&calls,1)+1; if(n<=failures)return ESP_FAIL;
    pthread_mutex_lock(&gate.m); entered=true;pthread_cond_broadcast(&gate.c);
    while(!released)pthread_cond_wait(&gate.c,&gate.m);
    pthread_mutex_unlock(&gate.m);
    *out=(game_battery_handle_t)malloc(1);assert(*out);return ESP_OK;
}
void game_battery_delete(game_battery_handle_t h) { assert(h);++deleted;free(h); }
bool game_battery_poll(game_battery_handle_t h,int64_t now,game_battery_reading_t *out)
{ assert(h && now==123);++polls;*out=(game_battery_reading_t){67,GAME_BATTERY_DISCHARGING};return true; }
static void finish(game_battery_service_handle_t s)
{ game_battery_service_delete(s);assert(pthread_join(last_task->thread,NULL)==0);sync_destroy(&last_task->sync);free(last_task); }
static void *stop_thread(void *s) { finish(s);stopped=true;return NULL; }
static void reset(unsigned fail_count,bool release)
{ calls=0;deleted=0;polls=0;failures=fail_count;entered=false;released=release; }
int main(void)
{
#ifdef _WIN32
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    game_battery_service_handle_t s=NULL;game_battery_reading_t r;
    reset(2,false);assert(game_battery_service_create(&s)==ESP_OK && s);
    pthread_mutex_lock(&gate.m);while(!entered)pthread_cond_wait(&gate.c,&gate.m);pthread_mutex_unlock(&gate.m);
    /* The background initializer is deliberately blocked. UI must not wait. */
    for(unsigned i=0;i<100;++i)assert(!game_battery_service_poll(s,123,&r));
    assert(calls==3 && polls==0);
    pthread_mutex_lock(&gate.m);released=true;pthread_cond_broadcast(&gate.c);pthread_mutex_unlock(&gate.m);
    bool ready=false;for(unsigned i=0;i<1000 && !ready;++i){ready=game_battery_service_poll(s,123,&r);if(!ready)short_wait();}
    assert(ready && r.percent==67 && r.state==GAME_BATTERY_DISCHARGING);
    finish(s);assert(deleted==1 && calls==3);
    /* Stop while retrying: wake the backoff, don't forcibly kill I2C work. */
    reset(100000,true);assert(game_battery_service_create(&s)==ESP_OK);
    while(calls==0)short_wait();
    finish(s);assert(deleted==0);
    /* A successful handle waiting in the handoff queue is cleaned up too. */
    reset(0,true);assert(game_battery_service_create(&s)==ESP_OK);
    while(calls==0)short_wait();
    short_wait();finish(s);assert(deleted==1);
    /* Shutdown must wait, rather than kill a task holding an I2C transaction. */
    reset(0,false);stopped=false;assert(game_battery_service_create(&s)==ESP_OK);
    pthread_mutex_lock(&gate.m);while(!entered)pthread_cond_wait(&gate.c,&gate.m);pthread_mutex_unlock(&gate.m);
    pthread_t stopper;assert(pthread_create(&stopper,NULL,stop_thread,s)==0);
    for(unsigned i=0;i<10;++i)short_wait();
    assert(!stopped);
    pthread_mutex_lock(&gate.m);released=true;pthread_cond_broadcast(&gate.c);pthread_mutex_unlock(&gate.m);
    assert(pthread_join(stopper,NULL)==0 && stopped && deleted==1);
    return 0;
}
