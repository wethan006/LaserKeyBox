#include <Servo.h> //adds servo motor library
#include <SPI.h> //SPI coms
#include <MFRC522.h> 
#define RST_PIN 8 //sets the reset pin to reset
#define SS_PIN 10 //SDA (SS) pin connected

//objects
Servo myservo; //creates a servo motor object to control
MFRC522 rfid(SS_PIN, RST_PIN);

const String UPDATE_MODE = "UPDATE_MODE";
//pins
const int SERVOPIN = 9; //the connected pin for orange cable can be 9 or 10 digital
const int LIMITSWITCHPIN = A4; //sets the limit switch pin
const int LOCKEDANGLE = 0;
const int UNLOCKEDANGLE = 90;
const int BUZZERPIN = A3; //connects to the positive side of the buzzer
const int GREENLEDPIN = A0; //the pin for LEDS, can be 13,12,11
const int YELLOWLEDPIN = A1; //used to show that ID adding is on
const int REDLEDPIN = A2;
const int FANPIN = 6;
//analog outputs A0-A5
//RFID, NUMBER
const byte masterUID[] = {0xCB, 0xCE, 0xE2, 0x1A}; //must be prefixed with 0x for byte 0x1A
//sets the ready for action
boolean ready = false;

void setup() {
  // put your setup code here, to run once: 
  while (!Serial); //wait for serial connection to open
  Serial.begin(9600); //connect serial to raspberry pi
  //sets the servo lock
  myservo.attach(SERVOPIN); //connects the object to the signal pin
  myservo.write(LOCKEDANGLE); //inital position of the motor should be the lock position
  //sets the limit switch
  pinMode(LIMITSWITCHPIN, INPUT_PULLUP); //uses the internal pullup resistor for the limitswitch
  //sets the buzzer
  pinMode(BUZZERPIN, OUTPUT); //sets the output pin for the buzzer
  digitalWrite(BUZZERPIN, LOW); //this sets the buzzer to off in the beginning
  //sets the LED pins
  pinMode(GREENLEDPIN, OUTPUT); //sets pin to output
  digitalWrite(GREENLEDPIN, HIGH); //green is on, showing system ready
  pinMode(YELLOWLEDPIN, OUTPUT);
  digitalWrite(YELLOWLEDPIN, HIGH);
  pinMode(REDLEDPIN, OUTPUT);
  digitalWrite(REDLEDPIN, HIGH);
  //RFID
  SPI.begin(); //start SPI
  rfid.PCD_Init(); //start object
  ready = false;
  //fan pin
  pinMode(FANPIN, OUTPUT);
  analogWrite(FANPIN, 50);
}

