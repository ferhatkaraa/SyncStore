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
- [x] `SharedData` icinde `KeyValue db[100]`, `count`, interval ve config version alanlari var.
- [x] Child 1 varsayilan 2 saniye periyotla calisiyor.
- [x] Child 2 varsayilan 3 saniye periyotla calisiyor.
- [x] Child 3 varsayilan 4 saniye periyotla calisiyor.
- [x] Child'lar 60 saniyelik calisma dongusune sahip.
- [x] Child'lar ilk 30 saniye ve son 30 saniyede farkli okuma/yazma davranisi sergiliyor.
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
7. Child'lar `ortak_depo` key'i uzerinde okuma/yazma yapar.
8. Her storage erisimi semaphore ile korunur.
9. Parent child'larin bitmesini bekler.
10. Cikis sirasinda IPC kaynaklari temizlenir.

## Kalan veya Gelistirilebilir Noktalar

- [ ] Child'lar icin coklu key senaryosu eklenebilir.
- [ ] `kilitle()` ve `kilidi_ac()` fonksiyonlari hata durumunu boolean/int olarak dondurecek sekilde gelistirilebilir.
- [ ] Storage kapasitesi doldugunda daha ayrintili hata/log mekanizmasi eklenebilir.
- [ ] `syncstore.conf` icin yorum satiri ve bos satir parse davranisi daha acik hale getirilebilir.

## Son Durum

Proje, semaforla korunan IPC anahtar-deger deposu olarak calisir durumdadir. Son duzeltmeyle child kaynak dosyalarindaki eksik header include problemi giderilmis ve derleme sirasinda gorulen implicit declaration uyarilarinin kaynagi kapatilmistir.
