# Полный установщик Windows

## Что входит

Единый установщик включает:

- \`Elehant_SVT15_Monitor.exe\` — основное окно мониторинга;
- \`Elehant_SVT15_Service.exe\` — фоновый сбор показаний BLE;
- \`Elehant_Graph_Viewer.exe\` — графики расхода и последние сохранённые показания;
- сценарий запуска фонового сервиса при входе в Windows;
- ярлыки основного монитора и Graph Viewer;
- лицензию проекта и инструкцию Graph Viewer.

Программа устанавливается в \`C:\Elehant\`. Установщик не включает личный CSV-файл истории.

## Получение установщика

### Последняя сборка

1. Открой [GitHub Actions — Full Windows Installer](https://github.com/alisatsyplyarova-art/elehant-svt15-monitor/actions/workflows/full-installer-build.yml).
2. Выбери последний успешный запуск.
3. В разделе **Artifacts** скачай \`Elehant_SVT15_Full_Installer_Windows_x64\`.
4. Распакуй архив и запусти \`Elehant_SVT15_Setup.exe\` от имени администратора.

### Постоянный релиз

Чтобы создать опубликованный релиз, создай и отправь тег, например:

\`\`\`powershell
git tag elehant-v1.0.0
git push origin elehant-v1.0.0
\`\`\`

Workflow соберёт установщик и опубликует его в GitHub Releases. Перед созданием следующего релиза используй новый номер версии.

## Обновление существующей установки

1. Не удаляй \`C:\Elehant\elehant_history.csv\`.
2. Запусти новый установщик от имени администратора.
3. Оставь каталог установки \`C:\Elehant\`.
4. После установки проверь, что существуют:
   - \`Elehant_SVT15_Monitor.exe\`;
   - \`Elehant_SVT15_Service.exe\`;
   - \`Elehant_Graph_Viewer.exe\`;
   - \`elehant_history.csv\`.

Установщик не содержит и не копирует CSV-историю, поэтому файл существующей истории не должен заменяться установкой. Перед обновлением всё равно рекомендуется сделать резервную копию истории.

## Сборка вручную на Windows

1. Установи Python 3.14 x64 и Inno Setup 6.
2. Клонируй репозиторий и открой PowerShell в его корне.
3. Выполни:

\`\`\`powershell
.\scripts\build_exe.ps1
$python = ".\.venv\Scripts\python.exe"
& $python -m pip install matplotlib
& $python -m PyInstaller --onefile --windowed --clean --noconfirm --name Elehant_Graph_Viewer --distpath release --workpath build-graph tools\graph_viewer\elehant_graph_periods_manual.py
& "\${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" "/DAppVersion=1.0.0" "installer\Elehant_SVT15_Setup.iss"
\`\`\`

Готовый файл появится в \`release\Elehant_SVT15_Setup.exe\`.

## Безопасность и данные

- Не добавляй реальные \`elehant_history.csv\` в Git.
- Установщик требует прав администратора, чтобы установить программу в \`C:\Elehant\` и зарегистрировать задачу автозапуска.
- Удаление программы не должно удалять файл истории. Для дополнительной безопасности делай резервную копию перед обновлением.
