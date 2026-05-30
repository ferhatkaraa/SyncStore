#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "storage.h"

void child3_function(int param) {
    printf("Child3: Parametre %d alindi. Storage gorevi icin SharedData ve semid gerekli.\n", param);
}

void child3_storage_task(int param, SharedData *data, int semid) {
    const int child_id = 3;
    const int interval = 4;
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

    printf("Child3: 60 saniyelik gorev basladi. Periyot: %d saniye.\n", interval);
    start_time = time(NULL);

    while (time(NULL) - start_time < 60) {
        int elapsed = (int)(time(NULL) - start_time);

        if (elapsed < 30) {
            int value = (int)getpid() + operation_count;
            storage_write(data, semid, key, value, child_id);
        } else {
            storage_read(data, semid, key, child_id);
        }

        operation_count++;
        sleep(interval);
    }

    printf("Child3: Gorev tamamlandi.\n");
}
