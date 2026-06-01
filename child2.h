#ifndef CHILD2_H
#define CHILD2_H

#include "storage.h"

void child2_function(int param, int shmid, int semid);
void child2_storage_task(int param, SharedData *data, int semid);

#endif
