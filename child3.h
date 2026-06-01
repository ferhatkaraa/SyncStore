#ifndef CHILD3_H
#define CHILD3_H

#include "storage.h"

void child3_function(int param, int shmid, int semid);
void child3_storage_task(int param, SharedData *data, int semid);

#endif
