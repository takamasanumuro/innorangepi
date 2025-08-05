#ifndef OFFLINE_QUEUE_H
#define OFFLINE_QUEUE_H

#include <stdbool.h>
#include "LineProtocol.h"

void offline_queue_init(void);
void offline_queue_add(const char* line_protocol);
void offline_queue_process(const InfluxDBContext* dbContext);

#endif // OFFLINE_QUEUE_H