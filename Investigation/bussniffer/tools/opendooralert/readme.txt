#start cron editor
crontab -e

#and add the line:
@reboot /usr/bin/python3 /home/pi/OpenDoorAlert/opendooralert.py

Save and close, and then run
update-rc.d cron defaults