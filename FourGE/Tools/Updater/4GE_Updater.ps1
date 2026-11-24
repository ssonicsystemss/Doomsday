# =====================================================================
#  4GE UPDATER - sprawdza, czy na GitHubie (Releases "4ge-latest") jest nowszy build 4GE, SAM go pobiera i podmienia w VST3.
#  Uruchamiany co 10 minut przez Harmonogram zadan (-Quiet, z prawami administratora nadanymi raz przy instalacji) albo z ikony.
#   -Quiet    nic nie pokazuje, gdy nie ma nowej wersji; gdy FL jest otwarte - czeka (mowi o tym raz) i podmienia po zamknieciu FL
#   -Install  (gdy program nie ma praw administratora: uruchamia sie sam jeszcze raz z nimi) pobiera i podmienia 4GE.vst3
#  Pliki: %LOCALAPPDATA%\4GE_Updater (state.json = ktory build jest zainstalowany, log.txt = co sie dzialo).
# =====================================================================
param([switch]$Quiet, [switch]$Install)
$ErrorActionPreference = 'Stop'
$Repo  = 'ssonicsystemss/Doomsday'
$Tag   = '4ge-latest'
$Asset = '4GE-VST3-Windows.zip'
$Dst   = 'C:\Program Files\Common Files\VST3\4GE.vst3'
$Dir   = Join-Path $env:LOCALAPPDATA '4GE_Updater'
New-Item -ItemType Directory -Force $Dir | Out-Null
$StateFile = Join-Path $Dir 'state.json'
$LogFile   = Join-Path $Dir 'log.txt'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$Headers = @{ 'User-Agent' = '4GE-Updater'; 'Accept' = 'application/vnd.github+json' }
Add-Type -AssemblyName System.Windows.Forms

function Write-Log ([string]$m) { "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')  $m" | Add-Content -Path $LogFile -Encoding UTF8 }

function Show-Box ([string]$text, [string]$buttons = 'OK', [string]$icon = 'Information') {
    $owner = New-Object System.Windows.Forms.Form
    $owner.TopMost = $true; $owner.ShowInTaskbar = $false; $owner.WindowState = 'Minimized'
    $owner.Show(); $owner.Hide()
    $r = [System.Windows.Forms.MessageBox]::Show($owner, $text, '4GE Updater', $buttons, $icon)
    $owner.Dispose()
    return $r
}

function Get-State {
    if (Test-Path $StateFile) { try { return (Get-Content $StateFile -Raw -Encoding UTF8 | ConvertFrom-Json) } catch {} }
    return [pscustomobject]@{ installed = ''; skipped = ''; waiting = '' }
}
function Set-State ($s) { $s | ConvertTo-Json | Set-Content -Path $StateFile -Encoding UTF8 }

