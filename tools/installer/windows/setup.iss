; DockOps StreamDeck - Windows installer (per-user, no admin required)
; Build from repo root:
;   iscc /DMyAppVersion="1.0.0" /DStagingDir="staging" /O"installer-out" tools\installer\windows\setup.iss
#ifndef MyAppVersion
  #define MyAppVersion "dev"
#endif
#ifndef StagingDir
  #define StagingDir "."
#endif

[Setup]
AppName=DockOps StreamDeck
AppVersion={#MyAppVersion}
AppVerName=DockOps StreamDeck {#MyAppVersion}
AppId={{7B4E2A1D-9C3F-4A5F-8D2E-1F3A5B6C7D8E}
AppPublisher=DockOps
DefaultDirName={localappdata}\Programs\DockOps StreamDeck
DefaultGroupName=DockOps StreamDeck
PrivilegesRequired=lowest
OutputDir=.
OutputBaseFilename=DockOps-StreamDeck-Setup-{#MyAppVersion}-windows-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=DockOps StreamDeck
; Keep user profiles/settings on uninstall:
; profile_state.json and server_settings.json are left in {app}.

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked
Name: "startup"; Description: "Start StreamDeck automatically at Windows logon (runs in background)"; GroupDescription: "Background:"

[Files]
Source: "{#StagingDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\Open StreamDeck"; Filename: "{app}\Run-StreamDeck.bat"
Name: "{group}\Uninstall StreamDeck"; Filename: "{uninstallexe}"
Name: "{autodesktop}\DockOps StreamDeck"; Filename: "{app}\Run-StreamDeck.bat"; Tasks: desktopicon
Name: "{userstartup}\DockOps StreamDeck"; Filename: "{app}\StreamDeckCompanion.exe"; Tasks: startup

[Run]
Filename: "{app}\Run-StreamDeck.bat"; Description: "Launch StreamDeck now"; Flags: postinstall shellexec skipifsilent
