# drybot
Drybot is a cheap (around $5-$10) water sensor to alert you
via push message when a sensor detects water.
Common problem spots like under the washer or in the mechanical/furnace room are good spots 
where you should have a water sensor.

# Alerting
Alerts are sent by pushover.net which is cost effective (one time charge per device install) and can be configured to
break through do not disturb on iphone (and probably android).

There is some support for hitting a flask app (or any endpoint) to do custom alerting and notification.

# Install
- Copy config.h.sample to config.h
- Set wifi and other details in config.h

# demo
[Youtube Demo](https://youtu.be/TYNeofcWZgk)

# hardware
![sensor pic](./img/sensor.png)
![controller pic](./img/controller.png)
