
//////////////////////////
// wifi
//////////////////////////
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
const char* host= "zumo";
const char* ssid = "Tux24";
const char* password = "a47a47a47a";
const char* apssid = "zumo";
const char* appassword = "a47a47a47a";
const int port = 9876; // and this port
MDNSResponder mdns;
ESP8266WebServer webserver(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiServer server(port);
WiFiClient client;

//////////////////////////
// servo
//////////////////////////
#include <Servo.h>
#define SERVO_PIN D0
Servo myservo;

//////////////////////////
// PIEZO output pin
//////////////////////////
#define PIEZO D4

//////////////////////////
// roboremo variables
//////////////////////////
unsigned int drivespeed=255;
int thisspeed=0;
int thisdir=0;
boolean connExists=false;
unsigned long reqid=0;
unsigned int ms_fr, ms_fl, ms_rl, ms_rr;
char drive[3];
int speed=0;

////////////////////////////////////////////////////////////////
// roboremo
////////////////////////////////////////////////////////////////
const int chCount = 4; // 4 channels, you can add more if you have GPIOs :)
int chVal[] = {1500, 1500, 1500, 1500}; // default value (middle)

int usMin = 0; // min pulse width
int usMax = 255; // max pulse width

char cmd[100]; // stores the command chars received from RoboRemo
int cmdIndex;
unsigned long lastHeartbeatTime=0;
unsigned long aliveSentTime=0;

unsigned int loopcnt=0;

boolean cmdStartsWith(const char *st) { // checks if cmd starts with st
  for(int i=0; ; i++) {
    if(st[i]==0) return true;
    if(cmd[i]==0) return false;
    if(cmd[i]!=st[i]) return false;;
  }
  return false;
}

//////////////////////////
// Zumo
//////////////////////////
#include <ZumoMotorsWemos.h>
#define MAX_SPEED             400 // max motor speed

#define LED_PIN LED_BUILTIN

ZumoMotorsWemos motors;

/*
 * This example uses the ZumoMotors library to drive each motor on the Zumo
 * forward, then backward. The yellow user LED is on when a motor should be
 * running forward and off when a motor should be running backward. If a
 * motor on your Zumo has been flipped, you can correct its direction by
 * uncommenting the call to flipLeftMotor() or flipRightMotor() in the setup()
 * function.
 */

/*
 * fDrive - set drive motors
 * m1/m2/m3/m4 = speed. if <0 then backward, if >0 then forward, if =0 then stop
 */
void fDrive(int s1, int s2){
	if (s1>255) s1=255;
	if (s2>255) s2=255;

	if (s1<-255) s1=-255;
	if (s2<-255) s2=-255;

	s1=s1*3;
	s2=s2*3;

	motors.setLeftSpeed(s1);
	motors.setRightSpeed(s2);
}

void exeCmd() { // executes the command from cmd
  if(cmdStartsWith("ch") ) {
    int ch = cmd[2] - '0';
    if(ch>=0 && ch<=9 && cmd[3]==' ') {
      chVal[ch] = (int)atof(cmd+4);
			if (ch==0){
				thisspeed=chVal[ch];

				if (thisspeed>10){
					// north
					fDrive(thisspeed, thisspeed);
				} else if (thisspeed<-10){
					// south
					fDrive(thisspeed, thisspeed);
				} else {
					// stop
					fDrive(0, 0);
				}
			} else if (ch==1){
				thisdir=chVal[ch];

				if (chVal[ch]>10){
					// east
					fDrive(thisdir, -thisdir);
				} else if (chVal[ch]<-10) {
					// west
					fDrive(thisdir, -thisdir);
				} else {
					fDrive(0, 0);
				}
			} else if (ch==2){
				// servo channel for missile launcher angle
				thisdir=chVal[ch];

				if (chVal[ch]>=0 && chVal[ch]<180){
					myservo.write(chVal[ch]);
				}
			}
    }
  } else if (cmdStartsWith("hb")) {
		// heartbeat received
  	lastHeartbeatTime=millis();
	} else if( cmdStartsWith("ca") ) {
		// use accelerometer:
		// example: set RoboRemo acc y id to "ca1"
  
    int ch = cmd[2] - '0';
    if(ch>=0 && ch<=9 && cmd[3]==' ') {
      chVal[ch] = (usMax+usMin)/2 + (int)( atof(cmd+4)*51 ); // 9.8*51 = 500 => 1000 .. 2000
    }
  }
}

/*
 * handleRoot
 *
 * create and transmit the control GUI
 */
void handleRoot() {
	webserver.send(200, "text/html", "<head></head><body style=\"font-size:24px;\">OK</body></html>");
}

/*
 * handleNotFound
 *
 * tell the browser that the page is not found
 */
void handleNotFound(){
	webserver.send(404, "text/plain", "File not found");
}

void beep(int pitch, int duration, int times) {
  for (int t=0; t<times; t++){
    for (int i = 0; i < duration; i++) {
      digitalWrite(PIEZO, HIGH);
      delayMicroseconds(pitch);
      digitalWrite(PIEZO, LOW);
      delayMicroseconds(pitch);
    }
    delay(100);
  }
}

void setup() {
  Serial.begin(115200);

  //////////////////////////////////////////////
  // setup servo
  //////////////////////////////////////////////
	myservo.attach(SERVO_PIN);
	myservo.write(90);
	delay(800);
	myservo.write(50);
	delay(800);
	myservo.write(130);
	delay(800);
	myservo.write(50);

	Serial.println("setup started...");
  //////////////////////////////////////////////
  // setup piezo speaker
  //////////////////////////////////////////////
  pinMode(PIEZO, OUTPUT);

	pinMode(LED_PIN, OUTPUT);

	//////////////////////////////////////////////
	// drive motors
	//////////////////////////////////////////////
	motors.setLeftSpeed(0);
	motors.setRightSpeed(0);

  beep(150, 100, 1);
  //button.waitForButton();
  
	// setup wifi as both a client and as an AP
	WiFi.mode(WIFI_AP_STA);

	// start the access point
	WiFi.softAP(apssid, appassword);

	// start the client
	WiFi.begin(ssid, password);

	Serial.println("");

	unsigned int conn_tries=0;
	boolean wifi_client_gaveup=false;
	while (WiFi.status() != WL_CONNECTED) {
		conn_tries++;
		delay(500);
		Serial.print(".");
		if (conn_tries>20) {
			// give up, just be an AP via softAP
			wifi_client_gaveup=true;
			beep(200, 750, 1);
			break;
		}
	}

	if (wifi_client_gaveup==false){
		Serial.print("Connected to ssid: ");
		Serial.println(ssid);
		Serial.print("IP address: ");
		Serial.println(WiFi.localIP());

		// register hostname on network
		if (mdns.begin(host, WiFi.localIP())) {
			Serial.print("Hostname: ");
			Serial.println(host);
		}

		beep(150, 100, 3);
	}

	Serial.print("AP ssid: ");
	Serial.println(apssid);
	Serial.print("AP IP address: ");
	Serial.println(WiFi.softAPIP());

	httpUpdater.setup(&webserver);

	// establish http bootloader updater
	webserver.on("/", handleRoot);
	webserver.onNotFound(handleNotFound);
	
	webserver.begin();
	server.begin();
	Serial.println("HTTP server started");

  cmdIndex = 0;
	beep(150, 500, 3);
}

void loop() {
	webserver.handleClient();
	loopcnt++;

  // if contact lost for more than half second
  if(millis()-lastHeartbeatTime>2000) {  
		// contact lost, stop vehicle
		connExists=false;
		fDrive(0, 0);
		if (connExists==true){
			beep(300, 500, 1);
		}

		// clear out channel values to their defaults
		chVal[0]=1500;
		chVal[2]=1500;
		chVal[3]=1500;
		chVal[3]=1500;
  }

  if(!client.connected()) {
    client = server.available();
		//Serial.println("no client connected: "+(String)loopcnt);
    return;
  }

  // here we have a connected client
  if(client.available()) {
		if (connExists==false){
			connExists=true;
			beep(150, 150, 1);
		}

    char c = (char)client.read(); // read char from client (RoboRemo app)

    if(c=='\n') { // if it is command ending
      cmd[cmdIndex] = 0;
      exeCmd();  // execute the command
      cmdIndex = 0; // reset the cmdIndex
    } else {      
      cmd[cmdIndex] = c; // add to the cmd buffer
      if(cmdIndex<99) cmdIndex++;
    }
	}

  if(millis() - aliveSentTime > 500) { // every 500ms
    client.write("alive 1\n");
    // send the alive signal, so the "connected" LED in RoboRemo will stay ON
    // (the LED must have the id set to "alive")
    
    aliveSentTime = millis();
    // if the connection is lost, the RoboRemo will not receive the alive signal anymore,
    // and the LED will turn off (because it has the "on timeout" set to 700 (ms) )
  }
}
