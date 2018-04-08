/********************************************************************************
   Boiler Interface V03
   4/9/18

   uses a 84x48 Graphic LCD
    https://www.sparkfun.com/products/10168

   Arduino pro mini with ATmega328P (3.3v, 8MHz)

   Vout = (Treturn + 18*Vin/10 - 74)/10

   10x values are used for Vin and Vout and Target to show 10ths value in display
   using /10 and %10 operations

   4/9/18: floor heating temperature limit increased to 135 degrees
***********************************************************************************/
#include <Wire.h>       // i2c for DtoA
#include <SPI.h>
#include "LCD_Functions.h"
#include "math.h"
#include <avr/wdt.h>      // watch dog timer
int i;
int Vin;                  // voltage read from controller
int Vout;                 // voltage sent to boiler
char Tvalue [3];

int DACsetting ;

char VinPort = A6;        // voltage read from controller / 3

char TretC [6];
char ToutC [6];
char TargetC [6];
char VinC[6];
char VoutC[6];

#define Rref 4016
#define R25 10000
float R1ntc;
int VR1;
char TretPort = A0;   // heat transfers from Tank1 to Tank2
char ToutPort = A1;

int Tret;
int Tout;
int Target;

float a = 0.001129148;    // Tekmar coefficients
float b = 0.000234125;
float d = 0.0000000876741;
float c = 0;

float av = 0.003354016;  //Vishay coefficients for NTCLP100E303H Thermistor
float bv = 0.000256985;
float cv = 2.62013E-06;
float dv = 6.38309E-08;

float TempK, TempC, TempF;

int offset = 3;

char HTPumpPort = 8;   // needs a voltage divider with BoilerInterfaceV00 PCB
char HWPumpPort = 2;
char LEDPort = 3;         // green LED
char BacklightPort = 9;   // backlight for display

int HTPump;            // on or off
int HWPump;            // on or off

char Status;            // Hot Tub, Hot Water, or Floor heating

