#pragma once
#include <Arduino.h>
#include <vector>

struct CamDevice {
    String ip;
    String name;
};

class CameraScanner {
public:
    static std::vector<CamDevice> foundCameras;
    static volatile bool isScanning;
    static volatile bool scanFinishedEvent;

    static void startScan();
private:
    static void scanTask(void *pvParameters);
};