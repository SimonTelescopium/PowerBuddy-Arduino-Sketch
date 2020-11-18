#include <EEPROM.h>
//
//                          __         ___                        
//                         (_ . _  _  _ | _| _ _ _ _  _ .    _ / _ 
//                         __)||||(_)| )|(-|(-_)(_(_)|_)||_|||| _) __ 
//                         |__)_     _ _|__)    _| _||        |   /  \
//                         |  (_)\)/(-| |__)|_|(_|(_|\/   \/  | O \__/
//                         ========================= /               

/* ############################################################################################
 * #//////////////////////////////////////////////////////////////////////////////////////////# 
 * #//////////////////////////////  USE AT YOUR OWN RISK  ////////////////////////////////////#
 * #//////////////////////////////  ====================  ////////////////////////////////////#
 * #//////////////////////////////////////////////////////////////////////////////////////////#
 * #// UNSUPPORTED SOFTWARE OFFERED UNDER GPLv3.0 (http://www.gnu.org/licenses/gpl-3.0.html)//#
 * #// Whilst I've taken care to ensure this works for me, this software does not come with //#
 * #// any warranty or guarantee, if it damages anything or causes you or another 3rd party //#
 * #// loss or harm this is NOT MY RESPONSIBILITY  ///////////////////////////////////////////#
 * #//////////////////////  ---------------------  ///////////////////////////////////////////#
 * #//////////////////////////////////////////////////////////////////////////////////////////#
 * ############################################################################################
 */

/* ----------------------------------------------------------------------------------------
                  developed from TIGRA Astronomy Power Controller
 https://bitbucket.org/tigra-astronomy/ta.arduinopowercontroller-ascom-switch-driver/wiki/Home
 I couldn't get their ASCOM Driver working so developed my own and expanded the ARDUINO UNO code 
 it is no longer compatable with the TIGRA driver as I've almost modified it beyond recognition
 the only thing that remains is the serial state machine
 EEPROM is good for 100,000 write/erase cycles, 3.3 ms per write, UNO 1024 bytes of EEPROM
   -----------------------------------------------------------------------------------------
 */

 /*-----------------------------------------------------------------------------------------
  *                                   CIRCUIT SET-UP
  *                                   ==============
  *  Designed to run on an Arduino UNO with a relay shield.
  *  You can use any (up to 8) digital pins, for relays, connect these to a relay driver
  *  a high state on the pin is interpreted as 'on' so wire-up your relay driver so it 
  *  is 'on' when there is a high state on the pin.
  *  For sensors, only 'on/off' are supported, a high state on a pin is interpreted as 'on'
  *  
  *  When the Arduino is powered down the relays will deenergise (off) so ensure you wire
  *  your powered down state as you would like it to behave when there is no power to the 
  *  Arduino. 
  *  
  *  To prevent the Arduino from resetting when it is connected connect a 10uF capacitor 
  *  between ground and the reset pin, remember you can't upload a sketch to the arduino 
  *  when this capacitor is connected. If you don't do this the arduino will reset to the
  *  power-up state when connected - which might also be desirable depending on your set-up
  *  
  *  ---------------------------------------------------------------------------------------
  */

 //                    -- addressing for EEPROM --
 //**-- these variables store the ADDRESS in the EEPROM not the value! --**
   
int addSWWrite = 1; //first EEPROM address of write property
int addnumSW = 9; //EEPROM address of number of switchs property
int addSWname = 10; // EEPROM address of the name of the first switch 
int adddescription = 266; //EEPROM address of the description of first switch
int addSWPowerOnStatus = 778; //EEPROM address for power on default status

void SerialReceive();

const int SerialBufferSize = 34; // this is the buffer size it needs to be 34 characters to allow for setting switch name

/* -----------------------------------------------------------------------------------
 *** -------                    SETUP PIN NUMBERS                          ------- ***

             Enter the digial pin numbers for your relays in order
                    e.g relay 0 in the example below is pin D4
                  This needs to reflect how your circuit is built.

 -----------------------------------------------------------------------------------*/

