    /*******************************************************           
    * Consider Growth -- Accountability and Community for  *
    * nurturing new habits; meditation, music, business.   *
    *                                                      *
    *                                                      *
    * DIGF-6037-001 Creation & Computation                 *                                       
    *                                                      *  
    * Assignment 4 Nov 24, 2017                            *
    *                                                      * 
    * Karo, Dave, Chris                                    *   
    *                                                      *   
    * Purpose:  Interfaces with website for start time,    *
    * lights up your cushion when your accountability      *
    * is seated. Triggers a web-based animation when you   *
    * sit.                                                 *
    *                                                      *   
    * Usage:                                               *   
    *  requires wifi credentials. Set MY_NAME & YOUR_NAME  *           
    *  for each user                                       *
    *                                                      *
    * Credit: Makes use of Kate and Nick's example code for*  
    * communicating with Pubnub                            *
    /*******************************************************/  

#include <ArduinoJson.h>
#include <SPI.h>

#include <WiFi101.h>
#define PubNub_BASE_CLIENT WiFiClient
#include <PubNub.h>

//OCADU Wifi credentials below
static char ssid[] = "ocadu-embedded";      //SSID of the wireless network
static char pass[] = "internetofthings";    //password of that network
int status = WL_IDLE_STATUS;                // the Wifi radio's status


//Chris's pubnub account keys below
const static char pubkey[] = "pub-c-31315745-29a7-4863-bc2a-97a528df3c93";  //from PubNub account
const static char subkey[] = "sub-c-ac854de4-c885-11e7-9695-d62da049879f";  //from PubNub account

const static char pubChannel[] = "channel1"; //choose a name for the channel to publish messages to
const static char subChannel[] = "channel1"; //choose a name for the channel to read messages from

const int solenoid = 11;   //this pin switches the solenoid that rings the bell.
const int el = 10;         //this pin switches on/off the EL wire which illuminates the cushion
const int cushionPin = 9;  //this pin reads from the sandwich switch in the cushion.
const int NUM_RINGS = 3;   //number of times to ring the bell to start/end a session.
const String MY_NAME = "chrisMessage"; //this is the Key associated with the messages I publish.
const String YOUR_NAME = "karoMessage";//this is the key associated with messages my partner publishes.

const int I_AM_NOT_SITTING = 0;  //these are the values paired with the above keys that are published to pubnub 
const int I_AM_SITTING = 1;
const int YOU_ARE_NOT_SITTING = 0;
const int YOU_ARE_SITTING = 1;
const int BEGIN_SITTING = 4;     //these two values are published by the server
const int END_SITTING = 5;

int cushionVal = 1;      //Current state of cushion. 
int cushionPrev = HIGH;  //initial state of switch is on. That way, if it's off, we'll send a signal to pubnub indicating the "change"
int prevMsg = 0;         //the content of the message we'll send.

unsigned long lastRefresh = 0;
int publishRate = 1000;

int myMsg;                //myMsg is what I publish using MY_NAME
int yourMsg;              //the message I receive from YOUR_NAME on pubnub.

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
//in this loop, we publish only when something has changed. We read from pubnub on a fixed interval.
  
  cushionVal = digitalRead(cushionPin);

  if ((cushionVal == LOW) && (cushionPrev == HIGH)) //LOW = seated. If there is a change and I am now sitting, trigger the feed update. 
  {
    myMsg = I_AM_SITTING;
    publishToPubNub(myMsg);
    digitalWrite(el, LOW); //turn off el wire to reduce distractions when I am sitting
    Serial.print("My message: ");
    Serial.println(myMsg);
  } else if ((cushionVal == HIGH) && (cushionPrev == LOW))  { //if I get up, send a message to pubnub so that my animation stops.
    myMsg = I_AM_NOT_SITTING;
    publishToPubNub(myMsg);
    Serial.print("My message: ");
    Serial.println(myMsg);
  }
  
  if (millis() - lastRefresh >= publishRate) //theTimer to trigger the reads
  {
    readFromPubNub();
    if (yourMsg != prevMsg) {
      switch (yourMsg) {
        case YOU_ARE_SITTING:           //turn on the EL wire to remind me that our session has started and my partner is sitting.
          if (cushionVal == HIGH) {
            digitalWrite(el, HIGH);
          } else {
            digitalWrite(el, LOW);
          }
          break;
        case YOU_ARE_NOT_SITTING:      //turn off the EL wire if my partner is not sitting.
          digitalWrite(el, LOW);
          break;
        case BEGIN_SITTING:           //ring the bell if the server starts the session
          ding();
          break;
        case END_SITTING:             //ring the bell if the server ends the session
          ding();
          while(true); //then do nothing forever
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
    delay(50);              //ring works best when the solenoid is extended and retracted quickly.
    Serial.println("ding!");
    digitalWrite(solenoid, LOW);
    delay(2500);            //2.5s between rings
  }
}

