import socket
import ipaddress
import threading


def get_local_ip():
    """Findet die echte WLAN-IP deines PCs heraus."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(('10.255.255.255', 1))
        ip = s.getsockname()[0]
    except Exception:
        ip = '127.0.0.1'
    finally:
        s.close()
    return ip


def check_port(ip, port, found_devices):
    """Versucht einen TCP-Verbindungsaufbau """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(0.5)  # Nur eine halbe Sekunde warten
    try:
        # connect_ex liefert 0 zurück, wenn der Port offen ist
        if sock.connect_ex((str(ip), port)) == 0:
            found_devices.append(str(ip))
    except Exception:
        pass
    finally:
        sock.close()


def main():
    local_ip = get_local_ip()
    print(f"[*] Eigene IP ermittelt: {local_ip}")

    # Subnetz ermitteln (geht davon aus, dass es ein Standard /24 Netz ist)
    network = ipaddress.IPv4Network(local_ip + '/24', strict=False)
    print(f"[*] Scanne das Netzwerk {network} nach BabyCam-Sendern (TCP Port 8080)...")

    found_devices = []
    threads = []

    # Starte die App jetzt als SENDER auf dem Handy!

    # Für jede IP im WLAN einen schnellen Scan-Thread starten
    for ip in network.hosts():
        if str(ip) == local_ip:
            continue  # Uns selbst ignorieren

        t = threading.Thread(target=check_port, args=(ip, 8080, found_devices))
        threads.append(t)
        t.start()

    # Auf alle warten (dauert durch den Timeout max 0.5 Sekunden)
    for t in threads:
        t.join()

    print("\n" + "=" * 50)
    if found_devices:
        for device in found_devices:
            print(f"[!!!] TREFFER! Gerät mit offenem Kamera-Port gefunden: {device}")
            print(f"[*] Tippe http://{device}:8080 in deinen Browser ein, um es zu prüfen!")
    else:
        print("[-] Kein BabyCam-Sender gefunden. Läuft die App auf dem Handy als Sender?")
    print("=" * 50)


if __name__ == '__main__':
    main()