int relayArray [8] = {4, 5, 6, 7, 8, 9, 2, 3}; //enter digital pin numbers into array to reflect your build


enum SerialState {start, wait, receive};
enum SerialState currentState = start;
char receiveBuffer [SerialBufferSize];
int bufferPosition = 0;
enum SerialState lastState = start;

/* ------------------------------------------------------------------------------------
                                    Command Syntax        (you don't need to know this)
   ------------------------------------------------------------------------------------                                            
     
    : resets buffer (put this before every command)
    # terminates command (put this after every command)

    
                         *** COMMANDS ARE CASE SENSITIVE ***
     
                            UPPERCASE = WRITE COMMAND
                            lowercase = read command

      
    S = set relay position (turn relay on/off)
    s = get relay position (read relay position) 
    w = get 'write' property for relay - i.e. find out if you can control the switch
    n = get number of switches this arduino controls
    l = get name (label) of switch
    L = set name of switch 
    d = get description of switch e.g. 
    X = Force a clear Buffer 
    P = set power on state for relay
    p = read power on stare for relay
    
                                      EXAMPLES (you can test these with the serial monitor)
                     (inverted commas are not part of the string) 

                                x = relay number 0 - 7
                          B = Boolean logic, 1 = on, 0 = off
          $30 = string of up to 30 characters (do not use : or # in this string)
          $62 = string of up to 62 characters (do not use : or # in this string)
                          
    EXAMPLE      GENERIC EXAMPLE  RESPONSE    COMMENTS          
    ':S01#'         ':SxB#'       ':SxB#'     turn on(1)/off(0) relay (x) 
    ':s1#'          ':sx#'        ':sxB#'     read status (B) of relay (x)
    ':w1#'          ':wx#'        ':wxB#'     read the 'write' property (B) of relay (x) 
    ':n#'           ':n#'         ':x#'       read how many relays this arduino supports (x)
    ':L1EQ6 Power#' ':L$30#'      ':Cx#'      write label ($30) of switch (x) 
    ':l1#'          ':lx#'        ':lx$30#'   read label ($30) of switch (x) 
    ':d1#'          ':dx#'        ':dx$62#'   read description ($62) of switch (x)
    ':X#'           ':X#'         NO RESPONSE clears buffer (shouldn't be needed but if the driver gets 
                                              something it doesn't expect it will send this command)
    ':P11#'         ':PxB#'       ':PxB#'     set the power on state (B) for relay (x)
    ':p1#'          ':px#'        ':PxB#'     read power on state (B) for relay (x)

    ------------------------------------------------------------------------------------------------
*/
void setup()
{

    /* -------------------------------------------------------------------------------
     *** -------                1st TIME SET-UP PROCEEDURE               ------- ***
    
     We don't want to write defaults to the EEPROM on every boot, so we control this
     by writing a 1 in address 1 the code below then skips the writing of the defaults.
     However we do need defaults to be saved during comissioning, so once you have 
     modified the default values in the code below to suit your needs you need to 
     write a zero into address 1 the code will then copy your defaults to the EEPROM.
     Immeadiatly after you have done this you need to comment out 'EEPROM.write(0,0)' 
     and re-upload, then the code will skip the wrting of defaults on future power-ups
    
     *** -------                        PROCEDURE                         ------- ***
     
     1) to configure the Arduino first time UNCOMMENT the the line 'EEPROM.write(0,0)'
     2) compile and upload
     3) comment out the line 'EEPROM.write(0,0)'  
     4) compilie and upload again - now every time the arduino is started it wont 
        write defaults to the EEPROM 
    */
    
    //EEPROM.write(0,0);  // *** ------- uncomment to write default values ------- ***
    
    /* --------------------------------------------------------------------------------
     *  REMEMBER TO COMMENT OUT ABOVE LINE AND RE-UPLOAD ONCE DEFAULTS ARE WRITTEN
     * -------------------------------------------------------_------------------------
     */
    
    if (EEPROM.read(0) == 0)
    {

      
      EEPROM.write(0,1);  //set to 1 means this loop wont be called again

      /*       -----------------------------------------------------------              
       *       |          SET READ/WRITE STATUS OF SWITCH/RELAY          |
       *       |            Edit the value after the comma to...         |
       *       |           1 for a relay or 0 for a sensor/unused        |
       *       -----------------------------------------------------------
       */
      
      EEPROM.write(addSWWrite,1);  //switch 1 can control read/write
      EEPROM.write(addSWWrite+1,1);  //switch 2 can control read/write
      EEPROM.write(addSWWrite+2,1);  //switch 3 can control read/write
      EEPROM.write(addSWWrite+3,1);  //switch 4 can control read/write
      EEPROM.write(addSWWrite+4,0);  //switch 5 is read only/unused
      EEPROM.write(addSWWrite+5,0);  //switch 6 is read only/unused
      EEPROM.write(addSWWrite+6,0);  //switch 7 is read only/unused
      EEPROM.write(addSWWrite+7,0);  //switch 8 is read only/unused

       /*      -----------------------------------------------------------              
       *       |        SET THE NUMBER OF RELAYS/SWICHES SUPPORTED       |
       *       |  Edit the value after the comma to the number of relays |
       *       |    your circuit will support, up to 8 are supported     |
       *       -----------------------------------------------------------
       */
      
      EEPROM.write(addnumSW,4);      //number of switches supported (supports up to 8 switches)


      /*       -----------------------------------------------------------
      //       |   Do NOT USE '#' or ':'  in names or descriptions!!!!   |
      //       -----------------------------------------------------------
      */
      /*       -----------------------------------------------------------
       *       |                    DEFAULT NAME VALUES                  |
       *       |      edit the strings to defaults you want to use       |
       *       |        ( max of 30 characters dont use ':' or '#')      |
       *       -----------------------------------------------------------
       */         
       
      writeStringToEEPROM(addSWname,"EQ6 Power"); //name of switch 1 must be less than 30 characters
      writeStringToEEPROM(addSWname+32,"Filterwheel Power"); //name of switch 2 must be less than 30 characters
      writeStringToEEPROM(addSWname+64,"Dew Heater Power"); //name of switch 3 must be less than 30 characters
      writeStringToEEPROM(addSWname+96,"Camera Cooler Power"); //name of switch 4 must be less than 30 characters
      writeStringToEEPROM(138,"Switch 5 name must be <=30 chr"); //name of switch 5 must be less than 30 characters
      writeStringToEEPROM(170,"Switch 6 name must be <=30 chr"); //name of switch 6 must be less than 30 characters
      writeStringToEEPROM(202,"Switch 7 name must be <=30 chr"); //name of switch 7 must be less than 30 characters
      writeStringToEEPROM(234,"Switch 8 name must be <=30 chr"); //name of switch 8 must be less than 30 characters

      /*       -----------------------------------------------------------
               |              DEFAULT DESCRIPTION VALUES                 |
               |  these defaults are not yet used by the ASCOM driver    |
               |     an update might use them so worth settimg them      |
               |      edit the strings to defaults you want to use       |
               |        ( max of 62 characters dont use ':' or '#')      |
               -----------------------------------------------------------
       */   
      
      writeStringToEEPROM(adddescription,"Toggle power on/off for EQ6 mount"); //description of switch 1 must be less than 62 characters
      writeStringToEEPROM(adddescription+64,"Toggle power on/off for Filterwheel"); //description of switch 2 must be less than 62 characters
      writeStringToEEPROM(adddescription+128,"Toggle power on/off for Dew heaters"); //description of switch 3 must be less than 62 characters
      writeStringToEEPROM(adddescription+192,"Toggle power on/off for Camera cooler"); //description of switch 4 must be less than 62 characters
      writeStringToEEPROM(adddescription+256,"description of switch 5 must be less than 62 characters"); //description of switch 5 must be less than 62 characters
      writeStringToEEPROM(adddescription+320,"description of switch 6 must be less than 62 characters"); //description of switch 6 must be less than 62 characters
      writeStringToEEPROM(adddescription+384,"description of switch 7 must be less than 62 characters"); //description of switch 7 must be less than 62 characters
      writeStringToEEPROM(adddescription+448,"description of switch 8 must be less than 62 characters"); //description of switch 8 must be less than 62 characters

      /*  -------------------------------------------------------------------------
                                    SET POWER ON STATE  
          IF you leave your scope unatended and the power fails and comes back 
          on you want to ensure critical systems are safe.
          E.G you might want the mount to be off but the dew heater on by default.
          edit the number after the comma to 1 for on or 0 for off 
          -------------------------------------------------------------------------
        */
      
      EEPROM.write(addSWPowerOnStatus,0);  //switch 1 off at arduino power-up
      EEPROM.write(addSWPowerOnStatus+1,0);  //switch 2 off at arduino power-up
      EEPROM.write(addSWPowerOnStatus+2,1);  //switch 3 on at arduino power-up
      EEPROM.write(addSWPowerOnStatus+3,0);  //switch 4 off at arduino power-up
      EEPROM.write(addSWPowerOnStatus+4,0);  //switch 5 off at arduino power-up
      EEPROM.write(addSWPowerOnStatus+5,0);  //switch 6 off at arduino power-up
      EEPROM.write(addSWPowerOnStatus+6,0);  //switch 7 off at arduino power-up
      EEPROM.write(addSWPowerOnStatus+7,0);  //switch 8 off at arduino power-up
      
    }

/*  ===================================================================================
                                        TESTING 
                                        
    Once code is uploaded to arduino (see notes above about 1st time set-up proceedure)
    Open the 'Serial Monitor' for the COM Port your arduino is connected to
    Connection is 9600bps 8N1 (this should be detected)
    send the following command (without the inverted commas) ':n#'
    it will return (without the inverted commas) ':x#' where x is the number of 
    switches you set
    This proves te arduino is responding as expected, if you want to test further
    refer to the commands above (line ~80)
    =================================================================================== 
 */
    
/*  ===================================================================================
   * Do not change code beyond this point - well it is open source you can do what you 
   *           like but it might not work and may break the ASCOM Driver
   * =================================================================================
   */











  
  //                  READ RELAY STATE TO POWER-UP DEFAULTS
  for (int i = 0; i < 8; i++) {
    if (EEPROM.read(addSWWrite+i)==1){
      pinMode(relayArray[i], OUTPUT);

      digitalWrite(relayArray[i],EEPROM.read(addSWPowerOnStatus+i));
    }
    else {
      pinMode(relayArray[i], INPUT);
    }
    
    
    
  }
}

