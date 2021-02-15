import requests
import time
from pushnotifier import PushNotifier as pn

# replace with your device ip
url="http://192.168.178.35/status"

#create account on https://pushnotifier.de/notifications
api_key = "add your api key"
username = 'add your username'
password = 'add your password'
package = 'add your package'

alerttime = 15 # alle 15 Minuten 
intervall = 5  # alle 5 Minuten

openstarttime = -1;

def sendAlert(message):
    session = pn.PushNotifier(username, password, package, api_key)
    session.send_text(message, silent=False, devices=session.get_all_devices())

def millis():
    return round(time.time() * 1000)
    
def handleOpenDoor():
    global openstarttime
    if openstarttime == -1:
        openstarttime = millis()
        
    if openstarttime+ alerttime*60*1000 < millis():
        sendAlert('Deine Garage ist offen!')
        openstarttime = millis()
    
def handleCloseDoor():
    global openstarttime
    if openstarttime > -1:
        sendAlert('Deine Garage wurde geschlossen')
    openstarttime = -1;
    

while True:
    print("get status...")
    try:
        r = requests.get(url)
        if r.status_code < 400:
            status = r.json()
            if status["valid"]:
                if status["doorstate"] != 0x40:
                    handleOpenDoor()
                else:
                    handleCloseDoor()
            else:
                print("Status is invalid")
        else:
            print("invalid response code "+ r.status_code)
    except requests.exceptions.RequestException as e:
        print(e)
    time.sleep(intervall*60)