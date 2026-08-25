#!/bin/sh
set -eu

echo "keyboard-mouse ve udev kurali kaldiriliyor (sudo gerekli)..."
sudo rm -f /usr/local/bin/keyboard-mouse
sudo rm -f /etc/udev/rules.d/70-keyboard-mouse.rules
sudo rm -f /etc/udev/rules.d/99-keyboard-mouse.rules
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=input --action=change
sudo udevadm trigger --subsystem-match=misc --action=change

if [ "${1:-}" = "--purge-config" ]; then
    CONFIG_ROOT=${XDG_CONFIG_HOME:-"${HOME}/.config"}
    rm -f "$CONFIG_ROOT/keyboard-mouse/config.conf"
    rmdir "$CONFIG_ROOT/keyboard-mouse" 2>/dev/null || true
    echo "Kalibrasyon ayari da silindi."
else
    echo "Kalibrasyon ayari korundu. Silmek icin: ./uninstall.sh --purge-config"
fi

echo "Kaldirma tamamlandi. Calisan bir surec varsa Ctrl+C ile durdurun."
