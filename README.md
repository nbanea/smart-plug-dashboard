# Smart Plug Dashboard
Dies ist ein Projekt, das während des Kurses "Internet Based Systems" an der Hochschule Aalen durchgeführt wurde. Es listet alle mit dem MQTT-Broker verbundenen Shelly Smart Plug S auf und zeigt sie auf der Seite an. Bitte beachten Sie, dass kein CSS verwendet wurde, da die Anweisungen des Dozenten dagegen sprachen.

## Installation
Die Installation des Projekts wird in vier Schritten durchgeführt. Das Projekt wurde auf Ubuntu 22.04 LTS getestet. Um das Projekt erfolgreich zu installieren, sind folgende Anforderungen nötig:
- Ubuntu 22.04 oder neuer
- Mosquitto Version 2.0 oder höher (Getestet: 2.0.11)
- Mosquitto-Include-Dateien für das Kompilieren des Mosquitto-Plugins (mosquitto.h, mosquitto\_plugin.h und mosquitto\_broker.h)
- Ein Browser oder Webserver, wie z.B. nginx, um die HTML-Datei zu laden

### Ubuntu
Für Ubuntu 22.04 oder neuer sind folgende Pakete erförderlich:
- mosquitto
- gcc
- sqlite3
- libsqlite3-dev
- libmosquitto-dev

Ebenfalls sind die Mosquitto-Include-Dateien erforderlich, da die Dateien von Ubuntu veraltet sind.

## Installationsschritte
### Mosquitto-Plugin
Um das Plugin benutzen zu können, ist eine Kompilierung nötig. Bevor das Plugin kompiliert wird, sollte der Pfad zur SQLite-Datenbank in der Datei *mosquitto-plugin.c* in Zeile 188 angepasst werden. Folglich kann das Plugin mit folgenden Befehl kompiliert werden:

`gcc -Wall  -shared -fPIC mosquitto-plugin.c -o mosquitto-plugin.so -lmosquitto -lsqlite3 -I path/to/include`

Diesen Befehl sowie weitere Dokumentation finden Sie im Plugin-Code.
### Mosquitto
Mosquitto unterstützt Plugins ab der Version 2.0. Nachdem Schritt [Mosquitto-Plugin](#mosquitto-plugin) erfolgreich abgeschlossen wurde, muss die Konfigurationsdatei (mosquitto-smart-plugs.conf) mit dem Plugin-Pfad aktualisiert werden.

Damit das Logging-Plugin im Einsatz kommen kann, muss Mosquitto mit der mitgelieferten, aktualisierten Konfigurationsdatei mit folgendem Befehl gestartet werden:

`mosquitto -c <Projektpfad>/mosquitto-smart-plugs.conf`

Somit wird sichergestellt, dass der Mosquitto-Broker auf Port 1883 einen MQTT-Listener und auf Port 1083 einen MQTT-over-Websockets-Listener startet und das Plugin lädt. Zusätzlich muss sichergestellt werden, dass der User, welcher den Mosquitto-Broker startet, ausreichende Zugangsrechte auf die SQLite-Datenbank-Datei hat, um Zugriffsprobleme zu vermeiden.

## HTML und JS
Um die Funktion der HTML- und JavaScript-Dateien zu gewährleisten ist es erforderlich, dass beide Dateien im gleichen Ordner sind. Ebenfalls sollte die IP-Adresse des Mosquitto-Brokers in der JavaScript-Datei (smartPlugs.js) in Zeile 17 angepasst werden.

## Shelly Plug S
Jedes verwendete Shelly Plug S muss die aktuelle Firmware besitzen. Ebenfalls muss in das Shelly-Dashboard die IP-Adresse des Mosquitto-Brokers angegeben werden.

## Testen
Falls alle Schritte erfolgreich abgeschlossen wurden, sollte beim Aufrufen der Seite das Dashboard erscheinen. Falls ein Shelly-Plug erkannt wird, dann wird automatisch ein kleines Feld mit ID und Informationen über dieses Shelly-Plug erstellt.
