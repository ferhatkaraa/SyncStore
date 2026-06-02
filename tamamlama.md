# Proje Tamamlama Durumu

## Tamamlananlar

- [x] `main.c` icinde shared memory `shmget()` ile olusturuluyor.
- [x] `main.c` icinde shared memory `shmat()` ile parent adres alanina baglaniyor.
- [x] `main.c` icinde semaphore `semget()` ile olusturuluyor.
- [x] Semaphore `semctl(SETVAL, 1)` ile mutex gibi baslatiliyor.
- [x] Parent 3 child process fork ediyor.
- [x] Child process'lere `shmid` ve `semid` bilgileri aktariliyor.
- [x] Child process'ler kendi iclerinde `shmat()` ile shared memory'ye baglaniyor.
- [x] `storage.c` icinde `kilitle()` ve `kilidi_ac()` fonksiyonlari `semop()` kullaniyor.
- [x] `storage_write()` key ekleme/guncelleme ve timestamp yazma islemi yapiyor.
- [x] `storage_read()` key okuma ve bulunamama durumunu isliyor.
- [x] `storage_delete()` key silme islemini semaphore korumali olarak yapiyor.
- [x] `storage_list()` tum key-value kayitlarini slot ve timestamp bilgisiyle listeliyor.
- [x] SET, GET, DELETE ve LIST komutlari ekrana ve `syncstore.log` dosyasina loglaniyor.
- [x] `SharedData` icinde `KeyValue db[100]`, `count`, interval ve config version alanlari var.
- [x] Child 1 varsayilan 2 saniye periyotla calisiyor.
- [x] Child 2 varsayilan 3 saniye periyotla calisiyor.
- [x] Child 3 varsayilan 4 saniye periyotla calisiyor.
- [x] Child'lar 60 saniyelik calisma dongusune sahip.
- [x] Child'lar ilk 20 saniye SET, 20-40 saniye GET/LIST, son 20 saniye DELETE/LIST senaryosu calistiriyor.
- [x] Child'lar hem ortak `ortak_depo` key'i hem de kendilerine ait key'ler uzerinde eszamanli islem yapiyor.
- [x] En az 2 istemcinin ayni anda calismasi isteri 3 child client ile karsilaniyor.
- [x] Parent server, child client'lar bittikten sonra kapanmadan calismaya devam ediyor.
- [x] IPC kaynaklari yalnizca `q`/`Q`, `SIGINT` veya `SIGTERM` ile temizleniyor.
- [x] `SIGINT` ve `SIGTERM` graceful shutdown icin isleniyor.
- [x] Parent kapanirken child'lara `SIGTERM` gonderiyor ve bekliyor.
- [x] Program sonunda shared memory ve semaphore kaynaklari temizleniyor.
- [x] `SIGUSR1` reset broadcast icin kullaniliyor.
- [x] `SIGUSR2` istatistik yazdirmak icin kullaniliyor.
- [x] `SIGHUP` config reload istegi icin kullaniliyor.
- [x] `SIGCHLD` ile beklenmeden biten child'lar toplanabiliyor.
- [x] Keyboard thread `r`, `s`, `q` komutlarini destekliyor.
- [x] Config thread `syncstore.conf` uzerinden interval degerlerini reload edebiliyor.
- [x] `child1.c`, `child2.c`, `child3.c` kendi header dosyalarini include ediyor.
- [x] `child*_storage_task` implicit declaration uyarilari giderildi.
- [x] README ve diger Markdown dosyalari son duruma gore guncellendi.

## Mevcut Davranis Ozeti

1. Parent process sinyal sistemini hazirlar.
2. Shared memory ve semaphore kaynaklarini olusturur.
3. Shared memory icindeki varsayilan interval degerlerini ayarlar.
4. IPC kaynaklarini sinyal modulune kaydeder.
5. 3 child process fork eder.
6. Parent keyboard thread ve config thread baslatir.
7. Child'lar `ortak_depo` ve kendi key'leri uzerinde SET/GET/DELETE/LIST komutlarini calistirir.
8. Her storage erisimi semaphore ile korunur.
9. Parent child client'larin bitmesini bekler.
10. Parent server child'lar bittikten sonra da calismaya devam eder.
11. Cikis yalnizca `q`/`Q`, `SIGINT` veya `SIGTERM` ile yapilir.
12. Cikis sirasinda IPC kaynaklari temizlenir.

## Kalan veya Gelistirilebilir Noktalar

- [x] Odev isterlerinde belirtilen SET, GET, DELETE ve LIST komutlari tamamlandi.
- [x] En az iki istemcinin eszamanli calismasi isteri 3 child client ile gosterildi.
- [x] Server process'in child client'lar bittikten sonra da surekli calismasi saglandi.
- [x] Graceful shutdown, async-signal-safe handler mimarisi, keyboard thread ve config thread korunarak tamamlandi.

## Son Durum

Proje, System V shared memory ve semaphore kullanan eksiksiz bir IPC anahtar-deger deposu olarak calisir durumdadir. SET, GET, DELETE ve LIST komutlari semaphore korumasi altinda uygulanmistir. Uc child client ayni anda calisarak ortak ve kendilerine ait key'ler uzerinde eszamanli islem yapar. Parent server, child client'lar tamamlandiktan sonra kapanmaz; keyboard thread, config thread ve sinyal handler'lari aktif kalir. IPC kaynaklari yalnizca `q`/`Q`, `SIGINT` veya `SIGTERM` ile graceful shutdown akisi icinde temizlenir. Bu haliyle proje odev isterlerini basariyla karsilamaktadir.
