// storage.h
#ifndef STORAGE_H
#define STORAGE_H

#include <string.h>
#include <time.h>

// Her bir anahtar-değer çifti için küçük struct
typedef struct {
    char key[32];   // "sicaklik", "hiz" vb.
    int value;      // 25, 100 vb.
    time_t last_update;
} KeyValue;

// Tüm veri tabanını ve kayıt sayısını tutan ana struct
typedef struct {
    KeyValue db[100]; // 100 adet kayıt kapasitesi
    int count;        // Şu an içinde kaç tane kayıt var?
} SharedData;

// Fonksiyon prototipleri (Diğer dosyalar bu fonksiyonları tanısın diye)
void storage_write(SharedData *data, int semid, const char *key, int value, int child_id);
int storage_read(SharedData *data, int semid, const char *key, int child_id);
void kilitle(int semid);
void kilidi_ac(int semid);

#endif
