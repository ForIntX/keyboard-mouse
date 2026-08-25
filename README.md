# Keyboard Mouse

[English documentation](README.en.md)

Keyboard Mouse, Linux'ta fare imlecini klavyeden yönetmenizi sağlayan C++17
uygulamasıdır. USB, Bluetooth, 2.4 GHz kablosuz ve dizüstü bilgisayarın dahili
klavyelerini Linux giriş yeteneklerinden otomatik olarak tanır.

Program donanıma, klavye firmware'ine veya BIOS'a hiçbir şey yazmaz. Fiziksel
klavye olaylarını `evdev` ile okuyup ayrı sanal klavye ve fare aygıtlarını
`uinput` ile oluşturur.

## Özellikler

- Varsayılan exclusive modda fare komutlarını açık uygulamadan gizleme
- Oklarla hızlanan, çapraz hızı dengelenmiş fare hareketi
- `+` ile sol, `-` ile sağ, `0` ile orta tıklama
- Tuşu basılı tutarak sürükle-bırak
- Fn algılanamazsa Caps Lock tetikleyicisine otomatik geçiş
- Birden fazla klavye profili ve Bluetooth yeniden bağlanma desteği
- X11 ve Wayland desteği
- `Ctrl+C`, `Ctrl+Alt+Esc` ve `--stop` ile güvenli kapanma
- Kullanıcı oturumuna sınırlı udev izni; `input` grubuna üyelik gerektirmez

## Desteklenen sistemler

- Linux kernel 4.5 veya daha yeni
- udev, `evdev` ve `uinput` kullanan masaüstü Linux dağıtımları
- CMake 3.16 veya daha yeni
- C++17 destekleyen GCC veya Clang

Güncel Debian, Ubuntu, Linux Mint, Fedora, Arch/Manjaro ve openSUSE sürümleri
hedeflenmektedir. WSL, Android, ChromeOS, container ortamları ve udev kullanmayan
minimal dağıtımlar doğrudan desteklenmez.

## Kurulum

### 1. Gerekli paketleri yükleyin

Debian, Ubuntu veya Linux Mint:

```bash
sudo apt update
sudo apt install build-essential cmake udev
```

Fedora:

```bash
sudo dnf install gcc-c++ cmake systemd-udev
```

Arch Linux veya Manjaro:

```bash
sudo pacman -S base-devel cmake systemd
```

openSUSE:

```bash
sudo zypper install -t pattern devel_basis
sudo zypper install cmake systemd
```

### 2. Projeyi indirin

GitHub depo adresini kendi deponuzla değiştirin:

```bash
git clone https://github.com/ForIntX/keyboard-mouse.git
cd keyboard-mouse
```

Projeyi ZIP olarak indirdiyseniz arşivi açıp terminalde proje klasörüne girin.

### 3. Eski süreci durdurun

Daha önce çalıştırılmış bir sürüm varsa:

```bash
pkill -TERM keyboard-mouse 2>/dev/null || true
```

Bu komut çalışan süreç yoksa hata vermeden devam eder.

### 4. Kurulum betiğini çalıştırın

```bash
chmod +x install.sh uninstall.sh
./install.sh
```

`chmod +x`, betiklere çalıştırma izni verir. `./install.sh` şu işlemleri yapar:

1. CMake ile Release derlemesi oluşturur.
2. Otomatik testleri çalıştırır; test başarısızsa kurulumu durdurur.
3. Güncel ikiliyi `/usr/local/bin/keyboard-mouse` konumuna kurar.
4. Eski `99-keyboard-mouse.rules` dosyasını temizler.
5. `70-keyboard-mouse.rules` udev kuralını kurar.
6. `uinput` kernel modülünü yüklemeyi dener.
7. udev kurallarını yenileyip giriş aygıtlarını tekrar işler.
8. Kurulan ikilinin derlenen dosyayla aynı olduğunu doğrular.

Sistem dosyalarını değiştiren adımlarda sudo parolanız istenir. Udev kuralının
`70-` ile başlaması önemlidir: systemd'nin seat ve uaccess kurallarından önce
çalışarak aktif masaüstü kullanıcısına dahili ve harici klavye erişimi sağlar.

### 5. Oturum izinlerini yenileyin

Kurulumdan sonra klavyeler `izin gerekli` görünüyorsa masaüstü oturumunu kapatıp
yeniden açın. Genellikle bilgisayarı yeniden başlatmak gerekmez.

### 6. Kurulumu doğrulayın

```bash
keyboard-mouse --version
keyboard-mouse --status
```

Beklenen sürüm çıktısı:

