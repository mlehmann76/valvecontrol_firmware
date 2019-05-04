import paho.mqtt.client as mqtt
import base64
import os
import time
import json
import hashlib
import argparse

class deviceControl():
    def __init__(self,device):
        self.device = device
        
    def start(self):
        host_name = 'raspberrypi'
        client_name = 'raspi_test'
        
        self.client = mqtt.Client(client_name)
        self.client.on_connect = self.onConnect
        self.client.on_message = self.on_message_print;

        self.client.connect(host_name)                    
    
    def onConnect(self, client , obj, flags, rc):
        self.client.subscribe(self.device + "/#")
        
    def on_message_print(self, client, userdata, message):
        print("%s %s" % (message.topic, message.payload))  
    
    def sendChanControl(self, chanNum = 0, chanVal=-1):
        json_string = """{"channel":{"num": """ + str(chanNum) + ""","val": """ + str(chanVal) + """}}"""
        data = json.loads(json_string)
        print(data)
        ret = self.client.publish(self.device + "/control", json_string, qos=1, retain=False)
        #while(run == 1):
        while not ret.is_published():
            self.client.loop()
    
    def sendConfig(self, jsondata):
        print(jsondata)
        data = json.loads(jsondata)
        print(data)
        ret = self.client.publish(self.device + "/config", jsondata, qos=1, retain=False)
        #while(run == 1):
        while not ret.is_published():
            self.client.loop()
    
if __name__ == "__main__":
    
    parser = argparse.ArgumentParser(description='esp32 mqtt message tool')
    parser.add_argument('dev', help='device string')
    parser.add_argument('data', help='json string to send')
    args = parser.parse_args()

    #if not args.dev.endswith("/"):
    #    args.dev = args.dev + "/"
    dc = deviceControl(args.dev)
    dc.start()
    dc.sendConfig(args.data)
    #dc.sendFirmware("/home/marco/git/valvecontrol/firmware/.gitignore")
   
