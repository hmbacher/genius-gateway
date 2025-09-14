X Alarm von einem unbekannten Gerät und Add Unknown Device ist konfiguriert
    o Device wird hinzugefügt
    o /config und /state topic werden über MQTT verteilt
    o Alarm wird gesetzt
    o /state config wird aktualisiert
    o Alarm-Event geht an FE

X Alarm von einem unbekannten Gerät und Add Unknown Device ist NICHT konfiguiert 
    o Device wird NICHT hinzugefügt
    o /config und /state topic werden NICHT über MQTT verteilt
    o Alarm wird NICHT gesetzt
    o /state config wird NICHT aktualisiert
    o Alarm-Event geht NICHT an FE

- Alarm
    X Alarm
        o WS Event geht an FE
        --> Reload der devices
        --> Alarm-Status in Statusbar
        --> Alarming devices auf Startseite
    - Zusätzlicher Alarm
        o WS Event geht an FE
        --> Reload der devices
        --> Alarm-Status in Statusbar
        --> Alarming devices auf Startseite
    - Erster fällt wieder weg
        o s.o.
    - Zweiter fällt wieder weg
        o s.o.

- MQTT @ Alarm
    o General Alarm disabled
        --> kein MQTT-Topic
    o General Alarm enabled:
        --> MQTT-Topic wird aktualisiert
    o HA-MQTT disabled
        --> Weder /config-, noch /state-Topic wird aktualisiert
    o HA-MQTT enabled
        --> /state-Topic wird aktualsiert
        --> /config-Topic wird nicht aktualisiert

- MQTT allg.
    o HA-MQTT disabled
        --> keine Topics bei Start
        --> keine Topics bei devices-Änderung
    o HA-MQTT enabled
        --> /config-Topic bei Start
        --> /state-Topic bei Start
        --> /config-Topic und /state topic bei Device-Änderungen