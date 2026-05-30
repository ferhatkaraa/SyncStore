#include "storage.h"
#include <stdio.h>
#include <string.h>
#include <sys/sem.h>
#include <time.h>

#define STORAGE_CAPACITY 100

static int find_key_index(SharedData *data, const char *key) {
    for (int i = 0; i < data->count; i++) {
        if (strncmp(data->db[i].key, key, sizeof(data->db[i].key)) == 0) {
            return i;
        }
    }

    return -1;
}

// --- KALEMİ AL (KİLİTLE) ---
void kilitle(int semid) {
    // İşletim sistemine diyoruz ki: "Kalemi alacağım, eğer kalem başkasındaysa beni burada beklet."
    struct sembuf operasyon = {0, -1, 0}; 
    semop(semid, &operasyon, 1);
}

// --- KALEMİ BIRAK (KİLİDİ AÇ) ---
void kilidi_ac(int semid) {
    // İşletim sistemine diyoruz ki: "İşim bitti, kalemi masaya bırakıyorum, bekleyen varsa gelsin."
    struct sembuf operasyon = {0, 1, 0};
    semop(semid, &operasyon, 1);
}

// --- VERİ YAZMA ---
void storage_write(SharedData *data, int semid, const char *key, int value, int child_id) {
    int index;

    if (data == NULL || key == NULL) {
        printf("Child %d: Yazma basarisiz, gecersiz storage verisi.\n", child_id);
        return;
    }

    kilitle(semid); // Kapıyı kilitledim, içeri girdim.

    index = find_key_index(data, key);
    if (index == -1) {
        if (data->count >= STORAGE_CAPACITY) {
            printf("Child %d: Yazma basarisiz, storage dolu. Key: %s\n", child_id, key);
            kilidi_ac(semid);
            return;
        }

        index = data->count;
        data->count++;
    }

    snprintf(data->db[index].key, sizeof(data->db[index].key), "%s", key);
    data->db[index].value = value;
    data->db[index].last_update = time(NULL);
    printf("Child %d: Yazdim: %s = %d (slot: %d)\n", child_id, data->db[index].key, value, index);

    kilidi_ac(semid); // Kapıyı açtım, çıktım.
}

// --- VERİ OKUMA ---
int storage_read(SharedData *data, int semid, const char *key, int child_id) {
    int index;

    if (data == NULL || key == NULL) {
        printf("Child %d: Okuma basarisiz, gecersiz storage verisi.\n", child_id);
        return -1;
    }

    kilitle(semid); // Kapıyı kilitledim, içeri girdim.

    index = find_key_index(data, key);
    if (index == -1) {
        printf("Child %d: Okuma basarisiz, key bulunamadi: %s\n", child_id, key);
        kilidi_ac(semid);
        return -1;
    }

    int okunan_deger = data->db[index].value;
    printf("Child %d: Okudum: %s = %d (slot: %d, son guncelleme: %ld)\n",
           child_id,
           data->db[index].key,
           okunan_deger,
           index,
           (long)data->db[index].last_update);

    kilidi_ac(semid); // Kapıyı açtım, çıktım.
    return okunan_deger;
}
