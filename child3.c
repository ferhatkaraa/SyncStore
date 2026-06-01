#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include "storage.h"

/* Child3'in çalıştığında yapması gereken ana task */
void child3_function(int param, SharedData *data, int semid) {
    if (param != 3) {
        printf("Child3: Parametre %d - Calismiyorum (sadece 3 kabul edilir)\n", param);
        return;
    }
    
    printf("Child3 (PID %d): Storage gorevini basliyorum...\n", (int)getpid());
    child3_storage_task(param, data, semid);
}

/* Child3'in storage üzerinde yapacağı işler */
void child3_storage_task(int param, SharedData *data, int semid) {
    const int child_id = 3;
    const int interval = 4;  /* 4 saniyede bir işlem */
    const char key[] = "ortak_depo";
    time_t start_time;
    int operation_count = 0;

    if (param != child_id) {
        printf("Child3: Parametre %d - Calismiyorum (sadece 3 kabul edilir)\n", param);
        return;
    }

    if (data == NULL) {
        printf("Child3: Paylasimli bellek baglantisi yok.\n");
        return;
    }

    printf("Child3 (PID %d): 60 saniyelik gorev basladi. Periyot: %d saniye.\n", 
           (int)getpid(), interval);
    start_time = time(NULL);

    while (time(NULL) - start_time < 60) {
        int elapsed = (int)(time(NULL) - start_time);

        /* İlk 30 saniye: yazma işlemi */
        if (elapsed < 30) {
            int value = (int)getpid() + operation_count;
            storage_write(data, semid, key, value, child_id);
        } 
        /* Son 30 saniye: okuma işlemi */
        else {
            storage_read(data, semid, key, child_id);
        }

        operation_count++;
        sleep(interval);
    }

    printf("Child3 (PID %d): Gorev tamamlandi. (toplam %d islem)\n", 
           (int)getpid(), operation_count);
}