void loop()
{
  SerialStateMachine();
}

int DigitalReadOutputPin(int pin)
{
  int bit = digitalPinToBitMask(pin);
  int port = digitalPinToPort(pin);
  if (port == NOT_A_PIN)
    return -1;
  //return (*portOutputRegister(port) & bit) ? 0 : 1;
  if ((*portOutputRegister(port) & bit) ? 0 : 1 == 1)
    {
      return 0;
    }
    else
    {
      return 1;
    }
  
}

char ReadOneChar()
{
    if (Serial.available() == 0)
    return 0;
    char rxByte = Serial.read();
    //Serial.println(rxByte);
    return rxByte;
}


void testMode()
{
  for (int i = 0; i < 8; i++) //turn relays on
  {
    digitalWrite(relayArray[i], 0);
    delay (1000);
  }

  for (int i = 0; i < 8; i++) //turn relays off
  {
    digitalWrite(relayArray[i], 1);
    delay (1000);
  }
}



void SerialStateMachine()
{
 
  lastState = currentState;
  switch (currentState)
  {
    case start: SerialStart();
      break;
    case wait: SerialWait();
      break;
    case receive: SerialReceive();
      break;
  }
  
}

void SerialStart()
{
  ClearBuffer();
  Serial.begin(9600);
  currentState = wait;
  // Serial.println("SerialStart Initiated");
}

