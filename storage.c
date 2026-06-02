#include "storage.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <sys/sem.h>
#include <time.h>
#include <unistd.h>

#define STORAGE_CAPACITY 100
#define STORAGE_LOG_FILE "syncstore.log"

/* Storage islemlerinin normal kod akisindaki ortak log fonksiyonu.
 * Sinyal handler icinden cagrilmaz; bu nedenle printf-benzeri formatlama
 * kullanabilir. SET/GET/DELETE/LIST ciktilarini hem ekrana hem log dosyasina
 * yazmak icin tek noktadan kullanilir. */
static void storage_log_printf(const char *fmt, ...) {
    char buffer[512];
    va_list args;
    int n;
    int fd;

    va_start(args, fmt);
    n = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (n < 0) return;
    if ((size_t)n >= sizeof(buffer)) {
        n = (int)sizeof(buffer) - 1;
        buffer[n] = '\0';
    }

    fputs(buffer, stdout);
    fflush(stdout);

    fd = open(STORAGE_LOG_FILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd != -1) {
        (void)!write(fd, buffer, (size_t)n);
        close(fd);
    }
}

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
    if (semop(semid, &operasyon, 1) < 0) {
        perror("kilitle semop");
    }
}

// --- KALEMİ BIRAK (KİLİDİ AÇ) ---
void kilidi_ac(int semid) {
    // İşletim sistemine diyoruz ki: "İşim bitti, kalemi masaya bırakıyorum, bekleyen varsa gelsin."
    struct sembuf operasyon = {0, 1, 0};
    if (semop(semid, &operasyon, 1) < 0) {
        perror("kilidi_ac semop");
    }
}

// --- VERİ YAZMA ---
void storage_write(SharedData *data, int semid, const char *key, int value, int child_id) {
    int index;

    if (data == NULL || key == NULL) {
        storage_log_printf("Child %d: SET basarisiz, gecersiz storage verisi.\n", child_id);
        return;
    }

    kilitle(semid); // Kapıyı kilitledim, içeri girdim.

    index = find_key_index(data, key);
    if (index == -1) {
        if (data->count >= STORAGE_CAPACITY) {
            storage_log_printf("Child %d: SET basarisiz, storage dolu. Key: %s\n", child_id, key);
            kilidi_ac(semid);
            return;
        }

        index = data->count;
        data->count++;
    }

    snprintf(data->db[index].key, sizeof(data->db[index].key), "%s", key);
    data->db[index].value = value;
    data->db[index].last_update = time(NULL);
    storage_log_printf("Child %d: SET Yazdim: %s = %d (slot: %d)\n",
                       child_id, data->db[index].key, value, index);

    kilidi_ac(semid); // Kapıyı açtım, çıktım.
}

// --- VERİ OKUMA ---
int storage_read(SharedData *data, int semid, const char *key, int child_id) {
    int index;

    if (data == NULL || key == NULL) {
        storage_log_printf("Child %d: GET basarisiz, gecersiz storage verisi.\n", child_id);
        return -1;
    }

    kilitle(semid); // Kapıyı kilitledim, içeri girdim.

    index = find_key_index(data, key);
    if (index == -1) {
        storage_log_printf("Child %d: GET basarisiz, key bulunamadi: %s\n", child_id, key);
        kilidi_ac(semid);
        return -1;
    }

    int okunan_deger = data->db[index].value;
    storage_log_printf("Child %d: GET Okudum: %s = %d (slot: %d, son guncelleme: %ld)\n",
                       child_id,
                       data->db[index].key,
                       okunan_deger,
                       index,
                       (long)data->db[index].last_update);

    kilidi_ac(semid); // Kapıyı açtım, çıktım.
    return okunan_deger;
}

// --- VERİ SİLME (DELETE) ---
/* DELETE komutu shared memory uzerindeki db[] dizisini degistirdigi icin
 * mutlaka semaphore kilidi altinda calisir. Key bulunursa silinen slotun
 * yerine dizinin son kaydi kopyalanir ve count azaltılır. Bu yontem kayitlari
 * kaydirmadan O(1) maliyetle silme yapar; siralama korunmak zorunda degildir. */
int storage_delete(SharedData *data, int semid, const char *key, int child_id) {
    int index;
    int last_index;

    if (data == NULL || key == NULL) {
        storage_log_printf("Child %d: DELETE basarisiz, gecersiz storage verisi.\n", child_id);
        return -1;
    }

    kilitle(semid);

    index = find_key_index(data, key);
    if (index == -1) {
        storage_log_printf("Child %d: DELETE basarisiz, key bulunamadi: %s\n", child_id, key);
        kilidi_ac(semid);
        return -1;
    }

    last_index = data->count - 1;
    storage_log_printf("Child %d: DELETE Sildim: %s = %d (slot: %d)\n",
                       child_id,
                       data->db[index].key,
                       data->db[index].value,
                       index);

    if (index != last_index) {
        /* Bosluk kalmamasi icin son elemani silinen slotun uzerine aliyoruz. */
        data->db[index] = data->db[last_index];
    }

    /* Eski son slotu temizleyip aktif kayit sayisini bir azalt. */
    memset(&data->db[last_index], 0, sizeof(data->db[last_index]));
    data->count--;

    kilidi_ac(semid);
    return 0;
}

// --- VERİ LİSTELEME (LIST) ---
/* LIST komutu veriyi sadece okur; yine de tutarli bir anlik goruntu almak icin
 * semaphore kilidi alir. Boylece baska process ayni anda SET/DELETE yaparken
 * count ve slot bilgileri yari guncellenmis halde yazdirilmaz. */
void storage_list(SharedData *data, int semid, int child_id) {
    if (data == NULL) {
        storage_log_printf("Child %d: LIST basarisiz, gecersiz storage verisi.\n", child_id);
        return;
    }

    kilitle(semid);

    storage_log_printf("Child %d: LIST basladi. Kayit sayisi: %d\n", child_id, data->count);
    if (data->count == 0) {
        storage_log_printf("Child %d: LIST depo bos.\n", child_id);
    } else {
        for (int i = 0; i < data->count; i++) {
            storage_log_printf("Child %d: LIST slot=%d key=%s value=%d son_guncelleme=%ld\n",
                               child_id,
                               i,
                               data->db[i].key,
                               data->db[i].value,
                               (long)data->db[i].last_update);
        }
    }
    storage_log_printf("Child %d: LIST bitti.\n", child_id);

    kilidi_ac(semid);
}
