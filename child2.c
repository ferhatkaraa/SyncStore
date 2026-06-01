#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include "storage.h"

/* Child2'in çalıştığında yapması gereken ana task */
void child2_function(int param, SharedData *data, int semid) {
    if (param != 2) {
        printf("Child2: Parametre %d - Calismiyorum (sadece 2 kabul edilir)\n", param);
        return;
    }
    
    printf("Child2 (PID %d): Storage gorevini basliyorum...\n", (int)getpid());
    child2_storage_task(param, data, semid);
}

/* Child2'in storage üzerinde yapacağı işler */
void child2_storage_task(int param, SharedData *data, int semid) {
    const int child_id = 2;
    const int interval = 3;  /* 3 saniyede bir işlem */
    const char key[] = "ortak_depo";
    time_t start_time;
    int operation_count = 0;

    if (param != child_id) {
        printf("Child2: Parametre %d - Calismiyorum (sadece 2 kabul edilir)\n", param);
        return;
    }

    if (data == NULL) {
        printf("Child2: Paylasimli bellek baglantisi yok.\n");
        return;
    }

    printf("Child2 (PID %d): 60 saniyelik gorev basladi. Periyot: %d saniye.\n", 
           (int)getpid(), interval);
    start_time = time(NULL);

    while (time(NULL) - start_time < 60) {
        int elapsed = (int)(time(NULL) - start_time);

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