function Get-Latest {
    $r = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/tags/$Tag" -Headers $Headers -TimeoutSec 30
    $a = $r.assets | Where-Object { $_.name -eq $Asset } | Select-Object -First 1
    if (-not $a) { throw "w Releases nie ma paczki $Asset" }
    $sha = ''
    if ($r.body -match 'commit ([0-9a-f]{40})') { $sha = $Matches[1] }
    $id = $sha; if (-not $id) { $id = [string]$a.updated_at }
    $msg = ''
    if ($sha) {
        try { $c = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/commits/$sha" -Headers $Headers -TimeoutSec 30; $msg = ($c.commit.message -split "`n")[0] } catch {}
    }
    $date = ''
    try { $date = ([datetime]$a.updated_at).ToLocalTime().ToString('yyyy-MM-dd HH:mm') } catch { $date = [string]$a.updated_at }
    return [pscustomobject]@{ id = $id; sha = $sha; url = $a.browser_download_url; date = $date; msg = $msg }
}

function Test-FlOpen { return [bool](Get-Process -Name 'FL64', 'FL' -ErrorAction SilentlyContinue) }

function Install-Latest {
    $L = Get-Latest
    $tmp = Join-Path $env:TEMP '4GE_update'
    if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
    New-Item -ItemType Directory $tmp | Out-Null
    $zip = Join-Path $tmp '4GE.zip'
    Invoke-WebRequest -UseBasicParsing -Uri $L.url -OutFile $zip -TimeoutSec 300
    Expand-Archive -Path $zip -DestinationPath $tmp -Force
    $src = Get-ChildItem $tmp -Directory -Filter '4GE.vst3' -Recurse | Select-Object -First 1
    if (-not $src) { throw 'w pobranej paczce nie ma folderu 4GE.vst3' }
    if (Test-FlOpen) { throw 'FL Studio jest otwarte - zamknij FL i sprobuj jeszcze raz' }
    # the new copy goes next to the old one first: a failed copy never leaves FL without 4GE
    $new = "$Dst.new"; $old = "$Dst.old"
    foreach ($x in @($new, $old)) { if (Test-Path $x) { Remove-Item $x -Recurse -Force } }
    Copy-Item $src.FullName $new -Recurse
    if (Test-Path $Dst) { Rename-Item $Dst (Split-Path $old -Leaf) }
    Rename-Item $new (Split-Path $Dst -Leaf)
    if (Test-Path $old) { Remove-Item $old -Recurse -Force -ErrorAction SilentlyContinue }
    Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
    $s = Get-State; $s.installed = $L.id; $s.skipped = ''; $s | Add-Member -Force -NotePropertyName waiting -NotePropertyValue ''; Set-State $s
    Write-Log "zainstalowano $($L.id) ($($L.date))"
    return $L
}

# ---------------- -Install: the elevated part
if ($Install) {
    try {
        $L = Install-Latest
        Show-Box ("4GE zainstalowany.`n`nBuild: $($L.date)`n$($L.msg)`n`nMozesz otworzyc FL.") | Out-Null
        exit 0
    } catch {
        Write-Log "BLAD instalacji: $($_.Exception.Message)"
        Show-Box ("Nie udalo sie zainstalowac 4GE:`n`n$($_.Exception.Message)`n`n(szczegoly: $LogFile)") 'OK' 'Error' | Out-Null
        exit 1
    }
}

# ---------------- the check
try { $L = Get-Latest }
catch {
    Write-Log "nie moge sprawdzic: $($_.Exception.Message)"
    if (-not $Quiet) { Show-Box ("Nie moge sprawdzic aktualizacji (brak internetu?).`n`n$($_.Exception.Message)") 'OK' 'Warning' | Out-Null }
    exit 1
}
$s = Get-State
if (-not ($s.PSObject.Properties.Name -contains 'waiting')) { $s | Add-Member -NotePropertyName waiting -NotePropertyValue '' }
if ($s.installed -eq $L.id) {
    if (-not $Quiet) { Show-Box ("Masz najnowszy 4GE.`n`nBuild: $($L.date)`n$($L.msg)") | Out-Null }
    exit 0
}
# a new build: FL open = wait (FL holds the old 4GE.vst3); said once per build in the background, asked from the icon
while (Test-FlOpen) {
    if ($Quiet) {
        if ($s.waiting -ne $L.id) {
            $s.waiting = $L.id; Set-State $s
            Write-Log "nowy build $($L.id) czeka - FL otwarte"
            Show-Box ("Jest nowy 4GE ($($L.date)):`n$($L.msg)`n`nPodmienie go sam, gdy zamkniesz FL.") | Out-Null
        }
        exit 0
    }
    $r = Show-Box ("Jest nowy 4GE ($($L.date)):`n$($L.msg)`n`nFL Studio jest otwarte - zapisz projekt, zamknij FL i kliknij OK (Anuluj = podmienie sam pozniej).") 'OKCancel' 'Warning'
    if ($r -ne 'OK') { exit 0 }
}
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if ($isAdmin) {
    try {
        $L = Install-Latest
        Show-Box ("4GE zaktualizowany.`n`nBuild: $($L.date)`n$($L.msg)") | Out-Null
    } catch {
        Write-Log "BLAD instalacji: $($_.Exception.Message)"
        if (-not $Quiet) { Show-Box ("Nie udalo sie zainstalowac 4GE:`n`n$($_.Exception.Message)`n`n(szczegoly: $LogFile)") 'OK' 'Error' | Out-Null }
    }
    exit 0
}
# no administrator rights (started from the icon): the same script again, elevated (Windows asks)
try {
    Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -ArgumentList "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$PSCommandPath`" -Install"
} catch {
    Write-Log "brak zgody administratora"
    Show-Box "Bez zgody administratora nie moge podmienic pliku w Program Files." 'OK' 'Warning' | Out-Null
}
