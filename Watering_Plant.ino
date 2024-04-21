#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Update these with values suitable for your network.

const char* ssid = "Boll-2.4G";
const char* password = "0878155687";
const char* mqtt_server = "broker.hivemq.com";


int ValvePin1 = D4;
int ValvePin2 = D3;
int MotorPin3 = D1;
int MotorPin4 = D2;

int MoistPin = A0;
int val = 0;

bool Activation = false;
bool valve_state = false;
bool pump_state = false;

#define Start "Start"
#define Time "Time"

WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, "time.nist.gov", 7 * 3600, 60000);
unsigned long unix_epoch;
char dateMsg[64];
char timeMsg[64];

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

unsigned long lastPumpActivation = 0; 
unsigned long lastValveActivation = 0;

void showRTC() {
  char dow_matrix[7][10] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
  byte x_pos[7] = {29, 29, 23, 11, 17, 29, 17};
  static byte previous_dow = 0;

  byte current_dow = weekday(unix_epoch);

  if ( previous_dow != weekday(unix_epoch) )   {
    previous_dow = weekday(unix_epoch);
  }

  sprintf( dateMsg, "%02u-%02u-%04u", day(unix_epoch), month(unix_epoch), year(unix_epoch) );
  sprintf( timeMsg, "%02u:%02u:%02u", hour(unix_epoch), minute(unix_epoch), second(unix_epoch) );

  // Serial.println(dateMsg);
  // Serial.println(timeMsg);
  // Serial.println(dow_matrix[current_dow - 1]);

  client.publish("current_date", dateMsg);
  client.publish("current_time", timeMsg);
  client.publish("Day", dow_matrix[current_dow - 1]);
}

void Open_pump()
{
  digitalWrite(MotorPin3,LOW);
  digitalWrite(MotorPin4,HIGH);
  lastPumpActivation = unix_epoch;
  valve_state = true;
}

void Close_pump()
{
  digitalWrite(MotorPin3,LOW);
  digitalWrite(MotorPin4,LOW);
  pump_state = false;
}

void Open_valve()
{
  digitalWrite(ValvePin1,HIGH);
  digitalWrite(ValvePin2,LOW);
  lastValveActivation = unix_epoch;
  valve_state = true;
}

void Close_valve()
{
  digitalWrite(ValvePin1,LOW);
  digitalWrite(ValvePin2,LOW);
  valve_state = false;
}

void Activate_Waterplant_Sys(){
  if (Activation == true) {
    if (hour(unix_epoch) == 13 && minute(unix_epoch) == 33 && second(unix_epoch) < 55) {
        Open_pump();
        Open_valve();
        Serial.println("active pump and valve");
    }
    else if (hour(unix_epoch) == 0 && minute(unix_epoch) == 0 && second(unix_epoch) < 5) {
        Close_pump();
        Close_valve();
        Activation = false;
    }
  }
  else{
    Close_pump();
    Close_valve();
    Serial.println("Pump not active");
  }
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    if (strcmp(topic, Start) == 0) {
        if ((char)payload[0] == '1') {
            Activation = true;
        } else if ((char)payload[0] == '0') {
            Activation = false;
        }
    }

    // if (strcmp(topic, Pump) == 0) {
    //     if ((char)payload[0] == '1') {
    //         Open_pump();
    //     } else if ((char)payload[0] == '0') {
    //         Close_pump();
    //     }
    // }

    // if (strcmp(topic, Valve) == 0) {
    //     if ((char)payload[0] == '1') {
    //         Open_valve();
    //     } else if ((char)payload[0] == '0') {
    //         Close_valve();
    //     }
    // }
    Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("ballsayHello", "hello world");
      // ... and resubscribe
      client.subscribe(Start);
      // client.subscribe(Pump);
      // client.subscribe(Valve);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  pinMode(MotorPin3, OUTPUT);
  pinMode(MotorPin4, OUTPUT);
  pinMode(ValvePin1, OUTPUT);
  pinMode(ValvePin2, OUTPUT);

  while ( WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(".");
  }
  timeClient.begin();
}

void loop() {
  val = analogRead(MoistPin);
  timeClient.update();
  unix_epoch = timeClient.getEpochTime();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;
    snprintf (msg, MSG_BUFFER_SIZE, "", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("ballsayHello", msg);
  }

  if (Activation == true) {
    Activate_Waterplant_Sys();
    Serial.println("active = true");
  } else {
    Close_pump();
    Close_valve();
  }

  
  client.publish("Pump_State", String(pump_state).c_str());
  client.publish("Valve_State", String(valve_state).c_str());

  client.publish("Soil_Moisture_value", String(val).c_str());

  showRTC();

  client.publish("pump_activation_time", String(lastPumpActivation).c_str());
  client.publish("valve_activation_time", String(lastValveActivation).c_str());
}