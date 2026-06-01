/*
 * sinyal.c - SyncStore Sinyal Yonetim Modulu
 * ===============================================================
 * Sorumluluk: Sunucu (main) process'inin sinyallerini yonetmek.
 *
 * EN ONEMLI GOREV (zorunlu):
 *   CTRL+C (SIGINT) geldiginde -> ONCE tum child process'ler duzgun
 *   kapatilir (her birine SIGTERM gonderilip waitpid ile beklenir),
 *   SONRA shared memory + semaphore kaynaklari temizlenir ve cikilir.
 *
 * Async-signal-safety notu:
 *   Sinyal handler'i icinde printf/fprintf gibi fonksiyonlar GUVENLI
 *   DEGILDIR (man signal-safety). Bu yuzden handler icinde sadece
 *   guvenli olan write(), kill(), waitpid(), getpid(), _exit(), shmctl(),
 *   semctl() cagrilarini kullaniyoruz. Ekrana/log'a yazi basmak icin de
 *   kendi yazdigimiz write() tabanli yardimcilari kullaniyoruz.
 *
 * Parent / Child ayrimi:
 *   signal_function() fork'lardan ONCE cagrildigi icin, fork edilen
 *   child'lar da bu handler'lari miras alir. Handler icinde getpid()'i
 *   kurulum anindaki ana PID ile karsilastiririz: eger bir child handler'a
 *   girerse, baskalarini kapatmaya veya IPC silmeye CALISMAZ; sadece kendini
 *   temiz sekilde sonlandirir. Boylece main.c'de ekstra degisiklik gerekmez.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define MAX_CHILD   64
#define LOG_DOSYASI "syncstore.log"

/* ----------------------- Global durum -----------------------
 * Handler icinde okunan degiskenler sig_atomic_t / basit tiplerdir.
 * Child PID dizisine yazma yalnizca parent'ta (handler disinda) olur,
 * okuma handler icinde olur; kapanma sirasinda yarisma olusmaz. */
static pid_t              g_child_pidler[MAX_CHILD];
static volatile sig_atomic_t g_child_sayisi = 0;
static pid_t              g_ana_pid        = 0;   /* kurulumdaki ana process PID */
static int                g_shmid          = -1;  /* temizlenecek shared memory id */
static int                g_semid          = -1;  /* temizlenecek semaphore id */
static int                g_log_fd         = -1;  /* log dosyasi fd (append) */
static volatile sig_atomic_t g_kapaniyor   = 0;   /* tekrar girisi engeller */

/* ============================================================
 *   Async-signal-safe yazdirma yardimcilari
 * ============================================================ */

/* Verilen yaziyi hem ekrana (stderr) hem log dosyasina yazar.
 * Handler icinden cagrilabilir (write() guvenlidir). */
static void guvenli_yaz(const char *s) {
    size_t n = 0;
    while (s[n] != '\0') n++;
    /* Donus degerlerini bilerek goz ardi ediyoruz; handler icinde
     * yapilacak makul bir sey yok. (void) ile derleyici uyarisini susturuyoruz. */
    (void)!write(STDERR_FILENO, s, n);
    if (g_log_fd != -1) (void)!write(g_log_fd, s, n);
}

/* Bir tam sayiyi async-signal-safe sekilde yazar. */
static void guvenli_yaz_sayi(long sayi) {
    char  ters[24];
    char  cikti[26];
    int   i = 0, j = 0, negatif = 0;

    if (sayi < 0) { negatif = 1; sayi = -sayi; }
    if (sayi == 0) ters[i++] = '0';
    while (sayi > 0) { ters[i++] = (char)('0' + (sayi % 10)); sayi /= 10; }

    if (negatif) cikti[j++] = '-';
    while (i > 0) cikti[j++] = ters[--i];

    (void)!write(STDERR_FILENO, cikti, (size_t)j);
    if (g_log_fd != -1) (void)!write(g_log_fd, cikti, (size_t)j);
}

/* ============================================================
 *   Normal (handler disi) loglama
 * ============================================================ */
void sinyal_log(const char *mesaj) {
    char       satir[256];
    time_t     simdi = time(NULL);
    struct tm *zt    = localtime(&simdi);
    int        n;

    n = snprintf(satir, sizeof(satir), "[%02d:%02d:%02d] %s\n",
                 zt ? zt->tm_hour : 0, zt ? zt->tm_min : 0, zt ? zt->tm_sec : 0,
                 mesaj ? mesaj : "");
    if (n < 0) return;

    (void)!write(STDERR_FILENO, satir, (size_t)n);
    if (g_log_fd != -1) (void)!write(g_log_fd, satir, (size_t)n);
}

/* ============================================================
 *   ZORUNLU: Guvenli kapatma (SIGINT / SIGTERM)
 * ============================================================ */
