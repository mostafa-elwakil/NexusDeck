# NexusDeck: trust the server's self-signed cert (run as Administrator).
# The certificate lives in the shared state folder and is also embedded in
# the Windows installer, which does this step automatically.
param([string]$CertPath = "")
if (-not $CertPath) {
    $CertPath = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'NexusDeck\nexusdeck.crt'
    if (-not (Test-Path $CertPath)) {
        $CertPath = Join-Path $env:APPDATA 'NexusDeck\nexusdeck.crt'
    }
}
if (-not (Test-Path $CertPath)) {
    Write-Error "Cert not found: $CertPath (start the server once; HTTPS is on by default)"
    exit 1
}
Import-Certificate -FilePath $CertPath -CertStoreLocation Cert:\LocalMachine\Root | Out-Null
Write-Host "Trusted. Restart the browser; https://nexusdeck/ should load without warnings."
