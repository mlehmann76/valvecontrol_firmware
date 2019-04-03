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
        self.client.subscribe(self.device + "/state/ota/#")
        
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

    def sendUpdateMsg(self, md5, size):

        json_string = '{"firmware":{"update": "ota","md5": "' + md5.hexdigest() + '","len": ' + str(size) + '}}'
        
        data = json.loads(json_string)
        #print(data)
    
        ret = self.client.publish(self.device + "/system", json_string, qos=1, retain=False)
        while not ret.is_published():
            self.client.loop()
            
    def sendFirmware(self, filename):
        with open(filename, "rb") as binary_file:
            # Read the whole file at once
            data = binary_file.read()
        #data = bytearray(os.urandom(size))
        #print(data)
        buf = base64.b85encode(data, pad=True)
        #print(buf)
              
        if not (len(data) % 4) == 0:
            print("filelength should be multiple of 4")
    
        print()
    
        d = hashlib.md5()
        d.update(data)
        
        print("sending --- "+ str(len(buf)) + ", md5: " + d.hexdigest())
        
        self.sendUpdateMsg(d, len(data))
        
        ret = self.client.publish(self.device + "/ota/$implementation/binary", buf, qos=1, retain=False)
        while not ret.is_published():
            self.client.loop()
    
if __name__ == "__main__":
    
    parser = argparse.ArgumentParser(description='esp32 mqtt firmware upload tool')
    parser.add_argument('dev', help='device string')
    parser.add_argument('file', help='filename')
    args = parser.parse_args()

    #if not args.dev.endswith("/"):
    #    args.dev = args.dev + "/"
    dc = deviceControl(args.dev)
    dc.start()
    dc.sendFirmware(args.file)
    #dc.sendFirmware("/home/marco/git/valvecontrol/firmware/.gitignore")
   
