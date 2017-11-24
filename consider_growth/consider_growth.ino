/*
   message key:
   0: I am not sitting
   1: I am sitting
   2: You are not sitting
   3: You are sitting
   4: Ring the bell to begin
   5: Ring the bell to end

*/
#include <ArduinoJson.h>
#include <SPI.h>

#include <WiFi101.h>
#define PubNub_BASE_CLIENT WiFiClient
#include <PubNub.h>

//OCADU Wifi credentials below
static char ssid[] = "Chris's Wi-Fi Network 2.4G"; 
static char pass[] = "summer99";
//static char ssid[] = "ocadu-embedded";      //SSID of the wireless network
//static char pass[] = "internetofthings";    //password of that network
int status = WL_IDLE_STATUS;                // the Wifi radio's status


//Chris's pubnub account keys below
const static char pubkey[] = "pub-c-31315745-29a7-4863-bc2a-97a528df3c93";  //get this from your PUbNub account
const static char subkey[] = "sub-c-ac854de4-c885-11e7-9695-d62da049879f";  //get this from your PubNub account

//COuld have user1 publishing to channel1, and listening to channel 2, with user2 publishing to channel 2, listening to channel 1
const static char pubChannel[] = "channel1"; //choose a name for the channel to publish messages to
const static char subChannel[] = "channel1"; //choose a name for the channel to publish messages to

const int solenoid = 11;
const int el = 10;
const int cushionPin = 9;
const int NUM_RINGS = 3;
const String MY_NAME = "chrisMessage";
const String YOUR_NAME = "karoMessage";

const int I_AM_NOT_SITTING = 0;
const int I_AM_SITTING = 1;
const int YOU_ARE_NOT_SITTING = 2;
const int YOU_ARE_SITTING = 3;
const int BEGIN_SITTING = 4;
const int END_SITTING = 5;

int cushionVal = 1;  
int cushionPrev = HIGH;
int prevMsg = 0;

unsigned long lastRefresh = 0;
int publishRate = 1000;

int myMsg;
int yourMsg;

void setup()
{
  pinMode(cushionPin, INPUT_PULLUP);
  pinMode(solenoid, OUTPUT);
  pinMode(el, OUTPUT);
  Serial.begin(9600);
  connectToServer();
}

void loop()
{
  cushionVal = digitalRead(cushionPin);

  if ((cushionVal == LOW) && (cushionPrev == HIGH)) //if sitting, trigger the feed update with a button, uses both current and prev value to only change on the switch
  {
    myMsg = 1;
    publishToPubNub(myMsg);
    digitalWrite(el, LOW);
    Serial.print("My message: ");
    Serial.println(myMsg);
  } else if ((cushionVal == HIGH) && (cushionPrev == LOW))  {
    myMsg = 0;
    publishToPubNub(myMsg);
    Serial.print("My message: ");
    Serial.println(myMsg);
  }
  
  if (millis() - lastRefresh >= publishRate) //theTimer to trigger the reads
  {
    readFromPubNub();
    if (yourMsg != prevMsg) {
      switch (yourMsg) {
        case YOU_ARE_SITTING:
          if (cushionVal == HIGH) {
            digitalWrite(el, HIGH);
          } else {
            digitalWrite(el, LOW);
          }
          break;
        case YOU_ARE_NOT_SITTING:
          digitalWrite(el, LOW);
          break;
        case BEGIN_SITTING:  
          ding();
          break;
        case END_SITTING:
          ding();
          while(true); //do nothing forever
      }
      
    }

    Serial.print("Your message: ");
    Serial.println(yourMsg);
    
    lastRefresh = millis();
    prevMsg = yourMsg;
  }

  cushionPrev = cushionVal; //store the value of this cycle to compare next loop
}


void connectToServer()
{
  WiFi.setPins(8, 7, 4, 2); //This is specific to the feather M0

  status = WiFi.begin(ssid, pass);                    //attempt to connect to the network
  Serial.println("***Connecting to WiFi Network***");


  for (int trys = 1; trys <= 10; trys++)                 //use a loop to attempt the connection more than once
  {
    if ( status == WL_CONNECTED)                        //check to see if the connection was successful
    {
      Serial.print("Connected to ");
      Serial.println(ssid);

      PubNub.begin(pubkey, subkey);                      //connect to the PubNub Servers
      Serial.println("PubNub Connected");
      break;                                             //exit the connection loop
    }
    else
    {
      Serial.print("Could Not Connect - Attempt:");
      Serial.println(trys);

    }

    if (trys == 10)
    {
      Serial.println("I don't this this is going to work");
    }
    delay(1000);
  }
}


void publishToPubNub(int messageNum)
{
  WiFiClient *client;
  StaticJsonBuffer<800> messageBuffer;                    //create a memory buffer to hold a JSON Object
  JsonObject& pMessage = messageBuffer.createObject();    //create a new JSON object in that buffer

  ///the imporant bit where you feed in values
  pMessage[MY_NAME] = messageNum;                      //add a new property and give it a value

  //  pMessage.prettyPrintTo(Serial);   //uncomment this to see the messages in the serial monitor

  int mSize = pMessage.measureLength() + 1;                   //determine the size of the JSON Message
  char msg[mSize];                                            //create a char array to hold the message
  pMessage.printTo(msg, mSize);                              //convert the JSON object into simple text (needed for the PN Arduino client)

  client = PubNub.publish(pubChannel, msg);                      //publish the message to PubNub

  if (!client)                                                //error check the connection
  {
    Serial.println("client error");
    delay(1000);
    return;
  }

  if (PubNub.get_last_http_status_code_class() != PubNub::http_scc_success)  //check that it worked
  {
    Serial.print("Got HTTP status code error from PubNub, class: ");
    Serial.print(PubNub.get_last_http_status_code_class(), DEC);
  }

  while (client->available())
  {
    Serial.write(client->read());
  }
  client->stop();
  Serial.println("Successful Publish");
}


void readFromPubNub()
{
  StaticJsonBuffer<1200> inBuffer;                    //create a memory buffer to hold a JSON Object
  WiFiClient *sClient = PubNub.history(subChannel, 1);

  if (!sClient) {
    Serial.println("message read error");
    delay(1000);
    return;
  }

  while (sClient->connected())
  {
    while (sClient->connected() && !sClient->available()) ; // wait
    char c = sClient->read();
    JsonObject& sMessage = inBuffer.parse(*sClient);

    if (sMessage.success())
    {
      //    sMessage.prettyPrintTo(Serial); //uncomment to see the JSON message in the serial monitor
      yourMsg = sMessage[YOUR_NAME];  
    }
  }
  sClient->stop();
}

void ding() {
  for (int i = 0; i < NUM_RINGS; i++) {
    digitalWrite(solenoid, HIGH);
    delay(50);
    Serial.println("ding!");
    digitalWrite(solenoid, LOW);
    delay(2500);
  }
}

