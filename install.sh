#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR="$PROJECT_DIR/build"
INSTALL_PATH=/usr/local/bin/keyboard-mouse

missing_tools=""
for tool in cmake c++ sudo install udevadm; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        missing_tools="$missing_tools $tool"
    fi
done
if [ -n "$missing_tools" ]; then
    echo "Eksik araclar:$missing_tools" >&2
    echo "Debian/Ubuntu/Mint: sudo apt install build-essential cmake udev" >&2
    echo "Fedora:             sudo dnf install gcc-c++ cmake systemd-udev" >&2
    echo "Arch/Manjaro:       sudo pacman -S base-devel cmake systemd" >&2
    echo "openSUSE:           sudo zypper install -t pattern devel_basis && sudo zypper install cmake systemd" >&2
    exit 1
fi

echo "Yonetici yetkisi dogrulaniyor..."
if ! sudo -v; then
    echo "Kurulum iptal edildi: sudo yetkisi alinamadi; eski program degistirilmedi." >&2
    exit 1
fi

echo "keyboard-mouse derleniyor..."
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo "Program ve sinirli udev erisim kurali kuruluyor (sudo gerekli)..."
sudo install -Dm755 "$BUILD_DIR/keyboard-mouse" "$INSTALL_PATH"
sudo rm -f /etc/udev/rules.d/99-keyboard-mouse.rules
sudo install -Dm644 "$PROJECT_DIR/packaging/70-keyboard-mouse.rules" \
    /etc/udev/rules.d/70-keyboard-mouse.rules
if command -v modprobe >/dev/null 2>&1; then
    sudo modprobe uinput
fi
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=input --action=change
sudo udevadm trigger --subsystem-match=misc --action=change

if ! sudo cmp -s "$BUILD_DIR/keyboard-mouse" "$INSTALL_PATH"; then
    echo "HATA: Kurulu program derlenen programla ayni degil." >&2
    echo "Eski surum kullaniliyor olabilir; kurulum basarisiz sayildi." >&2
    exit 1
fi

echo
echo "Kurulum tamamlandi ve ikili dosya dogrulandi:"
"$INSTALL_PATH" --version
echo
echo "Yeni tik duzeni: + sol tik, - sag tik, 0 orta tik"
if command -v pgrep >/dev/null 2>&1 && pgrep -x keyboard-mouse >/dev/null 2>&1; then
    echo "UYARI: Eski bir keyboard-mouse sureci halen calisiyor."
    echo "Onu Ctrl+C ile durdurup 'keyboard-mouse' komutunu yeniden calistirin."
fi
echo "Sirayla sunlari calistirin:"
echo "  keyboard-mouse --status"
echo "  keyboard-mouse --calibrate"
echo "  keyboard-mouse"
echo "Oturum izni hemen gorunmezse bir kez cikis yapip tekrar girin."
