#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

#define AppName "Elehant SVT-15 Monitor"
#define AppPublisher "Elehant SVT-15 Monitor Project"

[Setup]
AppId={{7A2F86B1-DC24-4B7B-A7D7-5C7E92C1A315}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppComments=Мониторинг счётчиков Элехант СВТ-15 и просмотр графиков расхода воды
DefaultDirName=C:\Elehant
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputDir=..\release
OutputBaseFilename=Elehant_SVT15_Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
CloseApplications=yes
RestartApplications=no
SetupLogging=yes

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "Создать ярлык основного монитора на рабочем столе"; GroupDescription: "Дополнительные ярлыки:"; Flags: unchecked
Name: "graphdesktopicon"; Description: "Создать ярлык Graph Viewer на рабочем столе"; GroupDescription: "Дополнительные ярлыки:"; Flags: unchecked

[Dirs]
Name: "{app}\install"; Flags: uninsneveruninstall
Name: "{app}\docs"; Flags: uninsneveruninstall

[Files]
Source: "..\release\Elehant_SVT15_Monitor.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\release\Elehant_SVT15_Service.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\release\Elehant_Graph_Viewer.exe"; DestDir: "{app}"; Flags: ignoreversion
; Историю показаний устанавливать только если её ещё нет; если файла в сборке нет — пропустить строку.
; CSV создаётся ниже, если его ещё нет; существующая история не перезаписывается.
Source: "..\install\start_elephant_hidden.vbs"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\install\create_task.ps1"; DestDir: "{app}\install"; Flags: ignoreversion
Source: "..\docs\GRAPH_VIEWER.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Elehant SVT-15\Основной монитор"; Filename: "{app}\Elehant_SVT15_Monitor.exe"
Name: "{autoprograms}\Elehant SVT-15\Графики расхода воды"; Filename: "{app}\Elehant_Graph_Viewer.exe"
Name: "{autodesktop}\Elehant SVT-15 Monitor"; Filename: "{app}\Elehant_SVT15_Monitor.exe"; Tasks: desktopicon
Name: "{autodesktop}\Elehant Graph Viewer"; Filename: "{app}\Elehant_Graph_Viewer.exe"; Tasks: graphdesktopicon

[Run]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\install\create_task.ps1"""; StatusMsg: "Настройка автоматического запуска BLE-сборщика..."; Flags: runhidden waituntilterminated
Filename: "{app}\Elehant_SVT15_Monitor.exe"; Description: "Запустить основной монитор"; Flags: postinstall nowait skipifsilent

[UninstallRun]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -Command ""Unregister-ScheduledTask -TaskName 'Elehant SVT-15 Monitor' -Confirm:$false -ErrorAction SilentlyContinue"""; Flags: runhidden waituntilterminated

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
var
  CsvPath: string;
  CsvHeader: string;
begin
  if CurStep = ssPostInstall then
  begin
    CsvPath := ExpandConstant('{app}\\elehant_history.csv');
    if not FileExists(CsvPath) then
    begin
      CsvHeader := 'date;time;serial;tariff;bluetooth;volume_m3;temperature_c;battery_percent;firmware;rssi' + #13#10;
      if not SaveStringToFile(CsvPath, CsvHeader, False) then
        MsgBox('Не удалось создать файл истории:' + #13#10 + CsvPath + #13#10 + 'Проверьте права доступа к папке установки.', mbError, MB_OK);
    end;
  end;
end;
