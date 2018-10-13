import paho.mqtt.client as mqtt
import base64
import os
from time import sleep

counter = 0
error = 0
run = 0
host_name = 'raspberrypi'
client_name = 'raspi_test'

client = mqtt.Client(client_name)
client.connect(host_name)
size = 64


def on_message_print(client, userdata, message):
    print("%s %s" % (message.topic, message.payload))
    pay_list = message.payload.split(b" ")
    if len(pay_list) > 0 :
        if int(pay_list[1]) == counter:
            print("counter " + str(counter))
            global run
            run = 0
        else:
            global error
            error = 1
            
client.on_message = on_message_print;
            
client.subscribe("esp32/0824/state/ota/#")

while (error == 0):
    buf = str(base64.a85encode(bytearray(os.urandom(size))))
    print("sending, counter " + str(counter))
    client.publish("esp32/0824/ota",'counter ' + str(counter) + " " + str((buf)), qos=1, retain=False)
    run = 1
    
    while(run == 1):
        client.loop(timeout=0.1, max_packets=1)
    
    counter = counter + 1
    