void ClearBuffer()
{
  bufferPosition = 0;
  for (int i =0; i < SerialBufferSize; i++)
  {
    receiveBuffer[i] = 0;
  }
}

void SerialWait()
{

  char rxByte = ReadOneChar();
 
  if (rxByte != ':')
    return;
  ClearBuffer();
  currentState = receive;
  //debug
  //Serial.println("buffer cleared");
}

void SerialReceive()
{

  char rxByte = ReadOneChar();
  
  switch (rxByte)
  {
    case ':': 
      ClearBuffer();
      break;
    case '#':
      InterpretCommand();
      break;
    case 0:
      return;
    default:
      receiveBuffer[bufferPosition++] = rxByte;
      break;
  }
}

// returns on the serial port the relaystatus for 'relay' 
void SendRelayStatus(int relay,int relayStatus)
{
  Serial.print(':');
  Serial.print(receiveBuffer[0]);
  Serial.print(relay);
  Serial.print(relayStatus);
  Serial.print('#');
}

// deciphers the relay number and returns it
int GetRelayNumber()
{
  char relay = receiveBuffer[1];
  if (relay <'0' || relay > '7')
  {
    Serial.println("Bad relay number");
    return -1;
  }
  int relayNumber = relay - '0';
  return relayNumber;
}


