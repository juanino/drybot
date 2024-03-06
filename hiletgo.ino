/*
 *  Using hiletgo esp8266
 *  More info at https://www.amazon.com/gp/product/B081CSJV2V/
 */

#include <ESP8266WiFi.h>
#include "config.h"

#include "Pushover.h"

/* Adjust your secrets in config.h 
   ssdid, password, hostname or ip */

// fixed items
const char* devicename = "washing machine";
const int water1 = 16; // water sensor plate on pin D0
const int water2 = 4; // second water sensor on pin D2
const int floodLed = 5; // LED on pin D1
const int runningLed = 2; // onboard BLUE led used to visually confirm the program is running
const int max_alerts = 4; // number of alerts before we trip a breaker and stop alerting
const unsigned long max_wait = 900000; // number of miliseconds before the breaker is reset

// variables for sensors
int water1State = 0; // var for reading plate sensor #1
int water2State = 0; //second sensor
int prior_w1state = 0; // placeholder for comparing state changes for sensor #1
int prior_w2state = 0; // second sensor
// vars for timers and logic settings
int flash_counter = 0; // how count through loop() for flashing the led
int alerts = 0; // how many alerts have been sent
int silent = 0; // 0 = not silent , 1 = silent mode
unsigned long breaker_hit_ms = 0; // store miliseconds uptime when we hit circuit breaker

// push message service setup
Pushover po = Pushover(app_key, user_key, UNSAFE);

void setup() {

  // Water sensor setup 
  // initialize the LED pin as an output:
  pinMode(floodLed, OUTPUT);
  digitalWrite(floodLed, LOW);
  flash_flood(3); // test the flood light on startup
  // init water plate as a input button
  pinMode(water1, INPUT);
  pinMode(water2, INPUT);
  
  Serial.begin(115200);
  delay(100);

  // We start by connecting to a WiFi network
  pinMode(runningLed, OUTPUT);
  digitalWrite(runningLed, LOW);
  setup_wifi();
  digitalWrite(runningLed, HIGH);
  String msg = "sensor bootup " + String(devicename);
  send_po(msg,1);
}


// main loop
void loop() {
  /* check sensor
   * call send_po for push msg
   * call sendsensor flask post and SMS 
   */
     
  /* check each sensor */
  check_water(water1, &prior_w1state, &water1State);
  // check_water(water2, &prior_w2state, &water2State);
   
  /* intermittently flash the blue led so we know it's still working */
  flash(); 

  /* check if too many alerts */
  if (circuit_breaker() == 1) {
    int prior_silent_state = silent;
    silent = 1; // if circuit_breaker() is tripped we go silent for a while
    Serial.println("---------------------");
    Serial.print(String(prior_silent_state) + " " + String(silent));
    if (prior_silent_state != silent) {
      // state change to break on occured
      String msg = "breaker" + String(silent);
      send_po(msg,2);
      delay(1000);
    }
  }
} // end main loop



// Subroutines below here
// ----------------------

void check_water(int sensorpin, int* prior_state, int* waterXstate ) {
  Serial.println("Checking plate sensor on " + String(sensorpin));
  *prior_state = *waterXstate;
  *waterXstate = digitalRead(sensorpin); // read water sensor #1
  if (*prior_state == *waterXstate) {
    Serial.println("Same state as before:" + String(*waterXstate));
    return;
  }
  // check if wet, because plate would be high #dank
  if (*waterXstate == HIGH) {
    //turn LED on:
    digitalWrite(floodLed, HIGH); // not sure why LOW is on for feather pin #0, HIGH for all else
    Serial.println("plate wet: light should go ON");
    delay(500);
    String wet_msg = String(sensorpin) + "wet";
    send_po(wet_msg,2);
    delay(500);
    // sendsensor(1,sensorpin); // call out to flask 0=dry 1=wet
  } else {
    // turn LED off:
    digitalWrite(floodLed, LOW);
    Serial.println("plate dry: light should go OFF");
    // sendsensor(0,sensorpin); // call out to flask 0=dry 1=wet
    String dry_msg = String(sensorpin) + "dry";
    send_po(dry_msg,2);
    delay(500);
  }
}


int circuit_breaker() {
  unsigned long current_ms = millis();
  Serial.print("current_ms:");
  Serial.println(current_ms);
  Serial.print("breaker_hit_ms:");
  Serial.print(breaker_hit_ms);
  Serial.println();
  if (current_ms - breaker_hit_ms >= max_wait) {
    Serial.println("----------->resetting the circuit breaker.");
    delay(1000);
    int prior_silent = silent;
    silent = 0;
    if (prior_silent != silent) {
      String msg = "breaker0";
      send_po(msg,2);
    }
    alerts = 0;
    breaker_hit_ms = current_ms;
    return(0);
  }
  Serial.println("You have sent: " + String(alerts) + " alerts.");
  if (alerts > max_alerts) {
     Serial.println("Alerts hit breaker max" + String(max_alerts) + " going silent for " + String(max_wait) + "ms");
     if (silent == 0) {
        breaker_hit_ms = millis(); // record uptime when we hit the circuit breaker
     }
     return(1);
  } else {
    return(0);
  }
} //end breaker

void flash() {
  /* alternate the blue light so we know we haven't crashed
   * this is better than a delay since we don't miss a sensor check
   * last if block should be the sum of the two other if blocks to make
   * even blink of on/off
  */
  if (flash_counter < 200) {
    digitalWrite(runningLed, HIGH);
  } 
  if (flash_counter > 200) {
    digitalWrite(runningLed, LOW);
  } 
  flash_counter++;
  if (flash_counter == 400) {
    flash_counter = 0;
  }
} // end flash routine

void setup_wifi() {
  // connect to SSID from config.h
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.hostname("ESP-host");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void sendsensor(int status, int sensorpin) {
  if (silent == 1) {
    Serial.println("skipping flask send due to circuit breaker");
    return;
  }
  Serial.print("you sent in a status of: " + String(status));
  delay(1000);
  Serial.print("sendsensor: connecting to ");
  Serial.println(host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 5000;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  // We now create a URI for the request
  String url = "/post/";
  Serial.print("Requesting URL: ");
  Serial.println(url);
  
  // This will send the request to the server
  client.print(String("GET ") + url + devicename + "/" + sensorpin + "/" + String(status) + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");

  delay(500);
  // Read all the lines of the reply from server and print them to Serial
  while(client.available()){
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
  
  Serial.println();
  Serial.println("closing connection on SENDSENSOR------------");
}

void flash_flood(int beeps) {
  // small loop to flash the flood light
  // this signals bootup or other events
  int i;
  for(i=0; i<beeps; i++) {
    delay(100);
    digitalWrite(floodLed, LOW);
    delay(100);
    digitalWrite(floodLed, HIGH);
    delay(100);
    digitalWrite(floodLed, LOW);
  }
}

void send_po(String msg, int sound) {
  String msg2 = msg + String(devicename);
  po.setDevice("iphone");
  po.setPriority(2);
  po.setRetry(60);
  po.setExpire(900);
  if (sound == 1) {
    po.setSound("siren");  
  } 
  else if (sound == 2) {
    po.setSound("alien");
  } else {
    po.setSound("alien");
  }
  po.setMessage(msg);
  Serial.println(po.send()); //should return 1 on success
  }