void loop() {
  // put your main code here, to run repeatedly:
  while(!ready) {
    String piConnected = Serial.readStringUntil('\n');
    piConnected.trim();
    if(piConnected.length() > 0) {
      if(piConnected == "PI_READY"){
        digitalWrite(YELLOWLEDPIN, LOW);
      } if (piConnected == "RFID_READY") {
        digitalWrite(REDLEDPIN, LOW);
        ready = true;
      }
    } 
  }
  
  String openLock = Serial.readStringUntil('\n'); //read output from pi until new line
  openLock.trim(); //gets rid of whitespace
  if(openLock.length() > 0) { //if the input has any length to it, if not here the serial input will keep printing
    if(openLock == "OPEN_LOCK") {
      Serial.println("Lock is opening");
      myservo.write(UNLOCKEDANGLE); //turns 90 degrees
      //controls the buzzer
      digitalWrite(BUZZERPIN, HIGH); //on
      delay(400);
      digitalWrite(BUZZERPIN, LOW); //Sound will turn off on its own, but the pin will not, this turns it off
      digitalWrite(YELLOWLEDPIN, HIGH);
      //cycles green led until the door is opened then turns off green led
      while(digitalRead(LIMITSWITCHPIN) == LOW) {
        digitalWrite(GREENLEDPIN, LOW);
        delay(200);
        digitalWrite(GREENLEDPIN, HIGH);
        delay(200);
      }
      digitalWrite(GREENLEDPIN, LOW);
    } else { //door not open 
      digitalWrite(REDLEDPIN, HIGH); //red light on
      for (int i = 0; i < 3; i++) {
        digitalWrite(BUZZERPIN, HIGH); //on
        delay(100);
        digitalWrite(BUZZERPIN, LOW); //off
        delay(100);
      }
      digitalWrite(REDLEDPIN, LOW); //red light off
    }
  }

  //auto locks the box
  if (digitalRead(LIMITSWITCHPIN) == LOW) { //LOW means pressed, HIGH means not pressed
    delay(50); //adds a small delay for the switch then will check again, bounce check
    if (digitalRead(LIMITSWITCHPIN) == LOW) {
      myservo.write(LOCKEDANGLE);
      digitalWrite(YELLOWLEDPIN, LOW);
      digitalWrite(GREENLEDPIN, HIGH); //turns green light back on
    }
  }
  
  //RFID
  if(rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) { //start of RFID if
    boolean authorized = false;
    //read and compare to master card designed so all you need to change is the masterUID for this to work
    if (rfid.uid.size == sizeof(masterUID)) {
      for(int i = 0; i < rfid.uid.size; i++) {
        if(memcmp(rfid.uid.uidByte, masterUID, sizeof(masterUID)) == 0){
          authorized = true;
        } else { //if any of the bytes do not match the boolean is false
          authorized = false;
          break;
        }
      }
    }
    //if the door is closed but was just swipped to open, which turns the yellow led on, the rfid will not work
    if (authorized && digitalRead(LIMITSWITCHPIN)==LOW && digitalRead(YELLOWLEDPIN)==HIGH){
      authorized = false;
    }
    else if (authorized && digitalRead(LIMITSWITCHPIN)==LOW) { //switching to update ID list mode
      Serial.println(UPDATE_MODE); //sends the update string to the raspberry PI
      rfid.PICC_HaltA(); //Halt PICC
      while(authorized) {
        String updateReady = Serial.readStringUntil('\n'); //scans for update ready message from PI
        updateReady.trim(); //gets rid of whitespace
        if(updateReady.length() > 0) {
          if(updateReady == "UPDATE_READY") {
            digitalWrite(YELLOWLEDPIN, HIGH); //turns on yellow light
            for (int i = 0; i < 5; i++) {
              digitalWrite(BUZZERPIN, HIGH); //on
              delay(100);
              digitalWrite(BUZZERPIN, LOW); //off
              delay(100);
            }
          }
          if(updateReady == "EXIT_UPDATE") {
            digitalWrite(BUZZERPIN, HIGH);
            delay(400);
            digitalWrite(BUZZERPIN, LOW);
            authorized = false;
          }
        }
      }
      digitalWrite(YELLOWLEDPIN, LOW); //turns off yellow led
    } //updatemode if
    //controls delete mode
    //checks if the door is open and yellow led is on and green led is off
    else if (authorized && digitalRead(LIMITSWITCHPIN)==HIGH && digitalRead(YELLOWLEDPIN)==HIGH && digitalRead(GREENLEDPIN)==LOW){
      boolean deleteMode = true;
      digitalWrite(REDLEDPIN, HIGH); //turns on red led
      //beep twice
      for(int i = 0; i < 2; i++){
        digitalWrite(BUZZERPIN, HIGH);
        delay(100);
        digitalWrite(BUZZERPIN, LOW);
        delay(100);
      }
      Serial.print("DELETE_MODE");
      while (deleteMode){
        String deleteComplete = Serial.readStringUntil('\n');
        deleteComplete.trim();
        if(deleteComplete.length()>0 && deleteComplete == "ID_RESET_COMPLETE"){
          deleteMode = false;
          authorized = false;
          digitalWrite(REDLEDPIN, LOW);
          digitalWrite(BUZZERPIN, HIGH);
          delay(100);
          digitalWrite(BUZZERPIN, LOW);
        }
      }
    }
    /*
    //Use this section of code for getting the RFID of a new tag card
    if ( ! rfid.PICC_IsNewCardPresent()) { //look for new cards
      return;
    }

    if (! rfid.PICC_ReadCardSerial()) {
      return;
    }

    Serial.print("Card UID:");
    for(byte i = 0; i < rfid.uid.size; i++) {
      Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(rfid.uid.uidByte[i], HEX);
    }
    Serial.println();

    rfid.PICC_HaltA(); 
    */
  } //end of RFID if
}
