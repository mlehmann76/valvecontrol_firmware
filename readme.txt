readme.txt
 Created on: 30.10.2018
     Author: marco

-- ota upload
python3 tools/upload.py "esp32_test" build/mqtt-valvecontrol.bin 

-- config upload
curl -X PUT http://192.168.178.45/config --header "Content-Type: application/json" -d @config.json