```text
keyboard-mouse 1.5.0 (+ sol tik, - sag tik, 0 orta tik)
```

`--status`; ayar dosyasını, seçilmiş klavyeleri, bağlantı durumunu, tıklama
kodlarını ve `/dev/uinput` erişimini gösterir.

### 7. Klavyeleri seçip kalibre edin

İlk kurulumda:

```bash
keyboard-mouse --calibrate
```

Komut önce bağlı ve daha önce kaydedilmiş klavyeleri listeler. Her klavye için
“kullanılsın mı?” sorusunu cevapladıktan sonra bağlı seçili klavyelerde Fn ve
tıklama tuşlarını kalibre eder.

Fn tuşu Linux'a gerçek bir olay gönderirse tetikleyici `Fn + Ctrl` olur. Çoğu
dizüstü klavyesinde Fn donanım içinde işlendiğinden olay görülmez; bu durumda
tetikleyici otomatik olarak **Caps Lock basılı tutma** olur.

## Hızlı başlangıç

```bash
keyboard-mouse --status
keyboard-mouse --calibrate
keyboard-mouse
```

Son komut varsayılan exclusive modda uygulamayı başlatır. Program otomatik
başlamaz ve terminal açık kaldığı sürece çalışır.

## Tuş eşlemeleri

Tetikleyici basılıyken:

| Tuş | Eylem |
| --- | --- |
| Sol/Sağ/Yukarı/Aşağı | Fare imlecini hareket ettirir |
| `+` | Sol tık |
| `-` | Sağ tık |
| `0` | Orta tık |

Ana sayı sırası ve numpad tuşlarının ikisi de desteklenir. Num Lock açık veya
kapalı olabilir. Klavye düzeninizde `+` için Shift gerekiyorsa tetikleyiciyle
birlikte Shift'e de basın. Tıklama tuşunu basılı tutmak sürükle-bırak yapar.

Hareket 180 piksel/sn hızla başlar ve 1,2 saniyede 1080 piksel/sn hıza çıkar.
Çapraz hareketin düz hareketten hızlı olmaması için hız normalize edilir.

## Komutların açıklaması

| Komut | Açıklama |
| --- | --- |
| `keyboard-mouse` | Önerilen exclusive modda başlatır; fare komutlarını uygulamadan gizler. |
| `keyboard-mouse --safe` | Fiziksel klavyeyi kilitlemeden çalışır; komut tuşları uygulamaya da ulaşabilir. |
| `keyboard-mouse --devices` | Kullanılacak bağlı veya kayıtlı klavye profillerini seçtirir. |
| `keyboard-mouse --calibrate` | Klavye seçimini açar ve bağlı seçili klavyeleri yeniden kalibre eder. |
| `keyboard-mouse --status` | Klavye, profil, bağlantı, eşleme ve izin durumunu gösterir. |
| `keyboard-mouse --stop` | PID'yi doğrulayıp çalışan sürece güvenli `SIGTERM` gönderir. |
| `keyboard-mouse --version` | Kurulu sürümü ve temel tıklama düzenini gösterir. |
| `keyboard-mouse --help` | Komut satırı yardımını gösterir. |
| `./install.sh` | Derler, test eder, ikiliyi ve udev kuralını sisteme kurar. |
| `./uninstall.sh` | İkiliyi ve udev kuralını kaldırır; ayarı korur. |
| `./uninstall.sh --purge-config` | Programla birlikte kullanıcı kalibrasyonunu da siler. |

## Klavye bağlantısı ve Bluetooth profilleri

Kullanılacak klavyeleri kalibrasyon yapmadan değiştirmek için:

```bash
keyboard-mouse --devices
```

- Bağlı yeni bir klavye seçilirse yalnız o klavye kalibre edilir.
- Kapalı fakat profili kayıtlı Bluetooth klavye listede görünür.
- Bağlı olmayan profil seçili bırakılırsa ayarları korunur.
- Profil seçilmezse yapılandırmadan kaldırılır.
- Çalışma sırasında seçili klavye ayrılırsa diğer seçili klavyeler devam eder.
- Klavye yeniden bağlanınca en geç iki saniye içinde otomatik etkinleştirilir.

Ayar dosyası:

```text
${XDG_CONFIG_HOME:-$HOME/.config}/keyboard-mouse/config.conf
```

Dosyadaki `start_speed`, `max_speed` ve `acceleration_seconds` değerleri elle
değiştirilebilir. Program çalışıyorsa değişiklikten sonra yeniden başlatın.

## Exclusive ve safe modları

