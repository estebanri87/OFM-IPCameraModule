# OFM-IPCameraModule

OpenKNX-Modul zur Integration von IP-Kameras in den KNX-Bus. Unterstützt bidirektionale Steuerung und Ereignismeldungen.

## Release Notes

- 0.1.0 Initial Release: Unterstützung für Reolink-Kameras (Bewegung, KI-Erkennung, Türklingel, Klingelton, Sirene, Flutlicht, PTZ, Datenschutz, Push), 8 Kanäle, NVR-Kanalindex

## Unterstützte Hersteller

- **Reolink** (Phase 1–3, vollständige Unterstützung)
- Hikvision (experimentell)
- Dahua (experimentell)

## Funktionen (Reolink)

- Bewegungserkennung, KI-Erkennung (Person, Fahrzeug, Tier, Haustier, Paket, Gesicht, Babyalarm)
- Türklingelauslöser + Halten, Nicht-Stören, automatische Antwort
- Klingelton-Steuerung (Stummschalten, Lautstärke, Klingelton, manueller Auslöser)
- Sirene, Flutlicht, IR-LEDs, PTZ-Preset, Datenschutzmodus
- Push-Benachrichtigungen per KNX aktivieren/deaktivieren
- Akkustand, Schlafzustand (Akkukameras)
- WLAN-Signalstärke (WLAN-Kameras)
- Aufnahmesteuerung, Bewegungserkennung aktivieren/deaktivieren, automatisches Tracking
- 8 Kanäle pro Gerät, NVR-Kanalindex-Unterstützung

## Abhängigkeiten

Das Modul setzt [OFM-Network](https://github.com/OpenKNX/OFM-Network) oder [OFM-WLAN](https://github.com/mgeramb/OFM-WLANModule) voraus.

## Hardware Unterstützung

|Prozessor | Status | Anmerkung                  |
|----------|--------|----------------------------|
|RP2040    | Beta   |                            |
|ESP32     | Beta   |                            |

Getestete Hardware:
- [OpenKNX Reg1-ETH](https://github.com/OpenKNX/OpenKNX/wiki/REG1-Eth)

## Einbindung in die Anwendung

In das Anwendungs-XML muss das OFM-IPCameraModule aufgenommen werden:

```xml
  <op:define prefix="IPC" ModuleType="23"
    share=   "../lib/OFM-IPCameraModule/src/IPCameraModule.share.xml"
    template="../lib/OFM-IPCameraModule/src/IPCameraModule.templ.xml"
    NumChannels="8"
    KoOffset="100">
    <op:verify File="../lib/OFM-IPCameraModule/library.json" ModuleVersion="0.1" />
  </op:define>
```

**Hinweis:** Die Werte für `ModuleType`, `KoOffset` und `NumChannels` müssen je nach Anwendung angepasst werden.

In `main.cpp` muss das IPCameraModule ebenfalls hinzugefügt werden:

```
[...]
#include "IPCameraModule.h"
[...]

void setup()
{
    [...]
    openknx.addModule(1, openknxNetwork);
    openknx.addModule(3, openknxIPCameraModule);
    [...]
}
```

## KO-Präfix

`IPC`

## Lizenz

[CC0](LICENSE)
