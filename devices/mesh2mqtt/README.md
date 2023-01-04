# mesh2mqtt

Шлюз соединяет две сети WiFi станции и WiFi mesh сеть умных устройств.
Шлюз позволяет передавать сообщения в обе стороны между сетями. 
Шлюз является корневым устройством в mesh сети. 
MESH сеть обеспечивается библиотекой
[painlessmess](https://gitlab.com/painlessMesh/painlessMesh).
*В прошивках всех остальных устройств следует указывать,
что в сети есть корневой узел*
```
mesh.setContainsRoot(true);
```
Основным (и единственным) приемником сообщений в сети станции является MQTT брокер. 
Спецификация сообщения из mesh сети умных устройств для успешной передачи в MQTT брокер:
```
{"topic": "...", "payload": "..."}
```

## Секреты
```
MESH_SSID              char*     Имя mesh сети устройств.
MESH_PASSWORD          char*     Пароль mesh сети устройств.
MESH_PORT              int       Порт mesh сети устройств, например 5555.
MESH_PAYLOAD_MAX_SIZE  int       Максимальный размер сообщения, например 512.
MESH_ROOT_MAC          uint8_t*  Виртуальный MAC адрес шлюза (не обязан совпадать с реальным)
```