void GetSW()
{  // GetSW() reads sw position and sends response
  if (bufferPosition != 2)
  {
    Serial.println("Wrong number of characters");
    return;
  }
  int relay = GetRelayNumber();
  if (relay <0) return;

  if (int writeproperty = EEPROM.read(addSWWrite+relay)==0){
    //this is a read only switch
    int relayStatus = digitalRead(relayArray[relay]); 
    SendRelayStatus(relay,relayStatus);
  }
  else {
    //this is a read wite switch so digital pin will be in OUTPUT mode
    int relayStatus = DigitalReadOutputPin(relayArray[relay]);
    SendRelayStatus(relay,relayStatus);
  }


  
  
  //Serial.println(relayStatus);

  }

void GetPowerOnState()
{  // Get power on state and send response
  if (bufferPosition != 2)
  {
    Serial.println("Wrong number of characters");
    return;
  }
  int relay = GetRelayNumber();
  if (relay <0) return;
  
  SendRelayStatus(relay,EEPROM.read(addSWPowerOnStatus+relay));
  
  }


// sets the relay on or off
void WriteRelayPin(int relayPin, int relayValue)
{
  digitalWrite(relayPin, relayValue == 1 ? 1 : 0);
}


void SetSW()
{  //this interprets the set SW command
  if (bufferPosition != 3)
  {
    Serial.println("Wrong number of characters");
    return;
  }
  int relay = GetRelayNumber();
  if (relay <0) return;
  int relayPin = relayArray[relay];
  int onOffCommand = receiveBuffer[2];
  if (onOffCommand <'0' || onOffCommand > '1')
  {
    Serial.println("Bad data");
    return;
  }
  int relayValue = onOffCommand - '0';
  WriteRelayPin(relayPin, relayValue);
  SendRelayStatus(relay,relayValue);
  }

void GetSWWriteProperty()
{  //sends over serial port the write status of the relay
  if (bufferPosition != 2)
  {
    Serial.println("Wrong number of characters");
    return;
  }
  int relay = GetRelayNumber();
  if (relay <0) return;
  int writeproperty =EEPROM.read(addSWWrite+relay); 
  SendRelayStatus(relay,writeproperty);
  }

void GetSWNumProperty()
{  // return the number of switches this Arduino controls
  if (bufferPosition != 1)
  {
    Serial.println("Wrong number of characters");
    return;
  }
  
  int writeproperty =EEPROM.read(addnumSW); 
  Serial.print(':');
  Serial.print(writeproperty);
  Serial.print('#');
  }

void GetSWLabelProperty()
{  // return the name of the switch/relay
  if (bufferPosition != 2)
  {
    Serial.println("Wrong number of characters");
    return;
  }
  int relay = GetRelayNumber();
  if (relay <0) return;
  String property; 

  property=readStringFromEEPROM(addSWname+(relay*32));
  Serial.print(':');
  Serial.print(receiveBuffer[0]);
  Serial.print(relay);
  Serial.print(property);
  Serial.print('#');
  }

