
controller:

	gcc -Wall controller.c -o ./bin/controller -lpthread -lm
	arm-linux-gnueabi-gcc controller.c -o ./bin/controller_arm -lpthread -lm
	# scp ./bin/controller_arm  root@192.168.7.2:/home/root

thermostat:

	gcc -Wall thermostat.c -o ./bin/thermostat
	arm-linux-gnueabi-gcc thermostat.c -o ./bin/thermostat_arm
	# scp ./bin/thermostat_arm  root@192.168.7.2:/home/root

sensor:

	gcc -Wall sensor.c -o ./bin/sensor
	arm-linux-gnueabi-gcc sensor.c -o ./bin/sensor_arm
	# scp ./bin/sensor_arm  root@192.168.7.2:/home/root

monitor:

	gcc -Wall monitor.c -o ./bin/monitor
	arm-linux-gnueabi-gcc monitor.c -o ./bin/monitor_arm
	# scp ./bin/sensor_arm  root@192.168.7.2:/home/root



