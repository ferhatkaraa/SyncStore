#ifndef CHILD1_H
#define CHILD1_H

#include "storage.h"

void child1_function(int param, int shmid, int semid);
void child1_storage_task(int param, SharedData *data, int semid);

#endif
