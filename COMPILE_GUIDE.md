# IPC Proje - Derleme ve Çalıştırma Kılavuzu

## **LINUX/WSL'de Derleme (Önerilen)**

```bash
# Makefile ile derle
make

# Programı çalıştır
make run

# Veya direkt
./program
```

## **Windows'ta MSYS2/MinGW ile Derleme**

1. MSYS2 kur: https://www.msys2.org/
2. Terminal'i aç ve şu komutları çalıştır:
```bash
pacman -S base-devel mingw-w64-x86_64-gcc
cd "/c/Users/FERHAT KARA/OneDrive/Masaüstü/sistem proje"
gcc -Wall -Wextra -std=c99 -pthread -o program \
    main.c sinyal.c storage.c child1.c child2.c child3.c -lpthread
./program
```

## **Programın Özellikleri**

### **Ana Akış:**
1. **3 child process** fork edilir
2. Her child **60 saniye** boyunca çalışır
3. Child'lar **paylaşımlı bellek** üzerinden IPC yapır
4. **Semaphore** ile race condition korunur

### **Child Görevleri:**
- **Child1** (PID'li): 2 saniyede bir yazma/okuma (ilk 30s yazma, son 30s okuma)
- **Child2** (PID'li): 3 saniyede bir okuma/yazma (ilk 30s okuma, son 30s yazma)
- **Child3** (PID'li): 4 saniyede bir yazma/okuma (ilk 30s yazma, son 30s okuma)

### **Sinyal İşleme:**

#### **Graceful Shutdown (SIGINT/SIGTERM)**
- Tüm child'lar SIGTERM alır
- Main waitpid() ile tüm child'ları bekleri
- Shared memory ve semaphore temizlenir
- Zombie process'ler bırakılmaz

#### **Reset Sinyali (SIGUSR1)**
- Tüm child'lara reset sinyali gönderilir
- Child'lar state'lerini sıfırlayabilir
- Asenkron `r` tuşu veya `kill -USR1 <pid>` ile tetiklenir

#### **İstatistik (SIGUSR2)**
- Aktif child sayısı ve PID'ler gösterilir
- `s` tuşu veya `kill -USR2 <pid>` ile tetiklenir

#### **Config Reload (SIGHUP)**
- Gelecek sürümlerde konfigürasyon yeniden yükleme
- `kill -HUP <pid>` ile tetiklenir

### **Asenkron Klavye Komutları (Program Çalışırken)**
Program çalışırken şu tuşları basabilirsiniz:
```
r / R  → Reset sinyali gönder (tüm child'lara SIGUSR1)
s / S  → İstatistik göster (PID listesi)
q / Q  → Programı kapat (gracefully - SIGINT gönder)
ENTER  → Komut yardımı göster
```

## **Örnek Çalışma Senaryosu**

```bash
$ ./program

[MAIN] Sinyal yonetimi kuruldu.
[MAIN] Asenkron klavye dinlemesi baslatildi.
[MAIN] Paylaşimlı bellek oluşturuldu (shmid=0, Size=404 bytes).
[MAIN] Semaphore oluşturuldu (semid=0) - initial value: 1
[MAIN] Child 1 fork edildi (PID=1234)
[MAIN] Child 2 fork edildi (PID=1235)
[MAIN] Child 3 fork edildi (PID=1236)
[MAIN] Tum child'lar bitmesini bekliyorum...

Child1 (PID 1234): Storage gorevini basliyorum...
Child1 (PID 1234): 60 saniyelik gorev basladi. Periyot: 2 saniye.
Child1 (PID 1234): Yazdim: ortak_depo = 1234 (slot: 0)

[Program çalışıyor - r tuşu ile reset, q tuşu ile çık]

r
[KEYBOARD] Reset sinyali gonderildi.
[SERVER] SIGUSR1 (RESET) -> tum child'lara reset sinyali gonderiliyor...
[CHILD 1234] SIGUSR1 (RESET) sinyali alindi - state sifirlanacak.

q
[KEYBOARD] Kapanma sinyali gonderildi.
[SERVER] Sinyal alindi (no=2). ONCE tum child'lar kapatiliyor...
[SERVER] Child kapandi: PID 1234
[SERVER] Child kapandi: PID 1235
[SERVER] Child kapandi: PID 1236
[SERVER] Tum child'lar kapandi. IPC kaynaklari temizleniyor...
[SERVER] Shared memory silindi.
[SERVER] Semaphore silindi.
[SERVER] Temiz cikis. Hosca kal.
```

## **Sorun Giderme**

### **"shmget: Permission denied"**
```bash
# Eski IPC kaynakları temizle
ipcrm -a
```

### **"pthread.h not found"**
- WSL2 veya Linux kurulu değil
- MSYS2/MinGW kullanın (yukarıda anlatıldı)

### **"Child process doesn't sync"**
- Storage yazma/okuma log'ları kontrol edin
- Semaphore'un SETVAL 1 olduğundan emin olun

## **Dosya Yapısı**

```
main.c           - Program giriş noktası, fork ve IPC setup
sinyal.c/h       - Sinyal yönetimi, graceful shutdown, async keyboard
storage.c/h      - Paylaşımlı bellek ve semaphore operasyonları
child1/2/3.c/h   - Child process'lerin görevleri
Makefile         - Derleme için make komutu
```

## **Projeyi Kontrol Etme Adımları**

1. ✅ **Main**: fork, child başlatma, storage IPC
2. ✅ **Children**: 60 saniye çalışma, okuma/yazma işlemleri
3. ✅ **Storage**: Semaphore ile race condition korunması
4. ✅ **Sinyal**: SIGINT graceful shutdown
5. ✅ **Reset**: SIGUSR1 ile tüm sinyalleri reset
6. ✅ **Asenkron Klavye**: Program çalışırken r/s/q komutları

Tüm gereksinimler **başarıyla uygulanmıştır**! 🎉
