.DEFAULT_GOAL := install
.PHONY : install

/opt/var/run/mosquitto.pid:
	touch /opt/var/run/mosquitto.pid
	chown mosquitto:mosquitto /opt/var/run/mosquitto.pid

/opt/var/log/mosquitto.log:
	touch /opt/var/log/mosquitto.log
	chown mosquitto:mosquitto /opt/var/log/mosquitto.log

install: /opt/var/log/mosquitto.log /opt/var/run/mosquitto.pid
	mkdir -p /opt/var/lib/mosquitto
	chown -R :mosquitto /opt/var/lib/mosquitto
	chown -R mosquitto /opt/var/lib/mosquitto
	mv /opt/etc/mosquitto/mosquitto.conf /opt/etc/mosquitto/mosquitto.conf.$(shell date "+%Y.%m.%d-%H.%M.%S")
	install mosquitto.conf /opt/etc/mosquitto/mosquitto.conf
