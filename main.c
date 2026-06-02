#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <errno.h>
#include "child1.h"
#include "child2.h"
#include "child3.h"
#include "sinyal.h"
#include "storage.h"

int main() {

    pid_t pid;
    int shmid, semid;
    SharedData *shared_data;
    int i;

    /* ===== ADIM 1: Sinyal yonetimini setup et ===== */
    signal_function();
    printf("[MAIN] Sinyal yonetimi kuruldu.\n");

    /* ===== ADIM 2: Paylaşimlı bellek oluştur ===== */
    shmid = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    
    shared_data = (SharedData *)shmat(shmid, NULL, 0);
    if (shared_data == (void *)-1) {
        perror("shmat");
        shmctl(shmid, IPC_RMID, NULL);
        exit(1);
    }

    /* Paylaşımlı bellek başlangıç değerlerini set et */
    shared_data->count = 0;
    shared_data->interval1 = 2;
    shared_data->interval2 = 3;
    shared_data->interval3 = 4;
    shared_data->config_version = 1;
    printf("[MAIN] Paylaşimlı bellek oluşturuldu (shmid=%d, Size=%zu bytes).\n", 
           shmid, sizeof(SharedData));

    /* ===== ADIM 3: Semaphore oluştur (mutex) ===== */
    semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (semid < 0) {
        perror("semget");
        shmdt(shared_data);
        shmctl(shmid, IPC_RMID, NULL);
        exit(1);
    }

    /* Semaphore'u 1 olarak initialize et (açık durumda = herkes girebilir) */
    if (semctl(semid, 0, SETVAL, 1) < 0) {
        perror("semctl SETVAL");
        shmdt(shared_data);
        shmctl(shmid, IPC_RMID, NULL);
        semctl(semid, 0, IPC_RMID);
        exit(1);
    }

    printf("[MAIN] Semaphore oluşturuldu (semid=%d) - initial value: 1\n", semid);

    /* ===== ADIM 4: Sinyal modulune IPC kaynaklarini kaydet ===== */
    sinyal_kaynak_kaydet(shmid, semid);

    printf("C Programi Baslatiliyor...\n");
    
    /* ===== ADIM 5: 3 child process fork et ===== */
    for (i = 1; i <= 3; i++)
    {
        pid = fork();
        
        if (pid < 0) {
            perror("Fork basarisiz");
            exit(1);
        }
        else if (pid == 0) {
            /* ===== CHILD PROCESS ===== */
            if (i == 1) {
                child1_function(i, shmid, semid);
            } else if (i == 2) {
                child2_function(i, shmid, semid);
            } else if (i == 3) {
                child3_function(i, shmid, semid);
            }
            exit(0);
        }
        else {
            /* ===== PARENT PROCESS ===== */
            /* Fork edilen child'in PID'sini sinyal moduluna kaydet.
             * Boylece Ctrl+C (SIGINT) gelince handler tum child'lari kapatabilir. */
            sinyal_child_ekle(pid);
            printf("[MAIN] Child %d fork edildi (PID=%d)\n", i, pid);
        }
    }

    /* ===== ADIM 5B: Yalnizca parent'ta asenkron klavye dinlemesi baslat ===== */
    sinyal_klavye_baslat();
    sinyal_config_baslat();
    printf("[MAIN] Asenkron klavye dinlemesi baslatildi.\n");

    /* ===== ADIM 6: Client child'lari bekle, fakat server'i kapatma ===== */
    printf("[MAIN] Tum child client'larin bitmesini bekliyorum...\n");
    for (i = 0; i < 3; i++) {
        int status;
        pid_t terminated_pid = wait(&status);
        if (terminated_pid < 0) {
            if (errno == ECHILD) {
                printf("[MAIN] Beklenecek child kalmadi.\n");
                break;
            }
            perror("wait");
            break;
        }
        printf("[MAIN] Child bitti (PID=%d)\n", terminated_pid);
    }

    /* ===== ADIM 7: KALICI SERVER MODU =====
     * Odev isterine gore server process child client'lar bitince kapanmamalidir.
     * Bu nedenle burada normal cleanup akisini calistirmiyoruz; parent process
     * pause() ile sinyal/klavye thread'lerinden gelecek kapanma istegini bekler.
     *
     * Kaynak temizligi sadece sinyal.c icindeki graceful shutdown handler'inda
     * yapilir: keyboard thread 'q'/'Q' basinca SIGINT gonderir, Ctrl+C veya
     * SIGTERM de ayni guvenli kapanma yolunu kullanir. */
    printf("[MAIN] Tum child client'lar tamamlandi.\n");
    printf("[MAIN] Server calismaya devam ediyor. Cikis icin 'q' veya Ctrl+C kullanin.\n");
    while (1) {
        pause();
    }
}
