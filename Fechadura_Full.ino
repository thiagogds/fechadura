/*
 
 * Pin layout 
 * Signal     Pin              Pin               Pin
 *            Arduino Uno      Arduino Mega      MFRC522 board
 * ------------------------------------------------------------
 * Reset      9                5                 RST
 * SPI SS     10               53                SDA
 * SPI MOSI   11               52                MOSI
 * SPI MISO   12               51                MISO
 * SPI SCK    13               50                SCK
 *
 * 
 */

#include <SPI.h>
#include <RFID.h>
#include <Wire.h>
#include <Keypad.h>
#include <Ethernet.h>

#define SS_PIN 6
#define RST_PIN 9

//Define constantes da máquina de estados.
#define PRINCIPAL 0
#define LER_RFID 1
#define LER_TECLADO 2
#define LER_TECLADO2 3
#define WAIT_HTTP 4

#define expander  B0100001  // Address with three address pins grounded.

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

// initialize the library instance:
EthernetClient client;

char server[] = "zonadarede.herokuapp.com";

///////////////////////////////

const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
char hexaKeys[ROWS][COLS] = {
  {'*','7','4','1'},
  {'0','8','5','2'},
  {'#','9','6','3'},
  {'d','c','b','a'}
};

RFID rfid(6,5); 
int estado;
char  tecla;
String senha;
int lastKeyTime;

void setup() {
	Serial.begin(9600);	// Initialize serial communications with the PC
        delay(1000);
        pinMode(6, OUTPUT);
        digitalWrite(6, HIGH); 
        // start the Ethernet connection
        Ethernet.begin(mac);
        delay(2000);
        Serial.print("My IP address: ");
        Serial.println(Ethernet.localIP());
	Serial.println("Sistema Pronto");
        SPI.begin();
        Wire.begin();         
        rfid.init();
        estado = PRINCIPAL;
}

void loop() {
  switch(estado)
  {
      case PRINCIPAL:
      {
	// Look for new cards
	if (rfid.isCard()) {      
            Serial.println("IS CARD");          
            if (rfid.readCardSerial()) {            
                          Serial.println(" ");
                          Serial.println("El numero de serie de la tarjeta es  : ");
  			Serial.print(rfid.serNum[0],DEC);
                          Serial.print(" , ");
  			Serial.print(rfid.serNum[1],DEC);
                          Serial.print(" , ");
  			Serial.print(rfid.serNum[2],DEC);
                          Serial.print(" , ");
  			Serial.print(rfid.serNum[3],DEC);
                          Serial.print(" , ");
  			Serial.print(rfid.serNum[4],DEC);
                          Serial.println(" ");
            }
          
        }
        else
        {
           if ( key_scan() )
           {
               estado = LER_TECLADO2;
               senha += tecla;
               lastKeyTime = millis();
               tone(8, 440, 20);
               Serial.print ("var: ");
      Serial.println (tecla);
           }
        }
	// Select one of the cards
	
        break;
      }      
      case LER_TECLADO:
      {
        int mSec = millis();
        if ( abs(mSec - lastKeyTime) > 200 )
        {
          if ( key_scan() )
          {             
               senha+= tecla;
               lastKeyTime = millis();
               tone(8, 440, 90);
               estado = LER_TECLADO2;
               Serial.print ("var: ");
               Serial.println (tecla);
               
          }
        }
        break;
      }
      case LER_TECLADO2:
      {
        int mSec = millis();
        if ( abs(mSec - lastKeyTime) > 200 )
        {
          if ( !key_scan() )
          {
              if ( senha.length() > 6 )
              {
                 estado = WAIT_HTTP;
                 Serial.println (senha);
                 httpRequest();
                 senha = "";
               }
               else
               {
                 estado = LER_TECLADO;
               }
              lastKeyTime = millis();  
          }
        }
        break;
      }
      case LER_RFID:
      {
	// Dump debug info about the card. PICC_HaltA() is automatically called.
	//mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
        estado = PRINCIPAL;
        break;
      }
      case WAIT_HTTP:
      {
  	  if (client.available()) {
              if (client.find("HTTP/1.1 200 OK")) 
              {
                Serial.println("Pode entrar!");
                toneAcerto();
              } else
              {
                toneErro();
                Serial.println("Senha incorreta :(");
              }    
              client.stop();
              estado = PRINCIPAL;
          }
          break;
      }
  }
}


/*
  void keyscan()
The columns are connected to the three lowest ports (P2, P1, P0) 
 and the columns have a weak pullup-resistor. We send a 1 to all three
columns at the same time, we send a 0 on the row for which we want to test, 
 and we send a 1 on the rows that we are not testing. The column on which 
 a key has been pressed will turn from 1 to 0.
Example: We send 11110111 which means we are testing on row 0 (R0). If the key 
 on row 0 and colum 1 is pressed (number 2 on keypad), then we will read the 
 pattern 11110101. In other words, the second lowest bit has been changed. We 
 repeat the procedure for each existing row on the keypad. In short:
with port connection [(-) R2 R1 R0 C2 C1 C0 ],
send 11110111 receive 11110101 means R0 C1 key was pressed.
Next, we send 11101111 to test row R1, etc.
*/
boolean key_scan()  
{
  
int row,column;
  byte send_pattern, receive_pattern,test_pattern; 
  byte send_pattern_array[]={
    B11101111, B11011111, B10111111,B01111111                        };
  byte test_pattern_array[]={
    B00000001, B00000010, B00000100,B00001000                       };
  int i=0;


  for (i=0; i<4;i++)
  {
    // Try each row. Send 0 on R1 port, the bit on the pressed column
    // will turn from 1 to 0 
    send_pattern=send_pattern_array[i];
    expanderWrite(send_pattern);
    receive_pattern=expanderRead();
    
    if(send_pattern!=receive_pattern)
    {
      row=i;
      for (int j=0;j<4;j++)
      {
        test_pattern=test_pattern_array[j]&receive_pattern;     
        if(test_pattern==0)
          column=j;
      }

             
      tecla = (hexaKeys[row][column]);
                        
      
      return true;
    }
  }
  return false;
}


// this method makes a HTTP connection to the server:
void httpRequest() {
  // if there's a successful connection:
  if (client.connect(server, 80)) {
    Serial.println("connecting...");
    // send the HTTP PUT request:
    client.println("GET /users/me/login?key=moleque&password="+senha+" HTTP/1.1");
    Serial.println("GET /users/me/login?key=moleque&password="+senha+" HTTP/1.1");
    client.println("Host: zonadarede.herokuapp.com");
    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");
    client.println();
  }
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
    Serial.println("disconnecting.");
    client.stop();
  }
}

void toneAcerto()
{
   tone(8, 1000, 300);
   delay(300);
   tone(8, 1000, 300);
}

void toneErro()
{
   tone(8, 100, 300);
   delay(300);
   tone(8, 100, 300);
}

void expanderWrite(byte _data ) {
  Wire.beginTransmission(expander);
  Wire.write(_data);
  Wire.endTransmission();
}

byte expanderRead() {
  byte _data;
  Wire.requestFrom(expander, 1);
  if(Wire.available()) {
    _data = Wire.read();
  }
  return _data;
}
