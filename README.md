# IPC Semaphore Key-Value Store Projesi

Bu proje, parent process tarafindan olusturulan 3 child process'in System V IPC uzerinden ortak bir anahtar-deger deposuna erismesini gosterir. Ortak veri alani shared memory icindedir; okuma ve yazma islemleri tek bir System V semaphore ile korunur. Boylece ayni anda sadece bir process kritik bolgeye girer ve `SharedData` yapisinin tutarliligi korunur.

## Temel Ozellikler

- Parent process 3 child process olusturur.
- IPC icin System V shared memory kullanilir.
- Senkronizasyon icin System V semaphore kullanilir.
- Depo modeli `KeyValue db[100]` + `count` alanindan olusur.
- Child process'ler 60 saniye calisir.
- Child'lar farkli periyotlarla okuma/yazma yapar.
- `SIGINT` ve `SIGTERM` ile graceful shutdown uygulanir.
- `SIGUSR1` ile reset sinyali child'lara broadcast edilir.
- `SIGUSR2` ile aktif child PID listesi yazdirilir.
- `SIGHUP` ile `syncstore.conf` dosyasindan interval reload istegi islenir.
- `SIGCHLD` handler'i beklenmeden biten child'lari toplar.
- Program calisirken `r`, `s`, `q` klavye komutlari desteklenir.

## Dosya Yapisi

| Dosya | Gorev |
| --- | --- |
| `main.c` | Program girisi, shared memory/semaphore kurulumu, fork, wait ve cleanup |
| `storage.h` | `KeyValue`, `SharedData` ve storage API tanimlari |
| `storage.c` | Semaphore lock/unlock, key arama, okuma ve yazma islemleri |
| `child1.c/.h` | Child 1 gorevi, 2 saniye varsayilan periyot |
| `child2.c/.h` | Child 2 gorevi, 3 saniye varsayilan periyot |
| `child3.c/.h` | Child 3 gorevi, 4 saniye varsayilan periyot |
| `sinyal.c/.h` | Sinyal handler'lari, keyboard thread, config reload thread, loglama |
| `syncstore.conf` | Calisma sirasinda reload edilebilen interval ayarlari |
| `Makefile` | GCC ile derleme hedefleri |
| `COMPILE_GUIDE.md` | Derleme ve calistirma notlari |
| `CODE_REVIEW.md` | Teknik kod inceleme ozeti |
| `tamamlama.md` | Son tamamlanma durumu |

## Mimari

```text
Parent process
  |
  |-- signal_function()
  |-- shmget() / shmat()
  |-- semget() / semctl(SETVAL=1)
  |-- fork()
  |     |-- child1 -> shmat(shmid), storage_write/read()
  |     |-- child2 -> shmat(shmid), storage_read/write()
  |     |-- child3 -> shmat(shmid), storage_write/read()
  |
  |-- keyboard thread: r/s/q
  |-- config thread: SIGHUP sonrasi syncstore.conf reload
  |-- wait()
  |-- shmdt(), shmctl(IPC_RMID), semctl(IPC_RMID)
```

Parent, IPC kaynaklarini olusturur ve child'lara `shmid` ile `semid` bilgisini verir. Child'lar kendi adres alanlarinda `shmat()` cagirarak ayni shared memory segmentine baglanir. `semid` ise tum child'lar tarafindan ayni semaphore setine erismek icin kullanilir.

## Shared Memory Veri Modeli

`storage.h` icindeki ana veri yapisi:

```c
typedef struct {
    char key[32];
    int value;
    time_t last_update;
} KeyValue;

typedef struct {
    KeyValue db[100];
    int count;
    int interval1;
    int interval2;
    int interval3;
    int config_version;
} SharedData;
```

Alanlar:

- `db[100]`: En fazla 100 anahtar-deger kaydi tutar.
- `count`: Depoda aktif kac kayit oldugunu belirtir.
- `interval1`, `interval2`, `interval3`: Child periyotlari. Varsayilan degerler `2`, `3`, `4`.
- `config_version`: Config reload basarili olunca artirilir.
- `last_update`: Bir key'in en son ne zaman yazildigini saklar.

