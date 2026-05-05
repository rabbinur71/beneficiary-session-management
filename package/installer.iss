#define MyAppName "Beneficiary Session Management"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Raaz"
#define MyAppExeName "BeneficiarySessionManagement.exe"

[Setup]
AppId={{8B97B949-7D8C-44E9-B880-RAAZ20260001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=output
OutputBaseFilename=BeneficiarySessionManagement_Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayName={#MyAppName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Dirs]
Name: "{commonappdata}\Beneficiary Session Management\data"; Permissions: users-modify

[Files]
Source: "app\*"; DestDir: "{app}"; Excludes: "data\camp.db"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "app\data\camp.db"; DestDir: "{commonappdata}\Beneficiary Session Management\data"; Flags: onlyifdoesntexist uninsneveruninstall

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent
