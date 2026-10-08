Set WshShell = CreateObject("WScript.Shell")

WshShell.Run _
    """C:\Elehant\.venv\Scripts\python.exe"" ""C:\Elehant\src\elehant_monitor.py""", _
    0, _
    False
