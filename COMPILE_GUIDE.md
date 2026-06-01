# Derleme ve Calistirma Kilavuzu

Bu proje System V IPC kullandigi icin Linux veya WSL ortaminda derlenmelidir. Windows tarafinda dogrudan klasik `cmd` veya PowerShell ile derlemek uygun degildir; WSL ya da MSYS2 kullanilmalidir.

## WSL/Linux ile Derleme

Proje klasorunde:

```bash
make
```

veya:

```bash
gcc main.c child1.c child2.c child3.c storage.c sinyal.c -o program -lpthread
```

Daha siki kontrol icin onerilen komut:

```bash
gcc -Wall -Wextra -std=c99 -pthread main.c child1.c child2.c child3.c storage.c sinyal.c -o program -lpthread
```

## Calistirma

```bash
./program
```

Program calisirken:

```text
r / R  -> reset sinyali gonder
s / S  -> istatistik yazdir
q / Q  -> graceful shutdown
Enter  -> klavye komutlarini goster
```

## Sinyal Komutlari

Baska terminalden parent PID biliniyorsa:

```bash
kill -USR1 <parent_pid>
kill -USR2 <parent_pid>
kill -HUP <parent_pid>
kill -TERM <parent_pid>
```

Anlamlari:

| Komut | Etki |
| --- | --- |
| `SIGUSR1` | Child'lara reset broadcast eder |
| `SIGUSR2` | Child PID listesini yazdirir |
| `SIGHUP` | `syncstore.conf` dosyasindan interval reload ister |
| `SIGTERM` | Temiz kapanma baslatir |

## Config Reload

`syncstore.conf` ornegi:

```conf
interval1=2
interval2=3
interval3=4
```

Dosyayi degistirdikten sonra:

```bash
kill -HUP <parent_pid>
```

Child'lar interval degerlerini her dongude shared memory'den okudugu icin degisiklik calisma sirasinda uygulanir.

## Sik Karsilasilan Durumlar

| Durum | Cozum |
| --- | --- |
| `pthread.h` bulunamiyor | WSL/Linux ya da MSYS2 kullanin |
| `shmget`/`semget` hata veriyor | Once `ipcs` ile kalan IPC kaynaklarini kontrol edin |
| Eski IPC kaynaklari kalmis | Gerekirse `ipcrm -a` ile temizleyin |
| Program input almiyor gibi gorunuyor | `r`, `s`, `q` tuslarindan sonra terminal davranisina gore `Enter` gerekebilir |

## Son Derleme Uyarisi Duzeltmesi

Eski durumda `child1.c`, `child2.c`, `child3.c` dosyalari kendi header dosyalarini include etmedigi icin su uyari gorulebiliyordu:

```text
warning: implicit declaration of function 'child1_storage_task'
warning: conflicting types for 'child1_storage_task'
```

Bu durum giderildi. Her child kaynak dosyasi artik kendi prototiplerini iceren header'i include eder:

```c
#include "child1.h"
#include "child2.h"
#include "child3.h"
```
