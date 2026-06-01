# Kod Inceleme Raporu

Bu rapor, mevcut kodun son durumuna gore hazirlanmistir.

## Genel Durum

Proje, parent-child process mimarisi ile System V shared memory ve semaphore kullanan bir anahtar-deger deposu ornegidir. Parent IPC kaynaklarini olusturur, 3 child fork eder, child'lar shared memory'ye baglanir ve `storage_read()` / `storage_write()` fonksiyonlari uzerinden ortak depoya erisir.

## Derleme Uyarisi Duzeltmesi

Gorulen uyarilar:

```text
implicit declaration of function 'child1_storage_task'
conflicting types for 'child1_storage_task'
```

Sebep:

`child1.c`, `child2.c`, `child3.c` icinde `child*_storage_task()` fonksiyonlari cagrildigi noktada derleyici henuz prototipi gormuyordu.

Duzeltme:

- `child1.c` icine `#include "child1.h"` eklendi.
- `child2.c` icine `#include "child2.h"` eklendi.
- `child3.c` icine `#include "child3.h"` eklendi.

Bu header dosyalari ilgili `child*_storage_task()` prototiplerini icerir.

## IPC Tasarimi

| Bilesen | Kod | Degerlendirme |
| --- | --- | --- |
| Shared memory | `shmget`, `shmat`, `shmdt`, `shmctl` | Parent olusturuyor, child'lar `shmid` ile baglaniyor |
| Semaphore | `semget`, `semctl`, `semop` | Tek semaphore mutex gibi kullaniliyor |
| Depo | `SharedData` | `db[100]`, `count`, interval ve config versiyon alanlari var |
| Child lifecycle | `fork`, `wait` | Parent child'lari olusturup bekliyor |
| Cleanup | `IPC_RMID` | Normal cikista kaynaklar temizleniyor |

## Storage Incelemesi

`storage_write()`:

- Gecersiz pointer kontrolu yapar.
- Semaphore ile kilit alir.
- Key varsa mevcut slotu gunceller.
- Key yoksa kapasite uygunsa yeni slot acar.
- `value` ve `last_update` alanlarini yazar.
- Kilidi birakir.

`storage_read()`:

- Gecersiz pointer kontrolu yapar.
- Semaphore ile kilit alir.
- Key arar.
- Bulursa degeri ve timestamp bilgisini yazdirir.
- Bulamazsa `-1` doner.
- Kilidi birakir.

Bu yapi, `count` ve `db` alanlari icin race condition riskini azaltir.

## Child Davranislari

| Child | Periyot | 0-30 saniye | 30-60 saniye |
| --- | ---: | --- | --- |
| Child 1 | `interval1`, varsayilan 2 | Yazma | Okuma |
| Child 2 | `interval2`, varsayilan 3 | Okuma | Yazma |
| Child 3 | `interval3`, varsayilan 4 | Yazma | Okuma |

Her child ayni key'i kullanir:

```c
const char key[] = "ortak_depo";
```

Bu, semaphore'un ortak kaynak uzerindeki etkisini gostermek icin uygundur.

## Sinyal Incelemesi

Desteklenen sinyaller:

| Sinyal | Davranis |
| --- | --- |
| `SIGINT` | Graceful shutdown |
| `SIGTERM` | Graceful shutdown |
| `SIGUSR1` | Reset broadcast |
| `SIGUSR2` | Istatistik |
| `SIGHUP` | Config reload istegi |
| `SIGCHLD` | Biten child'lari toplama |

`kapatma_handler()` parent ve child ayrimini `getpid() != g_ana_pid` kontroluyle yapar. Bu onemlidir; child process IPC kaynaklarini silmeye calismaz.

## Config Reload

`SIGHUP` handler'i dogrudan dosya okumaz. Bunun yerine `g_reload_config_request` flag'ini set eder. Config thread bu istegi gorunce `syncstore.conf` dosyasini okur, shared memory'ye baglanir, semaphore ile kilit alir ve interval alanlarini gunceller.

Bu tasarim, sinyal handler icinde agir ve guvensiz is yapmamak acisindan dogru yonde bir tercihtir.

## Dikkat Edilebilecek Noktalar

- `storage_write()` icinde `kilitle()` hata alsa bile fonksiyon devam ediyor. Daha guclu hata yonetimi icin `kilitle()` basari/hata dondurebilir.
- `sigchld_handler()` child'lari erken toplarsa `main.c` icindeki `wait()` bazen `ECHILD` gorebilir. Kod bu durumu kontrol ediyor.
- Tum child'lar tek key kullaniyor. Coklu anahtar senaryosu istenirse child bazli key veya random key uretimi eklenebilir.
- `printf()` child ve normal thread akisi icinde kullaniliyor; handler icinde ise `write()` tabanli yardimcilar tercih edilmis.

## Sonuc

Mevcut kod, IPC anahtar-deger deposu, semaphore ile kritik bolge korumasi, graceful shutdown, reset/istatistik sinyalleri, SIGCHLD toplama ve SIGHUP config reload ozelliklerini birlikte gosteren calisir bir ornektir. Bildirilen implicit declaration uyarilari header include duzeltmesiyle giderilmistir.