///////////////////////////////////////////////////////////////////////////////////////
//  display_refresh: update the display with temperature values and pump status
////////////////////////////////////////////////////////////////////////////////////////
void display_refresh() {
  // constant text
  setStr("Boiler Control", 0, 0, BLACK);
  setStr("Tr:", 0, 8 + offset, BLACK);        // offset takes horizontal line into account
  setStr("Ts:", 42, 8 + offset, BLACK);
  setStr("VI:  .", 0, 16 + offset, BLACK);
  setStr("VO:  .", 0, 32, BLACK);
  setStr("Target:   .", 0, 40, BLACK);
  setStr("#", 48, 16 + offset, BLACK);

  for (i = 0; i < 84; i++) setPixel(i, 8, 1); // horizontal line

  // load temperature values into strings and onto display
  // Vin and Vout are multiplied by 10
  // use %10 to get decimal value
  sprintf(TretC, "%3d", Tret);
  sprintf(ToutC, "%3d", Tout);
  sprintf(VinC, "%2d", Vin / 10);
  sprintf(VoutC, "%2d", Vout / 10);

  setStr(TretC, 15, 8 + offset, BLACK);
  setStr(ToutC, 60, 8 + offset, BLACK);
  setStr(VinC, 18, 16 + offset, BLACK);
  setStr(VoutC, 18, 32, BLACK);

  sprintf(VinC, "%1d", Vin % 10);
  setStr(VinC, 36, 16 + offset, BLACK);

  sprintf(VoutC, "%1d", Vout % 10);
  setStr(VoutC, 36, 32, BLACK);

  sprintf(TargetC, "%3d", Target / 10);
  setStr(TargetC, 42, 40, BLACK);
  sprintf(TargetC, "%1d", Target % 10);
  setStr(TargetC, 66, 40, BLACK);

  if (Status == 1) setStr("DHW", 54, 16 + offset, BLACK);
  if (Status == 2) setStr("HT ", 54, 16 + offset, BLACK);
  if (Status == 3) setStr("FLR", 54, 16 + offset, BLACK);
  if (Status == 4) setStr("OFF", 54, 16 + offset, BLACK);

  updateDisplay();
}
void Read_Temperatures() {
  //////////////////////////////////////////////////////////////////////////
  // Read_Temperatures: get NTC resistance and derive temperature
  //////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////
  // get temperature from return pipe NTC thermister
  /////////////////////////////////////////////////////////////////////////
  VR1 = 0;                            // take an average of 10 samples as a noise filter
  for (i = 0; i < 10; i++) {
    VR1 += analogRead(TretPort);
    delay(10);
  }
  VR1 /= 10;
  R1ntc = 1023.0 * Rref / VR1 - Rref;
  // Vishay thermister calculation
  //  TempK = 1 / (a + b * log(R1ntc / R25) + c * log(R1ntc / R25) * log(R1ntc / R25) + d * log(R1ntc / R25) * log(R1ntc / R25) * log(R1ntc / R25));
  // All other thermister calculation
  TempK = 1 / (a + b * log(R1ntc ) + c * log(R1ntc ) * log(R1ntc ) + d * log(R1ntc ) * log(R1ntc ) * log(R1ntc ));
  TempC = TempK - 273.15;
  TempF = 1.8 * TempC + 32;
  Tret = TempF;                           // Tret in degrees F for cold line into boiler

  //////////////////////////////////////////////////////////////////////////
  // get temperature from supply pipe NTC thermister
  /////////////////////////////////////////////////////////////////////////
  VR1 = 0;                           // take an average of 10 samples as a noise filter
  for (i = 0; i < 10; i++) {
    VR1 += analogRead(ToutPort);
    delay(10);
  }
  VR1 /= 10;
  R1ntc = 1023.0 * Rref / VR1 - Rref;
  // Vishay thermister calculation
  //TempK = 1 / (av + bv * log(R1ntc / R25) + cv * log(R1ntc / R25) * log(R1ntc / R25) + dv * log(R1ntc / R25) * log(R1ntc / R25) * log(R1ntc / R25));
  // All other thermister calculation
  TempK = 1 / (a + b * log(R1ntc) + c * log(R1ntc) * log(R1ntc) + d * log(R1ntc) * log(R1ntc) * log(R1ntc));
  TempC = TempK - 273.15;
  TempF = 1.8 * TempC + 32;
  Tout = TempF;                         // Tout in degrees F for hot line out of boiler

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//      SETUP
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(9600);
  pinMode (HWPumpPort, INPUT);
  pinMode (HTPumpPort, INPUT);
  pinMode (BacklightPort, OUTPUT);
  analogWrite (BacklightPort, 128);     // half brightness
  pinMode (LEDPort, OUTPUT);            // LOW to set LED ON
  digitalWrite (LEDPort, LOW);

  wdt_enable(WDTO_2S);      // 2 second watch dog timer

  lcdBegin();       // This will setup our pins, and initialize the LCD
  updateDisplay();  // with displayMap untouched, SFE logo
  setContrast(40);  // Good values range from 40-60
  delay(200);

  clearDisplay(WHITE);
  updateDisplay();

  delay(500);

  digitalWrite (LEDPort, HIGH);       // turn off green LED
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//      LOOP
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  wdt_reset();                         // pet the dog
  HWPump = digitalRead(HWPumpPort);     // see if Hot Water Pump is on
  HTPump = digitalRead(HTPumpPort);     // see if Hot Tub Pump is  on

  Read_Temperatures();                  // read thermisters

  Vin = analogRead(VinPort) * 3.0 * 3.36 * 10 / 1023.0;    // Vin from Controller. 10x Vin to get 10ths value
                                                           // PCB has a divide by 3 amplifier
                                                           // power supply is 3.36, which is max scale voltage

  if ((HWPump || HTPump) && Vin > 10) {   // Vin is 10x, so this is Vin > 1
    Vout = 100;                           // again 10x assumed, for display
    DACsetting = 4095;                    // Highest DAC setting, sends 10v to boiler
    Target = 10.0 * (Tret + 27 * Vin / 100.0);            // calculate target temperature for display purposes
    digitalWrite (LEDPort, LOW);          // turn on green LED
    analogWrite (BacklightPort, 200);     // bright backlight
    if (HWPump) Status = 1;               // Hot Water
    if (HTPump) Status = 2;               // Hot Tub
  }
  else if (HWPump || HTPump) {            // controller has released HW or HT but not turned off pumps yet
    Vout = Vin;
    Target = 10.0 * (Tret + 27 * Vin / 100.0);
    DACsetting = Vout / 100.0 * 4095;      // pass through VIN to VOUT. /10 for Vout, and /10 again for scaling of 10v to max of 4095
  }
  else if (Vin > 1) {                       // floor heating, Vin > 0.1 (or 10*Vin > 1), boiler is ON
    Target = 10.0 * (Tret + 27 * Vin / 100.0);    // calculate target temperature (10x)
    if (Target > 1350) Target = 1350;       // limit max temperature to 135 deg F, remember it's times 10
    Vout = abs(10 * (Target/10 - 74) / 10); // abs() fixes a negative number when Tret is a low value, e.g. 74
    DACsetting = Vout / 100.0 * 4095;       //
    if (DACsetting > 2458) {                // double check limiting Vout DACsetting
      DACsetting = 2458;                    // limit floor heating to 6v, (6/10*4096=2458)
      Vout = 60;
    }
    digitalWrite (LEDPort, LOW);          // turn on green LED
    analogWrite (BacklightPort, 200);     // bright 
    Status = 3;                           // floors
  }
  else {                                  // Vin = 0 so make boiler voltage 0, so boiler is off
    Target = 0;
    Vout = 0;
    DACsetting = 0;
    digitalWrite (LEDPort, HIGH);         // turn off green LED
    analogWrite (BacklightPort, 50);     // dim
    Status = 4;                           // Off
  }

  Wire.beginTransmission(0x61);                     // update D to A
  Wire.write(0x40);
  Wire.write(DACsetting >> 4);
  Wire.write((DACsetting & 15) << 4);
  Wire.endTransmission();

  display_refresh();                                 // updates display

}
