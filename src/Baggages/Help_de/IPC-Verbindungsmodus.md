### Verbindungsmodus

- **JSON Polling** (Standard): das Modul fragt die Reolink-API im Abfrageintervall ab. Läuft
  auf RP2040 und ESP32.
- **ONVIF Long Poll**: die Kamera meldet Ereignisse sofort über ONVIF. Nur auf ESP32.
- **ONVIF + JSON kombiniert**: Ereignisse über ONVIF, Zustände zusätzlich über JSON. Nur auf
  ESP32.

Bei ONVIF werden zusätzlich **ONVIF-Port** (Standard 80) und **ONVIF-Dienstpfad** (Standard
`/onvif/event_service`) eingeblendet.

