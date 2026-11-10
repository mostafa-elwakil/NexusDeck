# NexusDeck: trust the server's self-signed cert right after install.
# Runs elevated from the installer. The installer first runs
# NexusDeckCompanion.exe --ensure-cert, so the .crt normally already exists.
# The server's state dir is Documents\NexusDeck (APPDATA fallback when the
# user has no Documents folder); check both, then trust in the machine store.
param([string]$CertPath = "")

function Find-Cert {
    if ($CertPath) { return $CertPath }
    $candidates = @(
        (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'NexusDeck\nexusdeck.crt'),
        (Join-Path $env:USERPROFILE 'Documents\NexusDeck\nexusdeck.crt'),
        (Join-Path $env:APPDATA 'NexusDeck\nexusdeck.crt')
    )
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) { return $candidate }
    }
    return ""
}

$found = Find-Cert
if (-not $found) {
    Write-Warning "cert not generated yet (start NexusDeck once, then run tools\trust-cert.ps1 as admin)"
    exit 0
}
Import-Certificate -FilePath $found -CertStoreLocation Cert:\LocalMachine\Root | Out-Null
Write-Host "Trusted $found. https://nexusdeck/ is the primary address."
