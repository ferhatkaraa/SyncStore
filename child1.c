#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include "child1.h"
#include "storage.h"

/* Child1'in çalıştığında yapması gereken ana task */
void child1_function(int param, int shmid, int semid) {
    if (param != 1) {
        printf("Child1: Parametre %d - Calismiyorum (sadece 1 kabul edilir)\n", param);
        return;
    }

    SharedData *data = (SharedData *)shmat(shmid, NULL, 0);
    if (data == (void *)-1) {
        perror("Child1 shmat");
        return;
    }
    
    printf("Child1 (PID %d): Storage gorevini basliyorum...\n", (int)getpid());
    child1_storage_task(param, data, semid);

    if (shmdt(data) < 0) {
        perror("Child1 shmdt");
    }
}

/* Child1'in storage üzerinde yapacağı işler */
void child1_storage_task(int param, SharedData *data, int semid) {
    const int child_id = 1;
    const char shared_key[] = "ortak_depo";
    const char own_key[] = "child1_key";
    time_t start_time;
    int operation_count = 0;
    int interval;

    if (param != child_id) {
        printf("Child1: Parametre %d - Calismiyorum (sadece 1 kabul edilir)\n", param);
        return;
    }

    if (data == NULL) {
        printf("Child1: Paylasimli bellek baglantisi yok.\n");
        return;
    }

    interval = data->interval1;
    if (interval <= 0) interval = 2;
    printf("Child1 (PID %d): 60 saniyelik gorev basladi. Periyot: %d saniye.\n", 
           (int)getpid(), interval);
    start_time = time(NULL);

    while (time(NULL) - start_time < 60) {
        int elapsed = (int)(time(NULL) - start_time);
        interval = data->interval1;
        if (interval <= 0) interval = 2;

        /* Faz 1 (0-20 sn): SET komutunu test et.
         * Child1 hem kendi key'ini hem de diger child'larla cakisan ortak key'i yazar. */
        if (elapsed < 20) {
            int value = (int)getpid() + operation_count;
            storage_write(data, semid, own_key, value, child_id);
            storage_write(data, semid, shared_key, value + 1000, child_id);
        } 
        /* Faz 2 (20-40 sn): GET + LIST komutlarini test et.
         * LIST, tum shared memory deposunun semaphore altinda tutarli goruntusunu verir. */
        else if (elapsed < 40) {
            storage_read(data, semid, own_key, child_id);
            storage_read(data, semid, shared_key, child_id);
            storage_list(data, semid, child_id);
        }
        /* Faz 3 (40-60 sn): DELETE + LIST komutlarini test et.
         * Bazen kendi key'i, bazen ortak key silinir; cakisan silmeler race condition
         * olusturmadan "key bulunamadi" olarak raporlanir. */
        else {
            const char *delete_key = (operation_count % 2 == 0) ? own_key : shared_key;
            storage_delete(data, semid, delete_key, child_id);
            storage_list(data, semid, child_id);
        }

        operation_count++;
        sleep(interval);
    }

    printf("Child1 (PID %d): Gorev tamamlandi. (toplam %d islem)\n", 
           (int)getpid(), operation_count);
}