Su an child'lar ayni anahtari kullanir:

```c
const char key[] = "ortak_depo";
```

Bu tercih, ayni kayit uzerinde eszamanli erisim baskisi olusturarak semaphore korumasini gostermek icindir.

## Okuma, Yazma ve Veri Kaliciligi

Child process'ler okuma ve yazma yaparken birbirlerini bekler. Bunun nedeni `storage_read()` ve `storage_write()` fonksiyonlarinin ortak shared memory alanina erismeden once semaphore kilidi almasidir.

```c
kilitle(semid);
/* kritik bolge: shared memory okuma veya yazma */
kilidi_ac(semid);
```

Bir child kritik bolgedeyken baska bir child ayni anda depoya giremez. Diger child process `semop()` cagrisi uzerinde bekler. Bu nedenle okuma ve yazma islemleri sirali hale gelir ve `db` ile `count` alanlari ayni anda birden fazla process tarafindan degistirilmez.

Yazilan degerler program calistigi sure boyunca shared memory icinde kalir. Parent process program sonunda `shmctl(shmid, IPC_RMID, NULL)` cagirarak shared memory segmentini sildigi icin veriler program kapandiktan sonra kalici olarak saklanmaz. Bu proje dosyaya veya veritabanina kalici kayit yapmaz; veriler yalnizca calisma suresince IPC belleginde tutulur.

Bir child okuma yaptiginda tek bir sayi okumasinin nedeni mevcut senaryoda tum child'larin ayni key'i kullanmasidir:

```c
const char key[] = "ortak_depo";
```

Depo teknik olarak 100 farkli key-value kaydi tutabilir. Ancak mevcut child kodlari sadece `"ortak_depo"` anahtarini kullandigi icin pratikte tek kayit olusur. Yeni yazma islemi ayni key'i bulur ve eski degeri gunceller. Bu yuzden okuma islemi, o anda `"ortak_depo"` anahtarinda bulunan son yazilmis `int` degerini okur.

## Semaphore ile Kritik Bolge Koruması

`storage.c` icinde iki temel yardimci vardir:

```c
void kilitle(int semid) {
    struct sembuf operasyon = {0, -1, 0};
    semop(semid, &operasyon, 1);
}

void kilidi_ac(int semid) {
    struct sembuf operasyon = {0, 1, 0};
    semop(semid, &operasyon, 1);
}
```

Mantik:

- Semaphore baslangic degeri `1` oldugu icin kilit bostur.
- `kilitle()` semaphore degerini `-1` ile dusurur.
- Deger `0` iken baska process `kilitle()` cagirirsa kernel onu bekletir.
- `kilidi_ac()` semaphore degerini `+1` yapar ve bekleyen process varsa devam eder.

`storage_write()` ve `storage_read()` icinde key arama, `count` guncelleme, `db[index]` yazma ve okuma islemleri bu lock altinda yapilir.

## Child Process Davranislari

| Child | Varsayilan periyot | Ilk 30 saniye | Son 30 saniye | Deger |
| --- | ---: | --- | --- | --- |
| Child 1 | 2 saniye | Yazma | Okuma | `PID + operation_count` |
| Child 2 | 3 saniye | Okuma | Yazma | `PID + operation_count` |
| Child 3 | 4 saniye | Yazma | Okuma | `PID + operation_count` |

Her child 60 saniyelik dongu calistirir:

```c
while (time(NULL) - start_time < 60) {
    int elapsed = (int)(time(NULL) - start_time);
    interval = data->intervalN;

    if (elapsed < 30) {
        storage_write(...);
    } else {
        storage_read(...);
    }

    sleep(interval);
}
```

Child'lar interval degerlerini her dongude shared memory'den tekrar okur. Bu sayede `SIGHUP` ile config reload yapildiktan sonra yeni periyotlar calisma sirasinda etkili olur.

## Sinyal Yonetimi

