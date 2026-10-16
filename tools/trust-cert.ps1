# NexusDeck: trust the self-signed cert so browsers stop warning (run as Administrator).
# Usage: .\trust-cert.ps1 [-CertPath <path-to-nexusdeck.crt>]
param([string]$CertPath = "")
if (-not $CertPath) {
    $CertPath = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'NexusDeck\nexusdeck.crt'
}
if (-not (Test-Path $CertPath)) {
    Write-Error "Cert not found: $CertPath (start the server once with NEXUSDECK_HTTPS=1 first)"
    exit 1
}
Import-Certificate -FilePath $CertPath -CertStoreLocation Cert:\LocalMachine\Root | Out-Null
Write-Host "Trusted. Restart the browser; https://nexusdeck/ should load without warnings."
