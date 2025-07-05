x - Genius devices als Store? (Startseite, AlarmStatus, PacketAnalyzer, Devices)

x - Packet-Zähler bei jedem zu sendenden Paket erhöhen

x - Sync Enums btw. Backend and Frontend

- Mute-Endpoint (bei Devices?) zum Silencen aus Frontend

- Alarmlines: Sende-Timeout: 5s
- Alarmlines: Während Senden Status im FE anzeigen und UI sperren

- Robustness
    - Abbruch-Schleife für Warten auf Pin-States
    - Heartbeat, der schaut ob verkrüppelte Packete empfangen wurden und den Empfangsbuffer löscht
    - Heartbeat, der auf RX zurücksetzt, falls anderer State ist (und ohne dass Packete gesendet werden!)

BUGS

TEST
- Alarm von einem unbekannten Gerät und Add Unknown Device ist konfiguriert
    o Device wird hinzugefügt
    o /config und /state topic werden über MQTT verteilt
    o Alarm wird gesetzt
    o /state config wird aktualisiert
    o Alarm-Event geht an FE

- Alarm von einem unbekannten Gerät und Add Unknown Device ist NICHT konfiguiert 
    o Device wird NICHT hinzugefügt
    o /config und /state topic werden NICHT über MQTT verteilt
    o Alarm wird NICHT gesetzt
    o /state config wird NICHT aktualisiert
    o Alarm-Event geht NICHT an FE

OFFEN
- Verhalten von Edit-Devices-Dialog bei Devices, die aus einem Alarm hinzugefügt wurden...