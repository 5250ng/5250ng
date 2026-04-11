; 5250ng - Inno Setup Installer Script
; Creates a Windows installer for the 5250ng TN5250 terminal emulator.
;
; Usage:
;   iscc /DMyAppVersion=X.Y.Z 5250ng.iss
;
; Prerequisites:
;   Run "build.bat package" first to populate deploy\5250ng\ with the
;   application binary, Qt DLLs, and OpenSSL DLLs.

#ifndef MyAppVersion
  #define MyAppVersion "0.5.0"
#endif

#define MyAppName "5250ng"
#define MyAppPublisher "Remi GASCOU (Podalirius)"
#define MyAppURL "https://github.com/5250ng/5250ng"
#define MyAppExeName "5250ng.exe"

[Setup]
AppId={{E5A7D3F1-2B4C-4E8A-9F6D-1C3E5A7B9D2F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
LicenseFile=..\LICENSE
OutputDir=Output
OutputBaseFilename=5250ng-setup-x64
SetupIconFile=..\resources\icons\5250ng.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\deploy\5250ng\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