static void kapatma_handler(int sig) {
    int kalan = (int)g_child_sayisi;
    int i;

    /* --- Child ise: kimseyi kapatma, IPC silme, sadece kendini bitir --- */
    if (getpid() != g_ana_pid) {
        guvenli_yaz("[CHILD ");
        guvenli_yaz_sayi((long)getpid());
        guvenli_yaz("] Kapatma sinyali alindi, cikiyorum.\n");
        _exit(0);
    }

    /* --- Parent: ayni anda iki kez girmeyi engelle --- */
    if (g_kapaniyor) return;
    g_kapaniyor = 1;

    guvenli_yaz("\n[SERVER] Sinyal alindi (no=");
    guvenli_yaz_sayi((long)sig);
    guvenli_yaz("). ONCE tum child'lar kapatiliyor...\n");

    /* 1) Tum child'lara nazik kapanma istegi (SIGTERM) gonder */
    for (i = 0; i < kalan; i++) {
        if (g_child_pidler[i] > 0) {
            kill(g_child_pidler[i], SIGTERM);
        }
    }

    /* 2) Hepsinin gercekten kapanmasini bekle (zombie birakma) */
    for (i = 0; i < kalan; i++) {
        if (g_child_pidler[i] > 0) {
            int durum;
            while (waitpid(g_child_pidler[i], &durum, 0) < 0 && errno == EINTR)
                ; /* sinyalle bolunduyse tekrar bekle */
            guvenli_yaz("[SERVER] Child kapandi: PID ");
            guvenli_yaz_sayi((long)g_child_pidler[i]);
            guvenli_yaz("\n");
        }
    }

    guvenli_yaz("[SERVER] Tum child'lar kapandi. IPC kaynaklari temizleniyor...\n");

    /* 3) IPC kaynaklarini temizle (kaydedilmislerse) */
    if (g_shmid != -1) {
        if (shmctl(g_shmid, IPC_RMID, NULL) == 0)
            guvenli_yaz("[SERVER] Shared memory silindi.\n");
    }
    if (g_semid != -1) {
        if (semctl(g_semid, 0, IPC_RMID) == 0)
            guvenli_yaz("[SERVER] Semaphore silindi.\n");
    }

    guvenli_yaz("[SERVER] Temiz cikis. Hosca kal.\n");
    _exit(0);
}

/* ============================================================
 *   EK OZELLIK: Istatistik (SIGUSR2)
 * ============================================================ */
static void istatistik_handler(int sig) {
    (void)sig;
    if (getpid() != g_ana_pid) return;   /* sadece server cevap versin */

    guvenli_yaz("[SERVER] === ISTATISTIK ===\n");
    guvenli_yaz("[SERVER] Aktif kayitli child sayisi: ");
    guvenli_yaz_sayi((long)g_child_sayisi);
    guvenli_yaz("\n[SERVER] Child PID listesi:");
    for (int i = 0; i < (int)g_child_sayisi; i++) {
        guvenli_yaz(" ");
        guvenli_yaz_sayi((long)g_child_pidler[i]);
    }
    guvenli_yaz("\n[SERVER] ====================\n");
}

/* ============================================================
 *   EK OZELLIK: Child'lara yayin (SIGUSR1 broadcast)
 * ============================================================ */
static void yayin_handler(int sig) {
    (void)sig;

    if (getpid() != g_ana_pid) {
        /* child tarafi: yayini aldi */
        guvenli_yaz("[CHILD ");
        guvenli_yaz_sayi((long)getpid());
        guvenli_yaz("] SIGUSR1 yayini alindi.\n");
        return;
    }

    guvenli_yaz("[SERVER] SIGUSR1 -> tum child'lara yayinlaniyor...\n");
    for (int i = 0; i < (int)g_child_sayisi; i++) {
        if (g_child_pidler[i] > 0) kill(g_child_pidler[i], SIGUSR1);
    }
}

/* ============================================================
 *   EK OZELLIK: Konfig yeniden yukle (SIGHUP)
 * ============================================================ */
static void konfig_handler(int sig) {
    (void)sig;
    if (getpid() != g_ana_pid) return;
    guvenli_yaz("[SERVER] SIGHUP alindi -> konfigurasyon yeniden yuklenecek.\n");
    /* Buraya ileride config dosyasi okuma eklenebilir. */
}

/* ============================================================
 *   Kurulum yardimcisi (sigaction)
 * ============================================================ */
static void handler_kur(int sinyal_no, void (*fn)(int), int restart) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = fn;
    sigfillset(&sa.sa_mask);            /* handler calisirken diger sinyalleri maskele */
    sa.sa_flags = restart ? SA_RESTART : 0;
    if (sigaction(sinyal_no, &sa, NULL) == -1) {
        perror("sigaction");
    }
}

/* ============================================================
 *   PUBLIC API
 * ============================================================ */
void signal_function(void) {
    g_ana_pid     = getpid();
    g_child_sayisi = 0;

    /* Log dosyasini ac (append). Acilamazsa sadece ekrana yazariz. */
    g_log_fd = open(LOG_DOSYASI, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (g_log_fd == -1) {
        perror("log dosyasi acilamadi");
    }

    /* ZORUNLU sinyaller */
    handler_kur(SIGINT,  kapatma_handler,    0); /* Ctrl+C: blocking wait yapacagiz */
    handler_kur(SIGTERM, kapatma_handler,    0);

    /* EK ozellikler */
    handler_kur(SIGUSR1, yayin_handler,      1);
    handler_kur(SIGUSR2, istatistik_handler, 1);
    handler_kur(SIGHUP,  konfig_handler,     1);

    sinyal_log("Sinyal modulu hazir. SIGINT/SIGTERM ile guvenli kapanma aktif.");
    sinyal_log("Ipuclari: kill -USR2 <pid> = istatistik, "
               "kill -USR1 <pid> = yayin, kill -HUP <pid> = config reload.");
}

void sinyal_child_ekle(pid_t pid) {
    if (pid <= 0) return;
    if (g_child_sayisi >= MAX_CHILD) {
        sinyal_log("UYARI: child PID listesi dolu, yeni PID kaydedilemedi.");
        return;
    }
    g_child_pidler[g_child_sayisi] = pid;
    g_child_sayisi = g_child_sayisi + 1;

    sinyal_log("Yeni child kaydedildi.");
}

void sinyal_kaynak_kaydet(int shmid, int semid) {
    g_shmid = shmid;
    g_semid = semid;
    sinyal_log("IPC kaynaklari kaydedildi (kapanista temizlenecek).");
}
