[Setup]
AppName=GradeGoal
AppVersion=1.0.0
DefaultDirName={autopf}\GradeGoal
DefaultGroupName=GradeGoal
OutputDir=..\..\dist-installer
OutputBaseFilename=GradeGoal_Setup_Windows_x64
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
SetupIconFile=..\..\assets\icon\icon.ico

[Files]
Source: "..\..\dist\bin\*"; DestDir: "{app}\bin"; Flags: ignoreversion recursesubdirs
Source: "..\..\dist\share\*"; DestDir: "{app}\share"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "..\..\dist\lib\*"; DestDir: "{app}\lib"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "..\..\dist\etc\*"; DestDir: "{app}\etc"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\GradeGoal"; Filename: "{app}\bin\GradeGoal.exe"; IconFilename: "{app}\bin\GradeGoal.exe"
Name: "{autodesktop}\GradeGoal"; Filename: "{app}\bin\GradeGoal.exe"; IconFilename: "{app}\bin\GradeGoal.exe"
