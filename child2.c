#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include "child2.h"
#include "storage.h"

/* Child2'in çalıştığında yapması gereken ana task */
void child2_function(int param, int shmid, int semid) {
    if (param != 2) {
        printf("Child2: Parametre %d - Calismiyorum (sadece 2 kabul edilir)\n", param);
        return;
    }

    SharedData *data = (SharedData *)shmat(shmid, NULL, 0);
    if (data == (void *)-1) {
        perror("Child2 shmat");
        return;
    }
    
    printf("Child2 (PID %d): Storage gorevini basliyorum...\n", (int)getpid());
    child2_storage_task(param, data, semid);

    if (shmdt(data) < 0) {
        perror("Child2 shmdt");
    }
}

/* Child2'in storage üzerinde yapacağı işler */
void child2_storage_task(int param, SharedData *data, int semid) {
    const int child_id = 2;
    const char key[] = "ortak_depo";
    time_t start_time;
    int operation_count = 0;
    int interval;

    if (param != child_id) {
        printf("Child2: Parametre %d - Calismiyorum (sadece 2 kabul edilir)\n", param);
        return;
    }

    if (data == NULL) {
        printf("Child2: Paylasimli bellek baglantisi yok.\n");
        return;
    }

    interval = data->interval2;
    if (interval <= 0) interval = 3;
    printf("Child2 (PID %d): 60 saniyelik gorev basladi. Periyot: %d saniye.\n", 
           (int)getpid(), interval);
    start_time = time(NULL);

    while (time(NULL) - start_time < 60) {
        int elapsed = (int)(time(NULL) - start_time);
        interval = data->interval2;
        if (interval <= 0) interval = 3;

        /* İlk 30 saniye: okuma işlemi */
        if (elapsed < 30) {
            storage_read(data, semid, key, child_id);
        } 
        /* Son 30 saniye: yazma işlemi */
        else {
            int value = (int)getpid() + operation_count;
            storage_write(data, semid, key, value, child_id);
        }

        operation_count++;
        sleep(interval);
    }

    printf("Child2 (PID %d): Gorev tamamlandi. (toplam %d islem)\n", 
           (int)getpid(), operation_count);
}
