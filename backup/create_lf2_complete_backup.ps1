<#
.SYNOPSIS
Erstellt EIN vollständiges, geprüftes LF2-Vita-Backup-ZIP aus den 5 lokalen Dateien.
.DESCRIPTION
PowerShell 5.1+ unter Windows oder PowerShell 7 unter macOS/Linux.
Benötigt keinen SDK-Build, installiert keine Software und verändert keine VPK.
GitHub-Tooling ist auf Commit e11ba19c... eingefroren.
Beispiel:
  powershell -ExecutionPolicy Bypass -File .\create_lf2_complete_backup.ps1 -InputDirectory C:\LF2Backup
  pwsh ./create_lf2_complete_backup.ps1 -InputDirectory ~/LF2Backup
#>
param(
    [string]$InputDirectory = ".",
    [string]$OutputZip = "LF2Vita_Gesamtbackup_2026-10-30.zip",
    [string]$GitHubSnapshotPath = ""
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression

$sourceDir = (Resolve-Path -LiteralPath $InputDirectory).Path
$destination = if ([System.IO.Path]::IsPathRooted($OutputZip)) {
    [System.IO.Path]::GetFullPath($OutputZip)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $sourceDir $OutputZip))
}
if (Test-Path -LiteralPath $destination) {
    throw "Zieldatei existiert bereits; bitte umbenennen statt überschreiben: $destination"
}

$required = @(
    @("01_Aktueller_Source", "LF2Vita_Round27_Vollstaendiger_Quellcode_2026-10-30.zip"),
    @("02_Alter_Workspace", "LF2Vita-next-chat-source-workspace-2026-09-13.zip"),
    @("03_Letzte_VPK", "LF2Vita_Round24_Stage_Performance_Test_2026-10-30.vpk"),
    @("04_VitaSDK", "Vita SDK(4).zip"),
    @("05_Patch", "LF2Vita_Round27_Performance_Profiling.patch")
)
$files = New-Object System.Collections.ArrayList
foreach ($pair in $required) {
    $path = Join-Path $sourceDir $pair[1]
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Pflichtdatei fehlt: $path"
    }
    [void]$files.Add([pscustomobject]@{ArchiveName=($pair[0] + "/" + $pair[1]); Path=$path})
}

