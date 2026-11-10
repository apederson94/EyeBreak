#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef int WakeupId;

#define APP_LOG_LEVEL_INFO 1
#define APP_LOG_LEVEL_ERROR 3

#define APP_LOG(level, ...) ((void)0)

int persist_write_int(int key, int32_t value);
int persist_write_data(int key, const void *data, int size);
int persist_get_size(int key);
int persist_read_data(int key, void *data, int size);
int persist_read_int(int key);
bool persist_exists(int key);

void wakeup_cancel(WakeupId id);
void wakeup_cancel_all(void);
void wakeup_service_subscribe(void (*handler)(WakeupId, int32_t));
WakeupId wakeup_schedule(time_t when, int32_t cookie, ...);