| Sinyal | Handler | Davranis |
| --- | --- | --- |
| `SIGINT` | `kapatma_handler()` | Child'lara `SIGTERM` gonderir, bekler, IPC kaynaklarini temizler |
| `SIGTERM` | `kapatma_handler()` | `SIGINT` ile ayni graceful shutdown akisini izler |
| `SIGUSR1` | `reset_handler()` | Parent tum child'lara reset sinyali gonderir |
| `SIGUSR2` | `istatistik_handler()` | Kayitli child sayisini ve PID listesini yazar |
| `SIGHUP` | `konfig_handler()` | Config reload talebi olusturur |
| `SIGCHLD` | `sigchld_handler()` | Bitmis child process'leri `waitpid(..., WNOHANG)` ile toplar |

`signal_function()` fork'lardan once cagrildigi icin child'lar da handler'lari miras alir. Handler icinde `getpid()` ana PID ile karsilastirilir. Child bir kapatma sinyali alirsa sadece kendini sonlandirir; IPC kaynaklarini silmeye calismaz.

## Asenkron Klavye Komutlari

Parent process fork islemlerinden sonra keyboard thread baslatir.

| Tus | Etki |
| --- | --- |
| `r` veya `R` | Parent'a `SIGUSR1` gonderir, child'lara reset broadcast edilir |
| `s` veya `S` | Parent'a `SIGUSR2` gonderir, istatistik yazdirilir |
| `q` veya `Q` | Parent'a `SIGINT` gonderir, graceful shutdown baslar |
| `Enter` | Komut yardimini yazdirir |

## Config Reload

`syncstore.conf` dosyasi:

```conf
interval1=2
interval2=3
interval3=4
```

Program calisirken baska bir terminalden:

```bash
kill -HUP <parent_pid>
```

gonderildiginde `SIGHUP` handler'i reload istegi isaretler. Config thread bu istegi gorur, dosyayi okur, shared memory'ye baglanir, semaphore ile kilit alir ve interval alanlarini gunceller. Basarili degisiklikte `config_version` artar.

## Derleme

WSL/Linux icinde:

```bash
make
```

veya dogrudan:

```bash
gcc main.c child1.c child2.c child3.c storage.c sinyal.c -o program -lpthread
```

Daha fazla uyari yakalamak icin:

```bash
gcc -Wall -Wextra -std=c99 -pthread main.c child1.c child2.c child3.c storage.c sinyal.c -o program -lpthread
```

## Calistirma

```bash
./program
```

Beklenen akis:

```text
[MAIN] Sinyal yonetimi kuruldu.
[MAIN] Paylasimli bellek olusturuldu (...)
[MAIN] Semaphore olusturuldu (...)
[MAIN] Child 1 fork edildi (...)
[MAIN] Child 2 fork edildi (...)
[MAIN] Child 3 fork edildi (...)
[MAIN] Asenkron klavye dinlemesi baslatildi.
```

Program calisirken:

```text
r  -> reset
s  -> istatistik
q  -> temiz cikis
```

## Temizleme

Normal cikista parent:

1. Child process'lerin bitmesini bekler.
2. Shared memory'den `shmdt()` ile ayrilir.
3. Shared memory segmentini `shmctl(..., IPC_RMID, ...)` ile siler.
4. Semaphore'u `semctl(..., IPC_RMID)` ile siler.

Beklenmeyen durumda IPC kaynaklari kalirsa Linux/WSL uzerinde kontrol:

```bash
ipcs
```

Gerekirse temizleme:

```bash
ipcrm -a
```

## Son Durum

Kodun son halinde `child1.c`, `child2.c`, `child3.c` kendi header dosyalarini include eder. Boylece `child*_storage_task` fonksiyonlari icin implicit declaration uyarilari giderilmistir.

Proje su an parent-child IPC, semaphore tabanli kritik bolge korumasi, asenkron klavye kontrolu, graceful shutdown ve config reload ozelliklerini birlikte gosteren calisir bir System V IPC ornegidir.
