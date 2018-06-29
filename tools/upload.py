import paho.mqtt.client as mqtt
import base64
import os
from time import sleep

host_name = 'raspberrypi'
client_name = 'raspi_test'

client = mqtt.Client(client_name)
client.connect(host_name)
size = 1

while (1):
    buf = str(base64.a85encode(bytearray(os.urandom(size))))
    client.publish("esp32/0824/ota",'size ' + str(len(buf)) + str((buf)), qos=1, retain=False)
    sleep(0.1)
    size = size * 2

    if size >4096 :
        break
