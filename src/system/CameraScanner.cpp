#include "CameraScanner.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

std::vector<CamDevice> CameraScanner::foundCameras;
volatile bool CameraScanner::isScanning = false;
volatile bool CameraScanner::scanFinishedEvent = false;

// Hilfsfunktion: Entpackt komprimierte Namen aus dem DNS-Paket
static String parseDNSName(uint8_t* packet, int& idx, int len) {
    String name = "";
    int jumped = 0;
    int origIdx = idx;
    int i = idx;
    while (i < len && packet[i] > 0 && jumped < 10) {
        if ((packet[i] & 0xC0) == 0xC0) { // DNS Kompressions-Zeiger
            if (!jumped) origIdx = i + 2;
            i = ((packet[i] & 0x3F) << 8) | packet[i+1];
            jumped++;
            continue;
        }
        int labelLen = packet[i++];
        for (int j = 0; j < labelLen && i < len; j++) {
            name += (char)packet[i++];
        }
        name += ".";
    }
    if (!jumped) origIdx = i + 1;
    idx = origIdx;
    return name;
}

// Manueller UDP DNS Request an den Router (Reverse DNS)
static String getReverseDNS(String ipStr, IPAddress dnsServer) {
    IPAddress ip;
    if (!ip.fromString(ipStr)) return "";

    WiFiUDP udp;
    if (!udp.begin(esp_random() % 10000 + 10000)) return "";
    
    uint8_t req[100];
    memset(req, 0, sizeof(req));
    uint16_t transId = esp_random() & 0xFFFF;
    req[0] = transId >> 8; req[1] = transId & 0xFF;
    req[2] = 0x01; req[3] = 0x00; // Standard Query
    req[4] = 0x00; req[5] = 0x01; // 1 Frage
    
    String domain = String(ip[3]) + "." + String(ip[2]) + "." + String(ip[1]) + "." + String(ip[0]) + ".in-addr.arpa";
    int pos = 12;
    int start = 0;
    while (start < domain.length()) {
        int end = domain.indexOf('.', start);
        if (end == -1) end = domain.length();
        req[pos++] = end - start;
        for (int i = start; i < end; i++) req[pos++] = domain[i];
        start = end + 1;
    }
    req[pos++] = 0; // Terminating Byte
    req[pos++] = 0x00; req[pos++] = 0x0C; // QTYPE: PTR
    req[pos++] = 0x00; req[pos++] = 0x01; // QCLASS: IN
    
    udp.beginPacket(dnsServer, 53);
    udp.write(req, pos);
    udp.endPacket();
    
    uint32_t startMs = millis();
    while (millis() - startMs < 400) {
        int packetSize = udp.parsePacket();
        if (packetSize > 0) {
            uint8_t res[512];
            int len = udp.read(res, 512);
            if (len >= 12 && res[0] == (transId >> 8) && res[1] == (transId & 0xFF)) {
                int idx = pos; 
                if (idx + 12 < len) {
                    idx += 2; 
                    uint16_t type = (res[idx] << 8) | res[idx+1]; idx += 2;
                    if (type == 12) { // Ist PTR?
                        idx += 2; 
                        idx += 4; 
                        uint16_t rdlen = (res[idx] << 8) | res[idx+1]; idx += 2;
                        String hostname = parseDNSName(res, idx, len);
                        udp.stop();
                        if (hostname.endsWith(".")) hostname = hostname.substring(0, hostname.length() - 1);
                        if (hostname.endsWith(".local")) hostname = hostname.substring(0, hostname.length() - 6);
                        if (hostname.length() > 0) return hostname;
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    udp.stop();
    return "";
}

void CameraScanner::startScan() {
    if (isScanning) return;
    isScanning = true;
    scanFinishedEvent = false;
    xTaskCreatePinnedToCore(scanTask, "cam_scan", 4096, NULL, 5, NULL, 1);
}

void CameraScanner::scanTask(void *pvParameters) {
    foundCameras.clear();
    IPAddress localIP = WiFi.localIP();
    IPAddress gatewayIP = WiFi.gatewayIP(); // Hier fragen wir den Router nach dem DNS-Namen!
    String baseIP = String(localIP[0]) + "." + String(localIP[1]) + "." + String(localIP[2]) + ".";

    // ABSOLUT KRITISCH: Batch Size auf 6 limitiert, da ESP32 max ~16 parallele Sockets kann!
    const int BATCH_SIZE = 6; 
    for (int batch_start = 1; batch_start < 255; batch_start += BATCH_SIZE) {
        int socks[BATCH_SIZE];
        String ips[BATCH_SIZE];
        int max_fd = -1;
        fd_set writefds;
        FD_ZERO(&writefds);

        for (int i = 0; i < BATCH_SIZE; i++) {
            socks[i] = -1;
            int host = batch_start + i;
            if (host >= 255 || host == localIP[3]) continue; // Eigene IP ueberspringen

            String targetIP = baseIP + String(host);
            int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (s >= 0) {
                fcntl(s, F_SETFL, O_NONBLOCK); // Non-Blocking fuer ultra schnellen Scan
                struct sockaddr_in addr;
                memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_port = htons(8080);
                addr.sin_addr.s_addr = inet_addr(targetIP.c_str());

                connect(s, (struct sockaddr*)&addr, sizeof(addr));
                
                FD_SET(s, &writefds);
                if (s > max_fd) max_fd = s;
                socks[i] = s;
                ips[i] = targetIP;
            }
        }

        if (max_fd >= 0) {
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 300000; // 300ms Timeout per 6 IPs

            if (select(max_fd + 1, NULL, &writefds, NULL, &tv) > 0) {
                for (int i = 0; i < BATCH_SIZE; i++) {
                    if (socks[i] >= 0 && FD_ISSET(socks[i], &writefds)) {
                        int error = 0;
                        socklen_t len = sizeof(error);
                        getsockopt(socks[i], SOL_SOCKET, SO_ERROR, &error, &len);
                        
                        if (error == 0) {
                            CamDevice dev;
                            dev.ip = ips[i];
                            // Löst Reverse DNS via UDP beim Router auf!
                            String hostname = getReverseDNS(ips[i], gatewayIP);
                            if (hostname.length() > 0) {
                                dev.name = hostname;
                            } else {
                                dev.name = "BabyCam (" + ips[i] + ")";
                            }
                            foundCameras.push_back(dev);
                        }
                    }
                }
            }
        }

        for (int i = 0; i < BATCH_SIZE; i++) {
            if (socks[i] >= 0) close(socks[i]);
        }
        
        // Ganz wichtig: Dem Betriebssystem Luft lassen, sonst greift der Watchdog!
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }

    isScanning = false;
    scanFinishedEvent = true;
    vTaskDelete(NULL);
}