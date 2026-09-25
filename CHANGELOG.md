# Changelog OFM-IPCameraModule

## 0.3.0 - 2026-09-25

### Breaking
- **Kanalauswahl nach OpenKNX-Standard.** Der Schieberegler „Aktive Kameras" und der Tab
  „(mehr)" entfallen. Kameras werden auf der neuen Seite **Kanalauswahl** einzeln
  aktiviert (neuer Parameter je Kanal, Parameterblock wächst von 305 auf 306 Byte).
  **Bestehende Projekte: Applikation in der ETS aktualisieren, die genutzten Kameras in der
  Kanalauswahl aktivieren und das Gerät neu programmieren.**

### Added
- Seite **Kanalauswahl** (je Kamera eine Tabellenzeile: Kanal, Kanalaktivität,
  Beschreibung). Deaktivierte Kameras erscheinen nicht im ETS-Baum und werden von der
  Firmware nicht angelegt (bisher wurden immer alle 8 Kanäle angelegt).
- Applikationsbeschreibung `doc/Applikationsbeschreibung-IPCamera.md` und daraus erzeugte
  ETS-Hilfetexte (`src/Baggages/Help_de`, VS-Code-Task „OpenKNXproducer Documentation").

### Changed
- Kamera-Tab beginnt mit „Kanaldefinition"; das Feld „Bezeichnung" heißt jetzt
  „Beschreibung".
- Reolink: Der Zustand der Push-Benachrichtigungen (KO 7) wird bei jeder Abfrage von der
  Kamera zurückgelesen, damit Änderungen z. B. aus der Reolink-App sichtbar werden.

### Fixed
- Türklingel: KO 23 (Auslöser) sendet bei jedem Tastendruck einen Impuls (1 → 0), auch
  mehrfach innerhalb der Hold-Zeit. KO 24 (gedrückt) folgt direkt dem Tastenzustand statt
  einem Timer.

## 0.2.0 - 2026-07-31

### Added
- ONVIF-Client (Long Poll, WS-BaseNotification) mit Ereignisweiterleitung; Verbindungsmodus
  JSON Polling, ONVIF oder kombiniert (ONVIF nur auf ESP32).
- Kanäle für Hikvision und Dahua (experimentell, nur ONVIF-Ereignisse).
- Assistent „Kamera auslesen": liest die Fähigkeiten der Kamera und setzt die
  Ausstattungs-Haken in der ETS.
- ETS-Parameter ONVIF-Port, ONVIF-Dienstpfad, Verbindungstyp und HTTP-Port.

### Changed
- ETS-Parameter überarbeitet: KOs werden über Ausstattungs-Haken eingeblendet.
- Nach der ersten erfolgreichen Abfrage wird der Anfangszustand der KOs aktiv gesendet.

### Fixed
- Reolink-HTTP-API gegen echte Hardware korrigiert.
- ONVIF-Topics mit Namespace-Präfix werden korrekt ausgewertet.

## 0.1.0 - 2026-04-10

### Added
- Initial release
- Reolink camera support (motion, AI detection, doorbell, chime, siren, floodlight, PTZ, privacy, push)
- 8 channels, NVR channel index support
- Battery and WLAN parameter-controlled KO visibility
