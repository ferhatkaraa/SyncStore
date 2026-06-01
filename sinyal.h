#ifndef SINYAL_H
#define SINYAL_H

#include <sys/types.h>

/*
 * SINYAL MODULU - Genel Bakis
 * ---------------------------------------------------------------
 * Bu modul sunucunun (main) sinyallerini yonetir. En onemli gorev:
 * CTRL+C (SIGINT) geldiginde ONCE tum child process'leri duzgun
 * kapatmak, ardindan IPC kaynaklarini (shared memory + semaphore)
 * temizleyip cikmaktir.
 *
 * Desteklenen sinyaller:
 *   SIGINT  (Ctrl+C) -> Tum child'lari kapat, kaynaklari temizle, cik
 *   SIGTERM          -> SIGINT ile ayni (kademeli guvenli kapanma)
 *   SIGUSR1          -> Tum sinyalleri sifirla (reset)
 *   SIGUSR2          -> Anlik istatistikleri ekrana/log'a yaz
 *   SIGHUP           -> Konfigurasyonu yeniden yukle (log'lanir)
 *   SIGCHLD          -> Beklenmeden olen child'lari toplar (zombie engelle)
 */

/* Sinyal handler'larini kurar. main()'in EN BASINDA, fork'lardan
 * once cagrilmalidir. (Mevcut main.c zaten bunu cagiriyor.) */
void signal_function(void);

/* Fork edilen her child'in PID'sini modul'e bildirir. main() icindeki
 * fork dongusunde, PARENT tarafinda (pid > 0) her basarili fork'tan
 * sonra cagrilmalidir. Handler kapatirken bu listeyi kullanir. */
void sinyal_child_ekle(pid_t pid);

/* Temizlenecek IPC kaynaklarini modul'e bildirir. main() shared memory
 * ve semaphore'u olusturduktan sonra cagirir. Cagrilmazsa kaynak
 * temizligi guvenli sekilde atlanir (id'ler -1 kalir). */
void sinyal_kaynak_kaydet(int shmid, int semid);

/* Tum sinyalleri reset et - child'lara broadcast yap (SIGUSR1 ile)
 * Bu fonksiyon asenkron klavye girisi tarafından çağrılabilir.
 * Child'lara bir reset sinyali gonder, böylece state'leri sifirlanabilir. */
void sinyal_reset_tum_sinyaller(void);
void sinyal_config_baslat(void);

/* Asenkron klavye girişi thread'ini başlat.
 * Komutlar: 'r' = reset, 's' = istatistik, 'q' = kapanma.
 * Thread detached olarak çalışır (kendi kaynağını temizler). */
void sinyal_klavye_baslat(void);

/* Normal kod akisindan (handler disindan) zaman damgali log yazmak
 * icin yardimci fonksiyon. Istege bagli kullanilir. */
void sinyal_log(const char *mesaj);

#endif