void GetSWDescriptionProperty()
{  // return the description of the switch/relay
  if (bufferPosition != 2)
  {
    Serial.println("Wrong number of characters");
    return;
  }
  int relay = GetRelayNumber();
  if (relay <0) return;
  String property; 
  
  property=readStringFromEEPROM(adddescription+(relay*64));
  Serial.print(':');
  Serial.print(receiveBuffer[0]);
  Serial.print(relay);
  Serial.print(property);
  Serial.print('#');
  }

void SetSWLabelProperty()
{ // Save the name of the switch
  if (bufferPosition < 3)
  {
    Serial.println("setSWName() Wrong number of characters");
    return;
  }
  int relay = GetRelayNumber();
  if (relay <0) return;
  //
  
  Serial.print(":C");
  Serial.print(relay);
  Serial.println('#');
  writeStringToEEPROM(addSWname+(relay*32),CharCrop(receiveBuffer));
  //Serial.print(CharCrop(receiveBuffer));
  //Serial.print('#');
  }


void SetPowerOn()
{  //this interprets the SetPowerOn command and saves the status to the eeprom
  if (bufferPosition != 3)
  {
    Serial.println("Wrong number of characters");
    return;
  }
  
  int relay = GetRelayNumber();
  if (relay <0) return;
  int onOffCommand = receiveBuffer[2];
  if (onOffCommand <'0' || onOffCommand > '1')
  {
    Serial.println("Bad data");
    return;
  }
  int relayValue = onOffCommand - '0';

  EEPROM.write(addSWPowerOnStatus+relay,relayValue);  //save arduino power-up state to eeprom
  
  SendRelayStatus(relay,relayValue);
  }

  

String CharCrop(char *buf)
// crop the control commands from the string 
{
  char text[34];
    
  for (byte i = 0; i < 33; i++) {
    text[i]=0;
    if(buf[i] == '#') break;
    if(buf[i] == 0) break;
    text[i] = buf[i+2];
  } 
  //Serial.println("CharCrop returns:"& text); //debug only
  return text;
}
  
// this is the main sub that interprets the command and calls the appropriate subroutine
void InterpretCommand()
{
  // Serial.print("Received ");
  // Serial.println(receiveBuffer);
  // debug
  // Serial.println("InterpretCommand()");
  switch (receiveBuffer[0])
  {
    case 'S':
      SetSW();
      break;
    case 's': 
      GetSW();
      break;
    case 'w': 
      GetSWWriteProperty();
      break;
    case 'n': 
      GetSWNumProperty();
      break;
    case 'l': 
      GetSWLabelProperty();
      break;
    case 'L': 
      SetSWLabelProperty();
      break;
    case 'd': 
      GetSWDescriptionProperty();
      break;
    case 'X': 
      ClearBuffer();
      break;
    case 'P': 
      SetPowerOn();
      break;
    case 'p': 
      GetPowerOnState();
      break;
    default:
      Serial.println("!Bad command:use S,s,w,n,l,L,d,X e.g. :l2# to get switch 2 label");
      ClearBuffer(); // forces a clear of buffer - shouldn't be needed but makes sure buffer is empty
  }
  
}

// used for writing strings to the EEPROM
void writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
}

//used for reading strings from the eeprom
String readStringFromEEPROM(int addrOffset)
{
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];
  for (int i = 0; i < newStrLen; i++)
  {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
//  data[newStrLen] = '<pre class="EnlighterJSRAW" data-enlighter-language="cpp">String readStringFromEEPROM(int addrOffset);
{
int newStrLen = EEPROM.read(addrOffset);
char data[newStrLen + 1];
for (int i = 0; i < newStrLen; i++)
{
  data[i] = EEPROM.read(addrOffset + 1 + i);
}
data[newStrLen] = '\0';
return String(data);
}

  return String(data);
}