Varsayılan `keyboard-mouse` komutu exclusive moddur. Fiziksel klavye olaylarını
yakalayıp normal tuşları sanal klavyeye aktarır; yalnız fare komutlarını tüketir.
Sanal klavye ile sanal fare ayrı aygıtlardır. Aynı kullanıcı altında ikinci
program kopyası başlatılamaz.

Sanal klavyesi masaüstü tarafından tanınmayan bir sistemde kurtarma amacıyla:

```bash
keyboard-mouse --safe
```

Safe mod fiziksel klavyeyi kilitlemez. Bunun karşılığında yön ve tıklama tuşları
açık uygulamaya da ulaşabilir.

## Durdurma ve acil kurtarma

Normal kapanış:

```text
Ctrl+C
```

Terminal Ctrl+C sinyalini iletmiyorsa fiziksel acil kombinasyon:

```text
Ctrl+Alt+Esc
```

Başka bir terminalden:

```bash
keyboard-mouse --stop
```

Eski sürüm `--stop` desteklemiyorsa:

```bash
pkill -TERM keyboard-mouse
```

Bu yollar basılı sanal tuşları bırakıp fiziksel klavye kilitlerini kaldırır.
Süreç beklenmedik biçimde kapanırsa kernel dosyaları kapatır ve `EVIOCGRAB`
kilidini otomatik bırakır. Son çare olarak oturumu kapatmak veya bilgisayarı
yeniden başlatmak kilidi kaldırır; donanıma kalıcı zarar gelmez.

## Sorun giderme

### Dahili klavye görünmüyor

```bash
./install.sh
```

Ardından oturumu kapatıp açın ve `keyboard-mouse --status` çalıştırın. Kurulum,
eski geç çalışan `99-` kuralını kaldırıp doğru sıralı `70-` kuralını kurar.

### Kurulumdan sonra eski davranış devam ediyor

```bash
keyboard-mouse --stop
./install.sh
keyboard-mouse --version
```

Çalışan eski süreç kurulum sırasında kendiliğinden değişmez. Sürüm çıktısının
`1.5.0` olduğundan emin olun ve programı yeniden başlatın.

### `/dev/uinput: izin gerekli`

Oturumu kapatıp yeniden açın. Devam ederse:

```bash
sudo modprobe uinput
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=input --action=change
sudo udevadm trigger --subsystem-match=misc --action=change
```

### Klavye exclusive modda çalışmıyor

Önce `Ctrl+Alt+Esc` veya `keyboard-mouse --stop` ile çıkın. Tanılama için safe
modu deneyin:

```bash
keyboard-mouse --safe
```

Sorun bildirirken dağıtım, masaüstü ortamı, `keyboard-mouse --status` çıktısı ve
klavye modelini ekleyin. Hassas bilgi içerebileceğinden ham tuş olaylarını
paylaşmayın.

## Başka Linux bilgisayara taşıma

Proje klasörünü Git, ZIP veya USB ile hedef bilgisayara aktarın ve o bilgisayarda
`./install.sh` çalıştırın. Hazır ikiliyi kopyalamak yerine hedefte derlemek,
işlemci mimarisi ve sistem kütüphaneleriyle uyumluluk sağlar. Her bilgisayarda
`--status`, `--devices` ve `--calibrate` adımlarını yeniden uygulayın.

## Güvenlik

Ham klavye olaylarını okumak uygulamanın çalışması için zorunludur. Udev kuralı
erişimi aktif yerel masaüstü kullanıcısına verir; aynı kullanıcının çalıştırdığı
diğer süreçler de izin verilen klavye olaylarını okuyabilir. Ayrıntılar için
[SECURITY.md](SECURITY.md) dosyasına bakın.

## Kaldırma

Programı ve udev kuralını kaldırıp kalibrasyonu korumak için:

```bash
./uninstall.sh
```

Kalibrasyonu da silmek için:

```bash
./uninstall.sh --purge-config
```

Otomatik başlangıç servisi kurulmadığından geride çalışan servis kalmaz.

## Geliştirme ve test

Sistem kurulumu yapmadan derlemek ve test etmek için:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Derlenen ikili `build/keyboard-mouse` konumundadır. Donanım testi için udev izni
ve `/dev/uinput` gerekir; CI yalnız derleme ve birim testlerini çalıştırır.

Katkı kuralları için [CONTRIBUTING.md](CONTRIBUTING.md), sürüm geçmişi için
[CHANGELOG.md](CHANGELOG.md) dosyasına bakın.

## Lisans

Bu proje [MIT Lisansı](LICENSE) ile dağıtılır.
