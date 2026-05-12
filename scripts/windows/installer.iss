; Inno Setup 6 script for Traktor.
;
; Invoked by .github/workflows/release.yml:
;   ISCC.exe /DAppVersion=<tag> /DVersionInfoVersion=<x.y.z.w>
;            /DSourceDir=<absolute path to build\>
;            /O. /F"Traktor-<tag>" scripts\windows\installer.iss
;
; SourceDir must already contain everything to ship next to Traktor.exe:
; Qt runtime DLLs (windeployqt output), libssl-*.dll, libcrypto-*.dll,
; z*.dll, WinSparkle.dll, and file.ico. The CMake POST_BUILD step in
; CMakeLists.txt already deposits WinSparkle.dll into build\.

#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif
#ifndef VersionInfoVersion
  #define VersionInfoVersion "0.0.0.0"
#endif
#ifndef SourceDir
  #error SourceDir must be defined via ISCC /DSourceDir=...
#endif

[Setup]
; AppId is permanent. Inno uses it to detect previous installs and
; upgrade in place. Changing it in a later release would orphan the
; existing install and leave a duplicate uninstall entry.
AppId={{8B5E2A6F-3C4D-4F8A-9A21-7E2B1D9C4F70}
AppName=Traktor
AppVersion={#AppVersion}
AppVerName=Traktor {#AppVersion}
AppPublisher=ServMask, Inc.
AppPublisherURL=https://traktor.wp-migration.com
AppSupportURL=https://github.com/servmask/Qtraktor/issues
AppUpdatesURL=https://github.com/servmask/Qtraktor/releases
DefaultDirName={autopf}\Traktor
DefaultGroupName=Traktor
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Always overridden by ISCC's /F<filename> flag in CI, but Inno rejects
; the directive's value if it contains an unresolved preprocessor expr,
; so keep a static literal here.
OutputBaseFilename=Traktor-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Inno resolves [Setup] paths relative to the .iss file's directory
; (per the "Source Directory" notes in the Inno Setup help), so jump up
; two levels to reach repo root where LICENSE and icons\ actually live.
LicenseFile=..\..\LICENSE
SetupIconFile=..\..\icons\traktor.ico
; Wizard bitmaps live next to this .iss file. Inno picks the closest match
; to the current DPI scale from the comma-separated list at install time.
WizardSmallImageFile=traktor-wizard-small.bmp,traktor-wizard-small@2x.bmp
WizardImageFile=traktor-wizard.bmp,traktor-wizard@2x.bmp
UninstallDisplayIcon={app}\Traktor.exe
ChangesAssociations=yes
CloseApplications=yes
RestartApplications=no
VersionInfoVersion={#VersionInfoVersion}
VersionInfoCompany=ServMask, Inc.
VersionInfoDescription=Traktor Installer
VersionInfoProductName=Traktor

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce

[Files]
; ignoreversion forces overwrite of files in {app} even when the
; existing copy has a newer VS_VERSION_INFO than the one we ship.
; Required because Qt and vcpkg DLL version stamps do not strictly
; increase across upstream upgrades, but we still want the bytes from
; the new build to land.
;
; Traktor_autogen\ holds Qt's AUTOMOC/AUTOUIC intermediate files
; (mocs_compilation.cpp.obj, qrc_resources.cpp.obj, qch metadata,
; moc_*.cpp.obj, etc.) emitted next to the .exe by CMake. None of
; that is runtime; excluding the whole subtree keeps {app} clean.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "*.obj,*.ilk,*.pdb,CMake*,cmake*,*.cmake,tst_*,_deps,_deps\*,Traktor_autogen,Traktor_autogen\*"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Traktor"; Filename: "{app}\Traktor.exe"
Name: "{autodesktop}\Traktor"; Filename: "{app}\Traktor.exe"; Tasks: desktopicon

[Registry]
; .wpress association via the ProgID wpress_auto_file. The ProgID name
; intentionally matches what Qt IFW's RegisterFileType operation used
; in earlier (unreleased) builds so any pinned Open With entries from
; mid-development upgraders survive.
Root: HKLM; Subkey: "Software\Classes\.wpress"; ValueType: string; ValueName: ""; ValueData: "wpress_auto_file"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "Software\Classes\.wpress"; ValueType: string; ValueName: "Content Type"; ValueData: "application/x-wpress"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "Software\Classes\MIME\Database\Content Type\application/x-wpress"; ValueType: string; ValueName: "Extension"; ValueData: ".wpress"; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Classes\wpress_auto_file"; ValueType: string; ValueName: ""; ValueData: "WPRESS Backup File"; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Classes\wpress_auto_file\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\file.ico"
Root: HKLM; Subkey: "Software\Classes\wpress_auto_file\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Traktor.exe"" ""%1"""

; "Extract with Traktor" context menu, machine-wide under HKLM (admin
; install convention; the menu becomes visible to all users on the
; machine, not just the installer-runner as it would under HKCU).
Root: HKLM; Subkey: "Software\Classes\wpress_auto_file\shell\Extract with Traktor"; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Classes\wpress_auto_file\shell\Extract with Traktor\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Traktor.exe"" ""%1"""

[Run]
Filename: "{app}\Traktor.exe"; Description: "{cm:LaunchProgram,Traktor}"; Flags: nowait postinstall skipifsilent
