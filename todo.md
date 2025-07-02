- Add smoke Detector on foreign Alarm
    o Is Radio Module ID provided over several hops? ->Experiment!

- Mute-Endpoint (bei Devices?) zum Silencen aus Frontend

- Wenn WS Logger ausgeschaltet ist, darf Packet Analyzer nicht gehen.
        --> Auslesen und Hinweis auf Packet Analyzer Seite

- Tests
    o ...


- Alle RWM eintragen
- Packet Analyzer: Save Bug: Geht nur einmal
- Packet Analyzer: Andere Packettypen
    o Line Test Stop
        o Test/Aufzeichnung durchführen
- Packet Analyzer: Farben Discovery Request/Response anders
- Packet Analyzer: Unknown alarm line: auch rot

- Alarmlines: Sende-Timeout: 5s
- Alarmlines: Während Senden Status im FE anzeigen und UI sperren

Robustness
- Abbruch-Schleife für Warten auf Pin-States
- Heartbeat, der schaut ob verkrüppelte Packete empfangen wurden und den Empfangsbuffer löscht
- Heartbeat, der auf RX zurücksetzt, falls anderer State ist (und ohne dass Packete gesendet werden!)