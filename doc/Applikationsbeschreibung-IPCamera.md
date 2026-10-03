<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 OpenKNX -->

<!-- KEINE MARKDOWN-TABELLEN in den DOC-Bloecken: die ETS zeigt sie als rohe Pipe-Zeichen an.
     Aufzaehlungen verwenden. Die Bloecke zwischen DOC und DOCEND werden von
     "OpenKNXproducer baggages" (Task "OpenKNXproducer Documentation") zu den Hilfetexten in
     src/Baggages/Help_de/ verarbeitet. Die Hilfetexte nicht von Hand bearbeiten. -->

# Applikationsbeschreibung IP Kamera

Das Modul bindet IP-Kameras, Video-Türklingeln und NVR-Kanäle an den KNX-Bus an. Ereignisse
der Kamera (Bewegung, KI-Erkennung, Klingeltaste, Erreichbarkeit) werden als KNX-Objekte
gesendet, Kamerafunktionen (Sirene, Flutlicht, Privatzone, Aufzeichnung, Chime usw.) lassen
sich über KNX schalten.

Je Kanal wird **eine Kamera** angebunden. Kameras werden auf der Seite **Kanalauswahl**
aktiviert; nur aktivierte Kameras erscheinen im ETS-Baum und werden von der Firmware
abgefragt.

## Wichtige Hinweise

* Diese KNXprod wird nicht von der KNX Association offiziell unterstützt!
* Die Erzeugung der KNXprod geschieht auf eure eigene Verantwortung!

# Allgemein

<!-- DOC -->
## Allgemein

Die Seite zeigt die Modulversion. Welche Kameras angebunden werden, wird auf der Seite
**Kanalauswahl** festgelegt: je Kamera „Deaktiviert" oder „Aktiviert" sowie eine
Beschreibung. Jede aktivierte Kamera bekommt eine eigene Seite mit Geräte-, Verbindungs- und
Ausstattungseinstellungen und einem eigenen Satz Kommunikationsobjekte.

<!-- DOCEND -->

# Kamera

<!-- DOC -->
## Kamera

Die Kameraseite beginnt mit der **Kanaldefinition** (Beschreibung der Kamera). Danach folgen
Gerät, Verbindung, Ausstattung, Verbindungsmodus und Abfrage. Welche Kommunikationsobjekte
eingeblendet werden, hängt vom Gerätetyp, vom Verbindungstyp und von den Ausstattungs-Haken
ab.

<!-- DOCEND -->

## Suspendiert

Legt eine fertig parametrierte Kamera still: Der Kanal bleibt mit allen Einstellungen und
Verknüpfungen erhalten, wird von der Firmware aber nicht angelegt und damit nicht abgefragt.
Suspendierte Kanäle tragen im ETS-Baum ein ⛔ vor der Beschreibung. Ausführlich beschrieben ist
der Parameter in der gemeinsamen Hilfeseite *Suspendiert*.

## Gerät

<!-- DOC -->
### Hersteller

- **Reolink**: vollständige Unterstützung über die Reolink-JSON-API (Ereignisse abfragen und
  Funktionen schalten).
- **Hikvision (experimentell)**: nur Ereignisse über ONVIF (Bewegung, Personen-, Fahrzeug-
  und Gesichtserkennung). Benötigt den Verbindungsmodus „ONVIF Long Poll" und einen ESP32.
- **Dahua (experimentell)**: wie Hikvision, nur Ereignisse über ONVIF.

<!-- DOCEND -->

<!-- DOC -->
### Gerätetyp

- **Kamera**: eigenständige IP-Kamera.
- **Türklingel**: Video-Türklingel. Blendet die Klingel-Objekte (Auslöser, gedrückt, Nicht
  stören, Klingel-LED, Auto-Reply) und die Option „Chime vorhanden" ein.
- **NVR-Kanal**: Kamera, die über einen Reolink-NVR angesprochen wird. Die IP-Adresse ist
  dann die des NVR, zusätzlich wird der **NVR Kanal-Index** benötigt.

<!-- DOCEND -->

<!-- DOC -->
### Verbindungstyp

- **LAN (PoE oder Netzteil)**: dauerhaft erreichbar.
- **WLAN (mit Netzteil)**: blendet zusätzlich das Objekt **WiFi-Signalstärke** (dBm) ein.
- **WLAN (Akku)**: blendet zusätzlich **WiFi-Signalstärke**, **Akkustand**, **Akku-Status**
  und **Kamera schläft** ein. Akkukameras schlafen zwischen Ereignissen; Abfragen können
  dann verzögert beantwortet werden.

<!-- DOCEND -->

<!-- DOC -->
### Chime

Nur bei Gerätetyp Türklingel: **Chime vorhanden** blendet den Abschnitt „Chime-Objekte" ein.
Dort lässt sich jedes Chime-Objekt einzeln ein- oder ausblenden:

- **Chime stumm**: schaltet den Gong stumm.
- **Chime Lautstärke**: Lautstärke 0–4 (0 = stumm).
- **Chime Klingelton**: Klingelton 0–9 (0 citybird, 1 originaltune, 2 pianokey, 3 loop,
  4 attraction, 5 hophop, 6 goodday, 7 operetta, 8 moonlight, 9 waybackhome). Der Klingelton
  gilt für den Auslöser.
- **Chime auslösen**: lässt den Gong mit dem eingestellten Klingelton ertönen.

<!-- DOCEND -->

