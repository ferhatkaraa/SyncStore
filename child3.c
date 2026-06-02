#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include "child3.h"
#include "storage.h"

/* Child3'in çalıştığında yapması gereken ana task */
void child3_function(int param, int shmid, int semid) {
    if (param != 3) {
        printf("Child3: Parametre %d - Calismiyorum (sadece 3 kabul edilir)\n", param);
        return;
    }

    SharedData *data = (SharedData *)shmat(shmid, NULL, 0);
    if (data == (void *)-1) {
        perror("Child3 shmat");
        return;
    }
    
    printf("Child3 (PID %d): Storage gorevini basliyorum...\n", (int)getpid());
    child3_storage_task(param, data, semid);

    if (shmdt(data) < 0) {
        perror("Child3 shmdt");
    }
}

/* Child3'in storage üzerinde yapacağı işler */
void child3_storage_task(int param, SharedData *data, int semid) {
    const int child_id = 3;
    const char shared_key[] = "ortak_depo";
    const char own_key[] = "child3_key";
    time_t start_time;
    int operation_count = 0;
    int interval;

    if (param != child_id) {
        printf("Child3: Parametre %d - Calismiyorum (sadece 3 kabul edilir)\n", param);
        return;
    }

    if (data == NULL) {
        printf("Child3: Paylasimli bellek baglantisi yok.\n");
        return;
    }

    interval = data->interval3;
    if (interval <= 0) interval = 4;
    printf("Child3 (PID %d): 60 saniyelik gorev basladi. Periyot: %d saniye.\n", 
           (int)getpid(), interval);
    start_time = time(NULL);

    while (time(NULL) - start_time < 60) {
        int elapsed = (int)(time(NULL) - start_time);
        interval = data->interval3;
        if (interval <= 0) interval = 4;

        /* Faz 1 (0-20 sn): SET komutunu test et.
         * Child3 kendi kaydini olusturur ve ortak key uzerinde diger child'larla yarisir. */
        if (elapsed < 20) {
            int value = (int)getpid() + operation_count;
            storage_write(data, semid, own_key, value, child_id);
            storage_write(data, semid, shared_key, value + 3000, child_id);
        } 
        /* Faz 2 (20-40 sn): GET + LIST komutlarini test et.
         * Listeleme sayesinde ayni anda calisan istemcilerin depoda biraktigi tum kayitlar gorulur. */
        else if (elapsed < 40) {
            storage_read(data, semid, own_key, child_id);
            storage_read(data, semid, shared_key, child_id);
            storage_list(data, semid, child_id);
        }
        /* Faz 3 (40-60 sn): DELETE + LIST komutlarini test et.
         * Silme sonrasi LIST cagrisi, count degerinin ve slotlarin tutarli kaldigini gosterir. */
        else {
            const char *delete_key = (operation_count % 2 == 0) ? own_key : shared_key;
            storage_delete(data, semid, delete_key, child_id);
            storage_list(data, semid, child_id);
        }

        operation_count++;
        sleep(interval);
    }

    printf("Child3 (PID %d): Gorev tamamlandi. (toplam %d islem)\n", 
           (int)getpid(), operation_count);
}
