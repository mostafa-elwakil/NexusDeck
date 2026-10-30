# NexusDeck: map the name "nexusdeck" to this PC (run as Administrator).
# After this, https://nexusdeck/ (or http://nexusdeck:8765/) opens Studio.
$hosts = "$env:SystemRoot\System32\drivers\etc\hosts"
$entry = "127.0.0.1`tnexusdeck"
$lines = @()
if (Test-Path $hosts) {
    $lines = Get-Content $hosts | Where-Object { $_ -notmatch '^\s*127\.0\.0\.1\s+nexusdeck(\s|$)' }
}
$lines += $entry
Set-Content -Path $hosts -Value $lines -Encoding ASCII -Force
Write-Host "Done. https://nexusdeck/ now resolves to this PC."
Write-Host "Then enable HTTPS: setx NEXUSDECK_HTTPS 1  (restart the server after)"
