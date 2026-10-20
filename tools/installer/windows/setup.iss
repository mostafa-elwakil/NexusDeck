; NexusDeck - Windows installer (needs admin: maps https://nexusdeck in hosts
; and trusts its certificate during setup)
; Build from repo root:
;   iscc /DMyAppVersion="1.0.0" /DStagingDir="staging" /O"installer-out" tools\installer\windows\setup.iss
#ifndef MyAppVersion
  #define MyAppVersion "dev"
#endif
#ifndef StagingDir
  #define StagingDir "."
#endif

[Setup]
AppName=NexusDeck
AppVersion={#MyAppVersion}
AppVerName=NexusDeck {#MyAppVersion}
AppId={{7B4E2A1D-9C3F-4A5F-8D2E-1F3A5B6C7D8E}
AppPublisher=Mostafa Elwakil
AppCopyright=Copyright (C) 2026 Mostafa Elwakil
DefaultDirName={localappdata}\Programs\NexusDeck
DefaultGroupName=NexusDeck
PrivilegesRequired=admin
OutputDir=.
OutputBaseFilename=NexusDeck-Setup-{#MyAppVersion}-windows-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
CloseApplicationsFilter=NexusDeckCompanion.exe
UninstallDisplayName=NexusDeck
SetupIconFile=..\..\..\streamdeck-simulator\server\assets\nexusdeck.ico
UninstallDisplayIcon={app}\nexusdeck.ico
; Keep user profiles/settings on uninstall:
; profile_state.json and server_settings.json are left in {app}.

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"
Name: "startup"; Description: "Start NexusDeck automatically at Windows logon (runs in background)"; GroupDescription: "Background:"

[Files]
Source: "{#StagingDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\Open NexusDeck"; Filename: "{app}\Run-NexusDeck.bat"; IconFilename: "{app}\nexusdeck.ico"
Name: "{group}\Uninstall NexusDeck"; Filename: "{uninstallexe}"
Name: "{autodesktop}\NexusDeck"; Filename: "{app}\Run-NexusDeck.bat"; IconFilename: "{app}\nexusdeck.ico"; Tasks: desktopicon
Name: "{userstartup}\NexusDeck"; Filename: "{app}\NexusDeckCompanion.exe"; Parameters: "--minimized"; Tasks: startup

[Run]
; Map the name + trust the cert so https://nexusdeck/ works right after
; install (installer runs elevated, so no extra UAC prompts).
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -Command ""& '{app}\setup-hostname.ps1'; & '{app}\setup-https.ps1'"""; Flags: runhidden
Filename: "{app}\Run-NexusDeck.bat"; Description: "Launch NexusDeck now"; Flags: postinstall shellexec skipifsilent
