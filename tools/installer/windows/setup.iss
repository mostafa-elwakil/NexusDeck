; NexusDeck - Windows installer (per-user, no admin required)
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
AppPublisher=NexusDeck
DefaultDirName={localappdata}\Programs\NexusDeck
DefaultGroupName=NexusDeck
PrivilegesRequired=lowest
OutputDir=.
OutputBaseFilename=NexusDeck-Setup-{#MyAppVersion}-windows-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
CloseApplicationsFilter=NexusDeckCompanion.exe
UninstallDisplayName=NexusDeck
SetupIconFile=..\..\..\streamdeck-simulator\server\assets\nexusdeck.ico
UninstallDisplayIcon={app}\NexusDeckCompanion.exe
; Keep user profiles/settings on uninstall:
; profile_state.json and server_settings.json are left in {app}.

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked
Name: "startup"; Description: "Start NexusDeck automatically at Windows logon (runs in background)"; GroupDescription: "Background:"

[Files]
Source: "{#StagingDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\Open NexusDeck"; Filename: "{app}\Run-NexusDeck.bat"
Name: "{group}\Uninstall NexusDeck"; Filename: "{uninstallexe}"
Name: "{autodesktop}\NexusDeck"; Filename: "{app}\Run-NexusDeck.bat"; Tasks: desktopicon
Name: "{userstartup}\NexusDeck"; Filename: "{app}\NexusDeckCompanion.exe"; Parameters: "--minimized"; Tasks: startup

[Run]
Filename: "{app}\Run-NexusDeck.bat"; Description: "Launch NexusDeck now"; Flags: postinstall shellexec skipifsilent
