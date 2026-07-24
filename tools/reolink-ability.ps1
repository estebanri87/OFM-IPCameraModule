<#
.SYNOPSIS
    Liest die Faehigkeiten einer Reolink-Kamera aus und zeigt an, welche Haekchen
    in der ETS unter "Ausstattung" zu setzen sind.

.DESCRIPTION
    Fragt GetAbility, GetAiState und GetChannelstatus ab (rein lesend, keine
    Aenderung am Geraet) und stellt das Ergebnis als Tabelle dar.

.EXAMPLE
    .\reolink-ability.ps1 -Ip 192.168.1.100 -User admin -Password geheim
    .\reolink-ability.ps1 -Ip 192.168.1.100 -User admin -Password geheim -Port 8000
#>
param(
    [Parameter(Mandatory = $true)][string]$Ip,
    [Parameter(Mandatory = $true)][string]$User,
    [Parameter(Mandatory = $true)][string]$Password,
    [int]$Port = 80,
    [int]$NvrChannel = 0
)

$ErrorActionPreference = 'Stop'
$base = "http://${Ip}:${Port}/api.cgi"

function Invoke-Reolink {
    param([string]$Cmd, [string]$Body, [string]$Token)
    $url = if ($Token) { "${base}?cmd=$Cmd&token=$Token" } else { "${base}?cmd=$Cmd" }
    try {
        $r = Invoke-WebRequest -Uri $url -Method POST -Body $Body -ContentType 'application/json' `
                               -TimeoutSec 15 -UseBasicParsing -DisableKeepAlive
        return ($r.Content | ConvertFrom-Json)
    } catch {
        Write-Host "  Fehler bei ${Cmd}: $($_.Exception.Message)" -ForegroundColor Red
        return $null
    }
}

Write-Host ""
Write-Host "Verbinde mit $Ip`:$Port ..." -ForegroundColor Cyan

$loginBody = "[{`"cmd`":`"Login`",`"action`":0,`"param`":{`"User`":{`"userName`":`"$User`",`"password`":`"$Password`"}}}]"
$login = Invoke-Reolink -Cmd 'Login' -Body $loginBody
$token = $login[0].value.Token.name
if (-not $token) {
    Write-Host "Login fehlgeschlagen. Bitte IP, Port und Zugangsdaten pruefen." -ForegroundColor Red
    exit 1
}

$info = Invoke-Reolink -Cmd 'GetDevInfo' -Body '[{"cmd":"GetDevInfo","action":0}]' -Token $token
$dev  = $info[0].value.DevInfo
$ab   = (Invoke-Reolink -Cmd 'GetAbility' -Token $token `
          -Body "[{`"cmd`":`"GetAbility`",`"action`":0,`"param`":{`"User`":{`"userName`":`"$User`"}}}]")[0].value.Ability
$chn  = $ab.abilityChn[$NvrChannel]
$ai   = (Invoke-Reolink -Cmd 'GetAiState' -Token $token `
          -Body "[{`"cmd`":`"GetAiState`",`"action`":0,`"param`":{`"channel`":$NvrChannel}}]")[0].value

Write-Host ""
Write-Host "  Modell   : $($dev.model)"
Write-Host "  Firmware : $($dev.firmVer)"
Write-Host "  Typ      : $($dev.type)   Kanaele: $($dev.channelNum)"
Write-Host ""

function P($node) { if ($node -and $node.permit -gt 0) { return $true } return $false }

# Fuer die KI-Typen ist GetAiState massgeblich: nur was dort "support":1 meldet,
# taucht spaeter im Polling ueberhaupt auf.
function AiSup($name) { if ($ai.$name -and $ai.$name.support -eq 1) { return $true } return $false }

$rows = @(
    [pscustomobject]@{ Haekchen='Gerätetyp = Türklingel'; Quelle='DevInfo.type';               Wert=$dev.type;                          Setzen=($dev.type -eq 'BELL') }
    [pscustomobject]@{ Haekchen='Flutlicht';              Quelle='floodLight/supportFLswitch'; Wert=$chn.floodLight.permit;             Setzen=((P $chn.floodLight) -or (P $chn.supportFLswitch)) }
    [pscustomobject]@{ Haekchen='Sirene';                 Quelle='alarmAudio';                 Wert=$chn.alarmAudio.permit;             Setzen=(P $chn.alarmAudio) }
    [pscustomobject]@{ Haekchen='PTZ-Presets';            Quelle='ptzPreset';                  Wert=$chn.ptzPreset.permit;              Setzen=(P $chn.ptzPreset) }
    [pscustomobject]@{ Haekchen='IR-LEDs';                Quelle='ledControl';                 Wert=$chn.ledControl.permit;             Setzen=(P $chn.ledControl) }
    [pscustomobject]@{ Haekchen='Privatzone';             Quelle='mask';                       Wert=$chn.mask.permit;                   Setzen=(P $chn.mask) }
    [pscustomobject]@{ Haekchen='Aufzeichnung';           Quelle='recCfg';                     Wert=$chn.recCfg.permit;                 Setzen=(P $chn.recCfg) }
    [pscustomobject]@{ Haekchen='Auto-Tracking';          Quelle='aiTrack';                    Wert=$chn.aiTrack.permit;                Setzen=(P $chn.aiTrack) }
    [pscustomobject]@{ Haekchen='IO-Eingang';             Quelle='alarmIoIn';                  Wert=$chn.alarmIoIn.permit;              Setzen=(P $chn.alarmIoIn) }
    [pscustomobject]@{ Haekchen='Push';                   Quelle='push';                       Wert=$ab.push.permit;                    Setzen=(P $ab.push) }
    [pscustomobject]@{ Haekchen='KI: Person';             Quelle='GetAiState.people';          Wert=$ai.people.support;                 Setzen=(AiSup 'people') }
    [pscustomobject]@{ Haekchen='KI: Fahrzeug';           Quelle='GetAiState.vehicle';         Wert=$ai.vehicle.support;                Setzen=(AiSup 'vehicle') }
    [pscustomobject]@{ Haekchen='KI: Tier';               Quelle='GetAiState.dog_cat';         Wert=$ai.dog_cat.support;                Setzen=(AiSup 'dog_cat') }
    [pscustomobject]@{ Haekchen='KI: Paket';              Quelle='GetAiState.package';         Wert=$ai.package.support;                Setzen=(AiSup 'package') }
    [pscustomobject]@{ Haekchen='KI: Gesicht';            Quelle='GetAiState.face';            Wert=$ai.face.support;                   Setzen=(AiSup 'face') }
    [pscustomobject]@{ Haekchen='Verbindungstyp = WLAN';  Quelle='DevInfo.wifi';               Wert=$dev.wifi;                          Setzen=($dev.wifi -eq 1) }
    [pscustomobject]@{ Haekchen='Verbindungstyp = Akku';  Quelle='battery';                    Wert=$chn.battery.permit;                Setzen=(P $chn.battery) }
    [pscustomobject]@{ Haekchen='Chime vorhanden';        Quelle='supportDingDongCtrl';        Wert=$chn.supportDingDongCtrl.permit;    Setzen=(P $chn.supportDingDongCtrl) }
    [pscustomobject]@{ Haekchen='Klingel-LED';            Quelle='supportDoorbellLight';       Wert=$chn.supportDoorbellLight.permit;   Setzen=(P $chn.supportDoorbellLight) }
    [pscustomobject]@{ Haekchen='Auto-Reply';             Quelle='supportAutoReply';           Wert=$chn.supportAutoReply.permit;       Setzen=(P $chn.supportAutoReply) }
)

$rows | ForEach-Object {
    $mark = if ($_.Setzen) { '[x]' } else { '[ ]' }
    $col  = if ($_.Setzen) { 'Green' } else { 'DarkGray' }
    $wert = if ($null -eq $_.Wert) { '-' } else { $_.Wert }
    Write-Host ("  {0} {1,-24} {2,-28} {3}" -f $mark, $_.Haekchen, $_.Quelle, $wert) -ForegroundColor $col
}

# Chime: gekoppelte Geraete auflisten (nur bei Tuerklingeln vorhanden)
if (P $chn.supportDingDongCtrl) {
    $dd = Invoke-Reolink -Cmd 'GetDingDongList' -Body '[{"cmd":"GetDingDongList","action":0}]' -Token $token
    $paired = @($dd[0].value.DingDongList.pairedlist | Where-Object { $_.deviceId -gt 0 })
    Write-Host ""
    if ($paired.Count -gt 0) {
        Write-Host "  Gekoppelte Chimes:" -ForegroundColor Cyan
        $paired | ForEach-Object { Write-Host ("    id={0}  {1}" -f $_.deviceId, $_.deviceName) }
    } else {
        Write-Host "  Kein Chime gekoppelt - Haekchen 'Chime vorhanden' nicht setzen." -ForegroundColor Yellow
    }
}

Invoke-Reolink -Cmd 'Logout' -Body '[{"cmd":"Logout","action":0,"param":{}}]' -Token $token | Out-Null
Write-Host ""
Write-Host "Fertig. Es wurde nichts am Geraet veraendert." -ForegroundColor Cyan
Write-Host ""