<!-- DOC -->
### Verbindung

- **IP-Adresse**: IP-Adresse der Kamera bzw. des NVR im lokalen Netz.
- **HTTP-Port**: Port der Kamera-API (Standard 80).
- **Benutzername** / **Passwort**: Zugangsdaten eines Kamera-Benutzers.

<!-- DOCEND -->

<!-- DOC -->
### NVR-Kanal

Nur bei Gerätetyp NVR-Kanal: **NVR Kanal-Index** (0–15) der Kamera am NVR. Kanal 1 im NVR
entspricht dem Index 0.

<!-- DOCEND -->

## Ausstattung

<!-- DOC -->
### Ausstattung

Die Haken legen fest, welche Funktionen die Kamera hat, und blenden die zugehörigen
Kommunikationsobjekte ein: Bewegungserkennung, Flutlicht, Sirene, PTZ-Presets, IR-LEDs,
Privatzone, Aufzeichnung steuerbar, Auto-Tracking, IO-Eingang, Push-Benachrichtigungen und
Tag/Nacht-Umschaltung.

**Kamera auslesen:** Der Knopf fragt die Fähigkeiten der Kamera über das OpenKNX-Gerät ab
und setzt die Haken automatisch. Dazu müssen die Verbindungsdaten bereits in das Gerät
programmiert und die Kamera aktiviert sein. Danach das Gerät erneut programmieren, damit die
neuen Objekte wirksam werden.

<!-- DOCEND -->

<!-- DOC -->
### KI-Erkennung

Haken für die KI-Erkennungen der Kamera: **Person**, **Fahrzeug**, **Tier (Hund/Katze)**,
**Paket** und **Gesicht**. Je Haken wird das zugehörige Erkennungsobjekt eingeblendet. Auch
diese Haken setzt „Kamera auslesen".

<!-- DOCEND -->

<!-- DOC -->
### Zusätzliche Objekte

Objekte, die das Modul selbst bereitstellt und die der Assistent nicht verändert:

- **Erreichbarkeits-KO**: meldet, ob die Kamera erreichbar ist.
- **Sammelalarm-KO**: wird bei jedem Ereignis (Bewegung, KI-Erkennung, IO-Eingang)
  gesetzt und nach der Hold-Zeit zurückgesetzt.
- **Snapshot-Auslöser-KO**: sendet beim Beginn eines Sammelalarms einen kurzen Impuls, z. B.
  für eine Bildaufzeichnung in einer Visualisierung.

<!-- DOCEND -->

## Verbindungsmodus und Abfrage

<!-- DOC -->
### Verbindungsmodus

- **JSON Polling** (Standard): das Modul fragt die Reolink-API im Abfrageintervall ab. Läuft
  auf RP2040 und ESP32.
- **ONVIF Long Poll**: die Kamera meldet Ereignisse sofort über ONVIF. Nur auf ESP32.
- **ONVIF + JSON kombiniert**: Ereignisse über ONVIF, Zustände zusätzlich über JSON. Nur auf
  ESP32.

Bei ONVIF werden zusätzlich **ONVIF-Port** (Standard 80) und **ONVIF-Dienstpfad** (Standard
`/onvif/event_service`) eingeblendet.

<!-- DOCEND -->

<!-- DOC -->
### Snapshot-URL

Optionale URL, die beim Beginn eines Sammelalarms per HTTP GET aufgerufen wird. Die Antwort
(typisch ein JPEG) wird verworfen; der Aufruf dient als Webhook oder um einen Upload der
Kamera anzustoßen. Leer lassen, wenn nicht benötigt.

<!-- DOCEND -->

<!-- DOC -->
### Abfrage

- **Polling-Intervall**: Abstand der JSON-Abfragen (5, 10, 30 oder 60 Sekunden).
- **Hold-Zeit Alarm-KOs** (5–300 s): so lange bleiben Ereignisobjekte (Bewegung,
  KI-Erkennungen, IO-Eingang, Sammelalarm) nach dem letzten Ereignis auf 1, bevor sie auf 0
  zurückfallen.

<!-- DOCEND -->

# Kommunikationsobjekte

Je Kamera stehen bis zu 37 Objekte zur Verfügung. Eingeblendet werden nur die Objekte, die zu
Gerätetyp, Verbindungstyp und Ausstattung passen:

- **Status:** Kamera erreichbar, Sammelalarm, Snapshot auslösen.
- **Ereignisse:** Bewegung erkannt, Person, Fahrzeug, Haustier, Paket, Gesicht, IO-Eingang.
- **Steuerung:** Privacy-Modus, Aufzeichnung, Manuelle Aufnahme, Push-Benachrichtigungen,
  Sirene, Flutlicht, IR-LEDs, PTZ-Preset, Bewegungsempfindlichkeit, Tag/Nacht-Modus,
  Auto-Tracking.
- **Türklingel:** Auslöser, gedrückt, Nicht stören, Klingel-LED, Auto-Reply sowie die
  Chime-Objekte.
- **Akku/WLAN:** Akkustand, Akku-Status, Kamera schläft, WiFi-Signalstärke.

# Diagnose

Serielle Konsole (CC = Kanalnummer zweistellig, z. B. `ipc01`):

- `ipc<CC> status`: zeigt den Status der Kamera.
- `ipc<CC> poll`: erzwingt eine Abfrage.
- `ipc<CC> login`: erzwingt eine neue Anmeldung an der Kamera.
