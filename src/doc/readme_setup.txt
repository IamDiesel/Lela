Necessary tweaks:

1) Open VS Code Terminal and install: C:\Users\Lenovo\.platformio\penv\Scripts\pip.exe install "click==8.1.8"
   (any 8.1.x works; click >= 8.2 breaks esptool: "get_metavar() missing ... 'ctx'")

Since v2.0.0 no longer needed: deleting ESP8266Audio/src/AudioOutputPDM.cpp
(the unused ESP8266Audio dependency was removed). Details: README.md, 5.2.