$commit = "e11ba19ccefc198304a62dabb564411eb0a79475"
$githubArchiveName = "06_GitHub_Round28-33/LF2Vita_tooling_$commit.zip"
$temporaryGitHub = $null
try {
    if (![string]::IsNullOrWhiteSpace($GitHubSnapshotPath)) {
        $githubPath = (Resolve-Path -LiteralPath $GitHubSnapshotPath).Path
    } else {
        $temporaryGitHub = Join-Path ([System.IO.Path]::GetTempPath()) ("lf2_git_" + [guid]::NewGuid().ToString("N") + ".zip")
        $githubUrl = "https://github.com/DannyDLux90/LF2Vita/archive/$commit.zip"
        Write-Host "GitHub-Snapshot wird geladen (fixierter Commit $commit)..."
        Invoke-WebRequest -Uri $githubUrl -OutFile $temporaryGitHub -UseBasicParsing
        $githubPath = $temporaryGitHub
    }
    if (!(Test-Path -LiteralPath $githubPath -PathType Leaf)) {
        throw "GitHub-Quellarchiv nicht gefunden: $githubPath"
    }
    [void]$files.Add([pscustomobject]@{ArchiveName=$githubArchiveName;Path=$githubPath})

    $hashLines = New-Object System.Collections.ArrayList
    $inventory = New-Object System.Collections.ArrayList
    $expected = @{}
    foreach ($file in $files) {
        $digest = (Get-FileHash -LiteralPath $file.Path -Algorithm SHA256).Hash.ToLowerInvariant()
        $size = (Get-Item -LiteralPath $file.Path).Length
        $expected[$file.ArchiveName] = $digest
        [void]$hashLines.Add("$digest  $($file.ArchiveName)")
        [void]$inventory.Add([pscustomobject]@{
            path=$file.ArchiveName;size_bytes=$size;sha256=$digest
        })
        Write-Host "Erfasst: $($file.ArchiveName) ($size bytes)"
    }

    $readme = @"
LF2 VITA - GESAMTBACKUP (30.10.2026)

SPIELSTAND/VERZEICHNISSE:
* Round 27: letzter vollständiger zusammengeführter Quellcode (mit Stage-CPU-D-Pad, SELECT-Rennen, Trinkfix und Round-27-Performance).
* Workspace 13.09.2026: älterer, vollständiger Workspace mit Projekt-/Ressourcenstand; NICHT automatisch die letzte Round-27-Engine.
* Round 24: letzte tatsächlich gepackte installierbare VPK. Sie enthält die Round-25-bis-Round-27-Korrekturen NICHT.
* Vita SDK(4): zuletzt hochgeladener SDK-Bestand, nicht auf diesem System gegen Vita-Build geprüft.
* Round-27-Patch: Entwicklungsschritt gegenüber Round 26.
* GitHub Round 28-33: experimentelle Kollisionsfilter, CI-Tests, Werkzeuge, Dokumentation. Getrennt vom produktiven Round-27-Source. Fixierter Git-Commit: $commit.

ACHTUNG: KEINE NEUE ROUND-33-VPK. Weder 100 % PC-Treue noch FPS-Gewinn auf echter Vita sind nachgewiesen.
Die enthaltenen ZIP-/VPK-Dateien werden unverändert als einzelne Dateien gespeichert.
MANIFEST.json und SHA256SUMS.txt dienen der Inhaltsprüfung.
"@
    $manifest = [ordered]@{
        project = "LF2 Vita"
        backup_date = "2026-10-30"
        source_milestone = "Round 27"
        newest_installable_vpk = "Round 24"
        latest_experimental_github_commit = $commit
        caveat = "Historischer Workspace und letzte VPK sind älter als der Source; Round28-33 ist experimentelles Tooling, kein neuer Spiel-Build."
        items = @($inventory.ToArray())
    }
    $manifestText = $manifest | ConvertTo-Json -Depth 6
    $sumText = ($hashLines -join "`n") + "`n"

    function Add-BytesToZip([System.IO.Compression.ZipArchive]$archive,[string]$name,[byte[]]$data) {
        $entry = $archive.CreateEntry($name,[System.IO.Compression.CompressionLevel]::NoCompression)
        $stream = $entry.Open()
        try { $stream.Write($data,0,$data.Length) } finally { $stream.Dispose() }
    }
    $encoder = New-Object System.Text.UTF8Encoding($false)
    $outstream = [System.IO.File]::Open($destination,[System.IO.FileMode]::CreateNew)
    try {
        $archive = New-Object System.IO.Compression.ZipArchive($outstream,[System.IO.Compression.ZipArchiveMode]::Create,$true)
        try {
            foreach ($file in $files) {
                $entry = $archive.CreateEntry($file.ArchiveName,[System.IO.Compression.CompressionLevel]::NoCompression)
                $inputStream = [System.IO.File]::OpenRead($file.Path)
                $entryStream = $entry.Open()
                try { $inputStream.CopyTo($entryStream) }
                finally { $entryStream.Dispose();$inputStream.Dispose() }
            }
            Add-BytesToZip $archive "00_README_Backup.txt" ($encoder.GetBytes($readme))
            Add-BytesToZip $archive "00_MANIFEST.json" ($encoder.GetBytes($manifestText))
            Add-BytesToZip $archive "00_SHA256SUMS.txt" ($encoder.GetBytes($sumText))
        } finally { $archive.Dispose() }
    } finally { $outstream.Dispose() }

    Write-Host "Prüfe alle Dateien nochmals innerhalb des ZIP..."
    $verify = [System.IO.Compression.ZipFile]::OpenRead($destination)
    try {
        foreach ($file in $files) {
            $entry = $verify.GetEntry($file.ArchiveName)
            if ($null -eq $entry) { throw "Fehlender ZIP-Eintrag: $($file.ArchiveName)" }
            $sha = [System.Security.Cryptography.SHA256]::Create()
            $entryStream = $entry.Open()
            try {
                $digest = [System.BitConverter]::ToString($sha.ComputeHash($entryStream)).Replace("-","").ToLowerInvariant()
            } finally { $entryStream.Dispose();$sha.Dispose() }
            if ($digest -ne $expected[$file.ArchiveName]) {
                throw "Prüfsumme stimmt nicht: $($file.ArchiveName)"
            }
        }
        foreach ($meta in @("00_README_Backup.txt","00_MANIFEST.json","00_SHA256SUMS.txt")) {
            if ($null -eq $verify.GetEntry($meta)) {throw "Fehlender Metadateneintrag: $meta"}
        }
    } finally { $verify.Dispose() }

    $bytes = (Get-Item -LiteralPath $destination).Length
    Write-Host "ERFOLG: $destination ($bytes bytes; 6 Bestandteile + 3 Metadateien, SHA-256 geprüft)"
} finally {
    if ($temporaryGitHub -and (Test-Path -LiteralPath $temporaryGitHub)) {
        Remove-Item -LiteralPath $temporaryGitHub -Force
    }
}
