# TODO
x - Packet-Zähler bei jedem zu sendenden Paket erhöhen
x - Sync Enums btw. Backend and Frontend

- Mute-Endpoint (bei Devices?) zum Silencen aus Frontend
    o Konzept überlegen

- Alarmlines: Sende-Timeout: 5s
- Alarmlines: Während Senden Status im FE anzeigen und UI sperren

- Robustness
    - Abbruch-Schleife für Warten auf Pin-States
    - Heartbeat, der schaut ob verkrüppelte Packete empfangen wurden und den Empfangsbuffer löscht
    - Heartbeat, der auf RX zurücksetzt, falls anderer State ist (und ohne dass Packete gesendet werden!)

- ToDos in Kommentaren suchen

# BUGS
- WebSocket Event wird bei jedem einzelnen Alarm-Packet an Frontend gesendet? Jedes Event lädt die Devices im FE neu. Vielleicht sollte nur ein Event gesendet werden, wenn ein neuer zusätzlicher Melder Feuer meldet?

# TESTS
siehe /tests/tests.md

# OFFEN
- Verhalten von Edit-Devices-Dialog bei Devices, die aus einem Alarm hinzugefügt wurden...