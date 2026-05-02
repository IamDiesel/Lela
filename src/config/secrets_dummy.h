#pragma once

// WLAN Zugangsdaten
#define SECRET_WIFI_SSID "DEIN_WLAN_NAME"
#define SECRET_WIFI_PASS "DEIN_WLAN_PASSWORT"

// Home Assistant Server
#define SECRET_HA_IP "192.168.1.100"
#define SECRET_HA_PORT 8123
#define SECRET_HA_TOKEN "DEIN_LONG_LIVED_ACCESS_TOKEN"

// MQTT Broker
#define SECRET_MQTT_PORT 1883
#define SECRET_MQTT_USER "mqtt_user"
#define SECRET_MQTT_PASS "mqtt_pass"

// Kamera & Snapshot Pfade
#define SECRET_CAM_ENTITY "camera.deine_kamera"
#define SECRET_CAM_SNAPSHOT_PATH "/local/snap/snapshot.jpg"

// Kamera Stream IP
#define SECRET_STREAM_IP "192.168.1.100"

// Bluetooth MAC Adressen
#define SECRET_MAC_MAT "00:00:00:00:00:00"
#define SECRET_MAC_KIPPY "00:00:00:00:00:00"

// Default MQTT Topics
#define SECRET_MQTT_BABY_TOPIC "smartmat/baby/cry/state"
#define SECRET_MQTT_CAM_TRIGGER "camera/trigger/snapshot"

#define SECRET_MQTT_CATMAT_CMD_SET    "smartmat/switch/stream/set"
#define SECRET_MQTT_CATMAT_CMD_STATE  "smartmat/switch/stream/state"
#define SECRET_MQTT_CATMAT_DISCONNECT "smartmat/switch/disconnect/state"
#define SECRET_MQTT_CATMAT_PRESSURE   "smartmat/sensor/pressure"
#define SECRET_MQTT_CATMAT_MAT_RSSI   "smartmat/sensor/mat_rssi"
#define SECRET_MQTT_CATMAT_KIPPY_RSSI "smartmat/sensor/kippy_rssi"

// Audio Stream
#define SECRET_BABY_STREAM_URL "http://192.168.1.100:1984/api/stream.aac?src=baby_audio_esp"