# Station (Роутер)

Модель: Keenetic Giga (KN-1011)

## Секреты
```
STATION_SSID                   char*  Название WiFi сети 
STATION_PASSWORD               char*  Пароль WiFi сети
STATION_ADMIN_PASSWORD         char*  Пароль `admin` пользователя оболочки станции 
STATION_SERVER_ROOT_PASSWORD   char*  Пароль `root` пользователя сервера станции
STATION_SERVER_ADMIN_LOGIN     char*  Имя администратора сервера станции
STATION_SERVER_ADMIN_PASSWORD  char*  Пароль администратора сервера cтанции
STATION_SERVER_ADMIN_KEY       char*  Ключ администратора сервера станции
STATION_SERVER_SSH_PORT        char*  Порт SSH-сервера на сервере станции
```

## Настройка сервера станции 
### Настройка OPKG Entware
1. Установить компонент OPKG и поддержку файловой системы Ext следуя
  [официальной инструкции](https://help.keenetic.com/hc/ru/articles/360000948719-OPKG).
2. Отформатировать USB накопитель в Ext4.
  [Официальная инструкция](https://help.keenetic.com/hc/ru/articles/115005875145),
  [инструкция для MacOS](https://help.keenetic.com/hc/ru/articles/360021214160).
3. Установить системы пакетов репозитория Entware на USB-накопитель по
  [официальной инструкции](https://help.keenetic.com/hc/ru/articles/360021214160). 
4. В результате на станции будет развернут Entware, 
  к которому можно подключиться по SSH.
  Команда для подключения, если SSH сервер работает на 222 порту:
  ```shell
  $ ssh -p 222 root@<stationIP> 
  ```

### Настройка доступа к серверу по SSH
По умолчанию после 
[настройки OPKG Entware](#настройка-opkg-entware) на станции будет запущен сервис `/opt/etc/init.d/S51dropbearp`. 
Это легковесный SSH сервер 
[Dropbear](https://matt.ucc.asn.au/dropbear/dropbear.html)
Файл конфигурации `/opt/etc/config/dropbear.conf` по факту поддерживает только изменение порта.
Настройка сервера осуществляется путем добавления флагов к бинарнику. 
[Инструкции OpenWrt](https://openwrt.org/docs/guide-user/base-system/dropbear) тут не поможет, так
как у нас Entware.

1. Создать нового пользователя на станции. 
  ```shell
  $ adduser <username>
  ```

2. Скопировать публичный SSH ключ на станцию в файл 
  `/opt/home/<username>/.ssh/authorized_keys` по
  [инструкции](https://linuxhandbook.com/add-ssh-public-key-to-server/).
  Если ключа нет, то его следует создать следуя 
  [инструкции](https://docs.github.com/en/authentication/connecting-to-github-with-ssh/generating-a-new-ssh-key-and-adding-it-to-the-ssh-agent).

3. Запретить доступ по SSH от имени `root` пользователя.
  В файле `/opt/etc/init.d/S51dropbear` добавляем флаг `-w` в секцию `start`. 
  Получится
  ```
  $DROPBEAR -w -p $PORT -P $PIDFILE
  ```
4. Перезапустить SSH сервер 
  ```shell
  $ /opt/etc/init.d/S51dropbear restart
  ```
5. Изменить пароль пользователя `root` командой `passwd`.


### Установка GCC и прочего для нативной сборки
  [Инструкция](https://github.com/Entware/Entware/wiki/Using-GCC-for-native-compilation).


### Установка `wget-ssl`
По умолчанию установлен `wget`, который не может в SSL (`wget-nossl`). 
1. Удаляем текущий `wget`.
  ```shell
  $ opkg remove wget-nossl
  ```
2. Устанавливаем новый c поддержкой SSL.
  ```shell
  $ opkg install wget-ssl
  ```


### Установка текстового редактора VIM
Выполняем команду. 
```
$ opkg install vim-full vim-runtime vim-help
```


### Установка файлового менеджера VIFM
По аналогии с этим
[скриптом](https://github.com/kephircheek/rebecca/blob/master/deps/ubuntu/vifm/install.sh).
1. Скачиваем архив c искодниками, распоковываем и переходим в корневую дирректорию проекта.
  ```shell
  $ wget -c "https://github.com/vifm/vifm/releases/download/v0.12.1/vifm-0.12.1.tar.bz2"
  $ tar xf vifm-0.12.1.tar.bz2
  $ rm vifm-0.12.1.tar.bz2
  $ cd vifm-0.12.1
  ```
2. При установке не генерировался `data/vifm-help.txt`. 
  Поиски решения привели к
  [обсуждению](https://github.com/vifm/vifm/issues/397), 
  где посоветовали установить `groff`.
  ```shell
  $ opkg install groff
  ```
3. Собираем и устанавливаем 
  ```shell
  $ ./configure --prefix=/opt && make && make install
  ```
4. Копируем необходимые файлы согласно инструкции в файле `INSTALL`.
  ```shell
  $ cp data/vifm-help.txt ../.vifm/
  $ cp data/vifmrc ../.vifm/
  ```

### Установка и запуск MQTT брокера Mosquitto
[Иструкция](https://kotyara12.ru/pubs/iot/keenetic-mqtt/)
по установке Mosquitto.
1. Устанавливаем пакет без поддержки SSL.
  ```shell
  $ opkg install mosquitto-nossl
  ```
2. Загружаем (если еще нет) репозиторий `downton` 
  ```shell
  $ opkg install git git-http
  $ git clone https://github.com/kephircheek/downton.git
  ```
3. Переходим в директорию сервиса и устанавливаем
  ```shell
  $ cd downton/services/mosquitto && make
  ```
4. Запускам сервис 
  ```shell
  $ /opt/etc/init.d/S80mosquitto start
  ```
  Смотрим лог
  ```
  $ tail -f /opt/var/log/mosquitto.log
  ```

### Полезные ссылки
- [Инструкция](https://github.com/Entware/Entware/wiki/Self-installation-of-python-modules)
  по установке python пакетов.

### Известные проблемы
- **git-diff выводит специальные символы `[[1m`**  
  *Решение:* Нужно установить полноценный `less` командой
  ```shell
  opkg install less
  ```
