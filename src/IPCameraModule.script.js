// Assistent "Kamera auslesen".
// Die Gegenstelle im Geraet ist IPCameraModule::processFunctionProperty.
// Ablauf wie beim OFM-SolarmanPV: CmdQuery stoesst an und kehrt sofort zurueck,
// CmdStatus wird gepollt, bis resultData[1] == 1 das Ergebnis meldet.

var IPC_FUNCTION_OBJECT   = 0xA2;
var IPC_FUNCTION_PROPERTY = 1;
var IPC_CMD_QUERY  = 1;
var IPC_CMD_STATUS = 2;

// Bit -> Parametername. Reihenfolge muss zu IPCameraModule::processFunctionProperty passen.
var IPC_ABILITY_BITS = [
    "HasFloodlight",    // Byte 0, Bit 0
    "HasSiren",
    "HasPtz",
    "HasIrLeds",
    "HasPrivacyMask",
    "HasRecording",
    "HasAutoTracking",
    "HasIoInput"
];
var IPC_AI_BITS = [
    "AiPerson",         // Byte 1, Bit 0
    "AiVehicle",
    "AiAnimal",
    "AiPackage",
    "AiFace",
    "HasMotion",        // Bit 5: klassische Bewegungserkennung, keine KI
    "HasPush",          // Bit 6
    "HasDayNight"       // Bit 7
];
// KoAnyAlarm, KoSnapshot und KoOnline sind bewusst nicht dabei: das sind
// Entscheidungen des Integrators, keine Faehigkeiten der Kamera.

function IPC_paramName(channel, name) {
    return "IPC_CH" + channel + name;
}

function IPC_applyBits(device, channel, names, value) {
    var applied = 0;
    for (var i = 0; i < names.length; i++) {
        var par = device.getParameterByName(IPC_paramName(channel, names[i]));
        if (!par) continue;
        par.value = (value >> i) & 1;
        applied++;
    }
    return applied;
}

function IPC_readAbility(device, online, progress, context) {
    var channel = Number(context.channel);

    progress.setProgress(5);
    progress.setText("IP Kamera " + channel + ": verbinde mit dem Geraet ...");
    online.connect();

    try {
        // 1. Abfrage anstossen
        var resp = BASE_invokeFunctionPropertyWrapper(
            IPC_FUNCTION_OBJECT, IPC_FUNCTION_PROPERTY, [IPC_CMD_QUERY, channel],
            device, online, progress);

        if (!resp || resp.length < 1 || resp[0] != 0) {
            progress.setText("IP Kamera " + channel + ": Geraet hat die Abfrage abgelehnt.");
            return;
        }

        // 2. Auf das Ergebnis warten. Login und GetAbility dauern typisch 1-3 s,
        //    bei nicht erreichbarer Kamera laeuft der Timeout im Geraet.
        progress.setText("IP Kamera " + channel + ": frage Kamera ab ...");
        var done = null;
        for (var attempt = 0; attempt < 30; attempt++) {
            IPC_sleep(1000);
            progress.setProgress(10 + attempt * 3);

            var status = BASE_invokeFunctionPropertyWrapper(
                IPC_FUNCTION_OBJECT, IPC_FUNCTION_PROPERTY, [IPC_CMD_STATUS, channel],
                device, online, progress);

            if (!status || status.length < 2 || status[0] != 0) {
                progress.setText("IP Kamera " + channel + ": Statusabfrage fehlgeschlagen.");
                return;
            }
            if (status[1] == 1) { done = status; break; }
        }

        if (!done) {
            progress.setText("IP Kamera " + channel + ": Zeitueberschreitung, Kamera antwortet nicht.");
            return;
        }

        // done: [0]=Status [1]=fertig [2]=Fehlercode [3]=Faehigkeiten [4]=KI
        if (done.length < 5) {
            progress.setText("IP Kamera " + channel + ": unerwartete Antwortlaenge.");
            return;
        }
        if (done[2] != 0) {
            progress.setText("IP Kamera " + channel + ": Kamera nicht erreichbar oder Login abgelehnt (Code " + done[2] + ").");
            return;
        }

        var n = IPC_applyBits(device, channel, IPC_ABILITY_BITS, done[3]);
        n += IPC_applyBits(device, channel, IPC_AI_BITS, done[4]);

        progress.setProgress(100);
        progress.setText("IP Kamera " + channel + ": " + n + " Einstellungen uebernommen. Bitte erneut programmieren.");
    } finally {
        online.disconnect();
    }
}

function IPC_sleep(milliseconds) {
    var start = new Date().getTime();
    while (new Date().getTime() - start < milliseconds) { /* warten */ }
}
