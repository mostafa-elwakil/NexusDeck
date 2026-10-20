# NexusDeck: trust the server's self-signed cert right after install.
# Runs elevated from the installer; waits briefly for the cert that the
# server generates on first HTTPS start (Documents\NexusDeck\nexusdeck.crt).
# If it does not exist yet (fresh install, server not launched yet), exit
# quietly - the manual fallback is tools/trust-cert.ps1 as admin.
param([string]$CertPath = "")
if (-not $CertPath) {
    $CertPath = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'NexusDeck\nexusdeck.crt'
}
$deadline = (Get-Date).AddSeconds(10)
while (-not (Test-Path $CertPath) -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
}
if (-not (Test-Path $CertPath)) {
    Write-Warning "cert not yet generated: $CertPath (run trust-cert.ps1 as admin after first launch)"
    exit 0
}
Import-Certificate -FilePath $CertPath -CertStoreLocation Cert:\LocalMachine\Root | Out-Null
Write-Host "Trusted. https://nexusdeck/ is the primary address."
