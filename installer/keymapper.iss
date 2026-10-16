; Windows installer for keymapper, built by .github/actions/build-windows with Inno Setup (ISCC).
; The config (mappings.txt, devices\) lives in the install folder but is not listed under [Files], so an
; update never overwrites it and an uninstall never removes it.
; Defines passed on the command line: /DSourceDir=<folder with the collected app> /DAppVersion=<version>

#ifndef SourceDir
  #define SourceDir "..\package"
#endif
#ifndef AppVersion
  #define AppVersion "dev"
#endif

[Setup]
AppId={{6F1C2B84-3D5A-4E0B-9A57-2C8D4F7B1E63}
AppName=Keymapper
AppVersion={#AppVersion}
DefaultDirName={localappdata}\keymapper
DefaultGroupName=Keymapper
PrivilegesRequired=lowest
DisableProgramGroupPage=yes
OutputDir=..
OutputBaseFilename=keymapper-windows-setup
Compression=lzma2
SolidCompression=yes
UninstallDisplayIcon={app}\keymapper.exe
SetupIconFile=..\assets\keymapper.ico

[Tasks]
Name: "startup"; Description: "Start Keymapper when I sign in"; Flags: unchecked

[Files]
Source: "{#SourceDir}\keymapper.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\actions.windows.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\mappings.default.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\INSTALL.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\web\*"; DestDir: "{app}\web"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Keymapper"; Filename: "{app}\keymapper.exe"; WorkingDir: "{app}"
Name: "{userstartup}\Keymapper"; Filename: "{app}\keymapper.exe"; WorkingDir: "{app}"; Tasks: startup

[Run]
Filename: "{app}\keymapper.exe"; WorkingDir: "{app}"; Description: "Start Keymapper"; Flags: nowait postinstall skipifsilent

[Code]
// The running copy locks keymapper.exe: stop it before files are replaced or removed.
procedure StopKeymapper;
var
  code: Integer;
begin
  Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM keymapper.exe', '', SW_HIDE, ewWaitUntilTerminated, code);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  StopKeymapper;
  Result := '';
end;

function InitializeUninstall: Boolean;
begin
  StopKeymapper;
  Result := True;
end;
