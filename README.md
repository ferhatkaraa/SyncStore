# IPC Sistem Projesi 🚀

## Proje Açıklaması
Bu proje, **3 child process**'in paylaşımlı bellek (IPC) üzerinde **güvenli** veri okuma/yazma işlemlerini gerçekleştirmesini sağlar. Semaphore ile senkronizasyon yapılır, graceful shutdown uygulanır ve asenkron klavye komutları desteklenir.

## ✨ Temel Özellikler
- ✅ Fork ile 3 child process oluşturma
- ✅ Paylaşımlı bellek (Shared Memory) ile IPC
- ✅ Semaphore ile race condition korunması
- ✅ 60 saniye çalışma süresi (child'lar farklı aralıklarla)
- ✅ **Graceful shutdown** (Ctrl+C - tüm child'ları bekle, kaynakları temizle)
- ✅ **Reset sinyali** (SIGUSR1 - tüm sinyalleri sıfırla)
- ✅ **Asenkron keyboard** (r/s/q komutları - program çalışırken)
- ✅ 5 farklı sinyal handler

---

## Dosya Yapısı

| Dosya | Açıklama |
|-------|----------|
| **main.c** | Fork, SHM oluşturma, Semaphore setup, cleanup |
| **child1.c** | Child1: 2 saniye aralık, yazma/okuma |
| **child2.c** | Child2: 3 saniye aralık, okuma/yazma |
| **child3.c** | Child3: 4 saniye aralık, yazma/okuma |
| **sinyal.c** | Sinyal handler'ları, async keyboard thread |
| **sinyal.h** | Sinyal API prototipleri |
| **storage.c** | Semaphore kilitleme, read/write fonksiyonları |
| **storage.h** | SharedData struct, KeyValue veri yapıları |
| **Makefile** | GCC derleme |

---

## Sistem Mimarisi

```
┌─────────────────────────────────────────────┐
│         MAIN PROCESS (Parent)               │
│  - Fork yapı (3 child process)              │
│  - Shared Memory oluşturma (shmget)         │
│  - Semaphore oluşturma (semget)             │
│  - Async Keyboard Thread                    │
│  - Waitpid() ile child'ları bekleme         │
│  - Kaynakları temizleme (cleanup)           │
└─────────────────────────────────────────────┘
          ↓         ↓         ↓
    ┌──────┴─────┬──────────┬──────┴──────┐
    ↓            ↓          ↓             ↓
┌────────┐  ┌────────┐  ┌────────┐  ┌─────────┐
│CHILD1  │  │CHILD2  │  │CHILD3  │  │KEYBOARD │
│(2s)    │  │(3s)    │  │(4s)    │  │THREAD   │
│        │  │        │  │        │  │         │
│Yazma   │  │Okuma   │  │Yazma   │  │r=Reset  │
│Okuma   │  │Yazma   │  │Okuma   │  │s=Stat   │
└────────┘  └────────┘  └────────┘  │q=Quit   │
    │           │          │         │         │
    └───────────┴──────────┴─────────┼─────────┘
                │          
         ┌──────▼──────┐
         │ STORAGE     │
         │ (IPC Layer) │
         │             │
         │ Sem: Lock   │
         │ SHM: Data   │
         └─────────────┘
```

### **IPC Bileşenleri:**

1. **Shared Memory** (shmid)
   - Tür: `IPC_PRIVATE`
   - Boyut: `sizeof(SharedData)` = 100 × KeyValue + int
   - İçerik: Anahtar-değer çiftleri (maks 100)

2. **Semaphore** (semid)
   - Tür: `IPC_PRIVATE`, 1 semaphore
   - İlk değer: 1 (açık/free)
   - Mekanizm: Mutex (kilitleme/açma)

### **Sinyal Handler'ları:**

| Sinyal | Kod | Handler | Fonksiyon |
|--------|-----|---------|-----------|
| SIGINT | 2 | `kapatma_handler()` | ✅ Graceful shutdown |
| SIGTERM | 15 | `kapatma_handler()` | ✅ Kademeli kapanma |
| SIGUSR1 | 10 | `reset_handler()` | ✅ Tüm child'lara reset |
| SIGUSR2 | 12 | `istatistik_handler()` | ✅ PID listesi göster |
| SIGHUP | 1 | `konfig_handler()` | ✅ Config reload (hazır) |

---

## Çalışma Akışı ve Zamanlamalar

### **Child Process'lerin 60 Saniyesi:**

| Child | Aralık | 0-30s | 30-60s | PID Kullanımı |
|-------|--------|-------|--------|---------------|
| **Child1** | 2s | Yazma | Okuma | Değer = PID + count |
| **Child2** | 3s | Okuma | Yazma | Değer = PID + count |
| **Child3** | 4s | Yazma | Okuma | Değer = PID + count |

### **Main Process Akışı:**

```
1. signal_function()          → Tüm handler'ları kur
2. sinyal_klavye_baslat()     → Async keyboard thread başlat
3. shmget()                   → Shared memory oluştur
4. shmat()                    → Process'e bağla
5. semget()                   → Semaphore oluştur
6. semctl(SETVAL, 1)          → Semaphore = 1 (açık)
7. sinyal_kaynak_kaydet()     → Kaynakları signal module'e kaydet
8. for (i=1; i<=3; i++) fork() → 3 child oluştur
9. waitpid() x 3              → Tüm child'lar bitişini bekle
10. shmdt()                   → Shared memory'den ayrıl
11. shmctl(IPC_RMID)          → Shared memory sil
12. semctl(IPC_RMID)          → Semaphore sil
```

---

## Güvenlik Mekanizması

### **Race Condition Korunması:**

```c
// Storage'da okuma/yazma:
kilitle(semid);              // ⬜ Lock al
// ... kritik bölge ...
// storage_write() veya storage_read()
kilidi_ac(semid);            // 🟩 Lock bırak
```

- **Semaphore kilitleme**: Aynı anda sadece 1 process'in veriye erişmesini sağlar
- **Atomic operasyonlar**: `kilitle()` ve `kilidi_ac()` atomic (bölünemez)
- **Veri tutarlılığı**: Tüm accesses senkronize

---

## Asenkron Keyboard Komutları

Program çalışırken bu tuşları basabilirsiniz:

```
┌─────────────────────────────────────────┐
│ r / R  → Reset sinyali gönder           │
│          (tüm child'lara SIGUSR1)       │
│                                         │
│ s / S  → İstatistik göster              │
│          (aktif child sayısı, PID list) │
│                                         │
│ q / Q  → Programı kapat (gracefully)    │
│          (tüm child'ları bekle)         │
│                                         │
│ ENTER  → Yardım mesajı                  │
└─────────────────────────────────────────┘
```

**Örnek:**
```
Program çalışıyor...
r
[KEYBOARD] Reset sinyali gonderildi.
[SERVER] SIGUSR1 (RESET) -> tum child'lara reset sinyali gonderiliyor...
```

---

## 🛠️ Derleme ve Çalıştırma

### **1️⃣ Linux / WSL / MSYS2 (Önerilen)**

```bash
# Makefile ile (en kolay)
make
make run

# Veya direkt:
gcc -Wall -pthread -std=c99 -o program \
    main.c sinyal.c storage.c child1.c child2.c child3.c -lpthread

./program
```

### **2️⃣ Windows MSYS2 Kurulumu**

```powershell
# 1. MSYS2 indir: https://www.msys2.org/
# 2. MSYS2 Terminal'i aç (MSYS2 MinGW x64):

pacman -S base-devel mingw-w64-x86_64-gcc mingw-w64-x86_64-make

# 3. Proje klasörüne git:
cd "/c/Users/FERHAT KARA/OneDrive/Masaüstü/sistem proje"

# 4. Derle:
make
./program
```

### **3️⃣ WSL2 Kurulumu**

```bash
# PowerShell (Admin):
wsl --install -d Ubuntu

# Sonra WSL Terminal'de:
sudo apt update && sudo apt install build-essential

cd /mnt/c/Users/FERHAT\ KARA/OneDrive/Masaüstü/sistem\ proje
gcc -Wall -pthread -std=c99 -o program \
    main.c sinyal.c storage.c child1.c child2.c child3.c -lpthread

./program
```

---

## 📊 Örnek Çalıştırma

```
$ ./program

[MAIN] Sinyal yonetimi kuruldu.
[MAIN] Asenkron klavye dinlemesi baslatildi.
[MAIN] Paylaşimlı bellek oluşturuldu (shmid=0, Size=404 bytes).
[MAIN] Semaphore oluşturuldu (semid=0) - initial value: 1
[MAIN] Child 1 fork edildi (PID=1234)
[MAIN] Child 2 fork edildi (PID=1235)
[MAIN] Child 3 fork edildi (PID=1236)

Child1 (PID 1234): Storage gorevini basliyorum...
Child1 (PID 1234): 60 saniyelik gorev basladi. Periyot: 2 saniye.
Child1 (PID 1234): Yazdim: ortak_depo = 1234 (slot: 0)
Child3 (PID 1236): Yazdim: ortak_depo = 1236 (slot: 0)

[Program çalışıyor - r tuşu ile reset, s tuşu ile stat, q tuşu ile çık]

r
[KEYBOARD] Reset sinyali gonderildi.
[SERVER] SIGUSR1 (RESET) -> tum child'lara reset sinyali gonderiliyor...

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

Program Bitti. Tum kaynaklar temizlendi.
```

---

## 🐛 Sorun Giderme

| Problem | Çözüm |
|---------|-------|
| `"shmget: Permission denied"` | `ipcrm -a` ile eski kaynakları temizle |
| `"pthread.h not found"` | WSL2 veya MSYS2 kullan |
| `"child process doesn't sync"` | Storage log'larını kontrol et, SHM ve SEM kurulu mu? |
| Program çalışırken input almıyor | Keyboard thread başlatıldı mı? (kontrol et: log'ta görünür) |

---

## 📚 Ek Kaynaklar

- [CODE_REVIEW.md](CODE_REVIEW.md) - Detaylı teknik inceleme
- [COMPILE_GUIDE.md](COMPILE_GUIDE.md) - Platform-spesifik derleme
- [Makefile](Makefile) - Build konfigürasyonu

---

## 👥 Proje Ekibi Notları

- ✅ Child1: Yazma dominantlı (ilk 30s)
- ✅ Child2: Okuma dominantlı (ilk 30s)
- ✅ Child3: Yazma dominantlı (ilk 30s)
- ✅ Farklı aralıklar: Race condition tetiklemek için optimal
- ✅ Semaphore: FIFO sırayla access sağlar
- ✅ Graceful shutdown: Zombie process'ler yok

---

## 📝 Lisans

Bu proje eğitim amaçlı bir IPC ve process yönetimi örneğidir.

**Tüm gereksinimler ✅ başarıyla uygulanmıştır!**
