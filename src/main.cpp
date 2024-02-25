/*
   hawa-lc4 27.02.2022 (SysVer = 1.51s)
   Board: Arduino Nano V3.0
*/
char SysVer[] = "1.51s";                    // System-Version; immer anpassen!!!
/*
   Soladin 600 Monitor
   Programm zur Abfrage und Anzeige der Betriebsdaten der Soladin 600 Solar-Einspeisewechselrichter.
   Anzeige auf TFT Display 1,8" mit 128x160 Pixel über SPI Schnittstelle;
     Querformat: 20 Zeichen bei 8 Zeilen; Zeichen: b:8, h:16 Pixel

   Diese Variante ausschließlich zur Darstellung der Daten eines WR.

   Der erweiterte DoDpClr zum löschen des BS ist nur beim Setup erforderlich;
    und auch nur für bestimmte Displays mit Pixel-Versatz.

  13.04.2019:
  - Umbau auf HW-SPI
  - Änderung der Ausgaben seriell und am Display zu "F()" um RAM zu sparen: perfekt gelungen!

  27.02.2022:
   - Vertauschen der Kanäle für meine neue Einspeisung "Klein-PV".


   Dokumentationen:
   Zum Display:
   - https://github.com/Bodmer/TFT_ST7735/issues/1   Pixel-Versatz am Rand von Display-a
   - TFT-Display-a_1.8Z-ST7735.odt  bzw.  TFT-Display-b_1.8Z-ST7735.odt
   - zum Font: https://github.com/olikraus/ucglib/wiki/fontgroupunifont
     bzw. https://github.com/olikraus/ucglib/wiki/fontgroupx11

   Zum Soladin 600:
   - readme.txt in  Arduino\libraries\SolaDin-master (Achtung auf Korrektur der Pin-Bezeichnung)
   - Kommandos:
  Serial.println("f - Firmware");
  Serial.println("r - Read Max power");
  Serial.println("o - Reset Max power");
  Serial.println("h - Read hystory data");
  Serial.println("d - Read device status");
  Serial.println("x - Write RXbuffer");  // ist hier nicht sinnvoll einsetzbar, wurde entfernt
  -- zusätzlich und modifiziert für Daueranzeige ist der "Read device status"
*/


/*
   Definition der Variablen und Namen/Ports für I/O
*/
// includes:
#include <Arduino.h>
#include "Soladin.h"
#include <SoftwareSerial.h>
#include <SPI.h>
#include <Ucglib.h>


// Belegung der analogen und digitalen I/O-PINs:
//
// A0 = ?                         // Input: Abfrage Taster History-Data
// A1 = ?                         //
// const byte XXXX = 2;           // Meßeingang A2
// const byte YYYY = 3;           // Meßeingang A3
// A4 und A5 bleiben reserviert für mögliche I2C Erweiterungen
// A6 = ?                         //
// A7 = ?                         //
//
// D0 und D1 bleiben frei für USB-Schnittstelle (Rx/Tx)
// D2 = ?                         //
// D3 = ? (PWM)                   //
// D4 = ?                         // Soladin-1 Tx-data
// D5 = ? (PWM)                   // Soladin-1 Rx-data
// D6 = ? (PWM)                   //
// D7 = ?                         //
// D8 = ?                         // Display Chip select
// D9 = ? (PWM)                   // Display Command Data
const byte DispLED = 10;          // (PWM) Display LED
// D11 = ? (PWM)                  // Display SPI data signal
const byte HDQ = 12;              // Input: Abfrage Taster History-Data
// D13 = ?  (Achtung: LED an GND) // Display SPI clock signal

// Definition von Variablen und Initialisierung:
boolean DoDpClr = true;           //Do Display Clear: setzen je nach Display-Typ
boolean connect = false;          // if soladin responds
unsigned long prevMil = 0;        // speichert die Zeit des letzten Durchlaufs
unsigned long prevDsip = millis(); // speichert die Zeit des letzten Tatsendrucks
const long interval1 = 10000;     // Anzeigeinterval in Millisekunden (10 Sekunden
const long interval2 = 1800000;   // Display-LED in Millisekunden (30 Minuten)
boolean normalDS = true;          // normale Daten-Anzeige oder History Daten? (Taster-Abfrage)
const int Ln1 = 14;               // Pixel-Position der 1. Zeile; und so weiter....
const int Ln2 = 30;
const int Ln3 = 46;
const int Ln4 = 62;
const int Ln5 = 78;
const int Ln6 = 94;
const int Ln7 = 110;
const int Ln8 = 126;
int Lnx = 0;                      // Hilsvariable zur Zeilenberechnung in HistoryData

//Ucglib_ST7735_18x128x160_HWSPI ucg(/*cd=*/ 9, /*cs=*/ 10, /*reset=*/ 8);
Ucglib_ST7735_18x128x160_HWSPI ucg(/*cd=*/ 9, /*cs=*/ 8, /*reset=*/ -1);
// Reset könnte man sich auch sparen und den Pin vom Display auf den Reset des Arduino legen.
// /*reset=*/ -1    das geht genauso.

SoftwareSerial solcom(5, 4);      // serial to conect to Soladin RX=5 TX=4
Soladin sol;                      // copy of soladin class

/*
   Aufbereitung der Daten für die Ausgabe über die serielle Schnittstelle
*/
void SPrintCmd(int cmd) {
  Serial.print(F("Cmd: "));
  switch (cmd) {
    case 1: Serial.println(F("Probe")); break;
    case 2: Serial.println(F("Devstate")); break;
    case 3: Serial.println(F("Firmware")); break;
    case 4: Serial.println(F("MaxPower")); break;
    case 5: Serial.println(F("Reset MaxPower")); break;
    case 6: Serial.println(F("History Data")); break;
  }
}

void SPrintDS() {
  Serial.print(F("PV= "));
  Serial.print(float(sol.PVvolt) / 10);
  Serial.print(F("V;   "));
  Serial.print(float(sol.PVamp) / 100);
  Serial.println(F("A"));

  Serial.print(F("AC= "));
  Serial.print(sol.Gridpower);
  Serial.print(F("W;  "));
  Serial.print(float(sol.Gridfreq) / 100);
  Serial.print(F("Hz;  "));
  Serial.print(sol.Gridvolt);
  Serial.println(F("Volt"));

  Serial.print(F("Device Temperature= "));
  Serial.print(sol.DeviceTemp);
  Serial.println(F("° Celcius"));

  Serial.print(F("AlarmFlag= "));
  Serial.println(sol.Flag, BIN);

  Serial.print(F("Total Power= "));
  Serial.print(float(sol.Totalpower) / 100);
  Serial.println(F("kWh"));
  // I really don't know, wy i must split the sprintf ?
  Serial.print(F("Total Operating Time= "));
  char timeStr[14];
  sprintf(timeStr, "%04d:", (sol.TotalOperaTime / 60));
  Serial.print(timeStr);
  sprintf(timeStr, "%02d hh:mm ",  (sol.TotalOperaTime % 60));
  Serial.println(timeStr);
  Serial.println();
}

void SPrintFW() {
  Serial.print(F("FW ID= "));
  Serial.println(byte(sol.FW_ID), HEX);
  Serial.print(F("Ver= "));
  Serial.println(word(sol.FW_version), HEX);
  Serial.print(F("Date= "));
  Serial.println(word(sol.FW_date), HEX);
  Serial.println();
}

void SPrintMP() {
  Serial.print(F("MaxPower= "));
  Serial.print(word(sol.MaxPower));
  Serial.println(F(" Watt"));
  Serial.println();
}

void SPrintHD() {
  Serial.print(F("Operation Time= "));
  char timeStr[14];
  //sprintf(timeStr, "%02d:%02d hh:mm ",(sol.DailyOpTm/12), ((sol.DailyOpTm*5)%12)); // Original
  sprintf(timeStr, "%02d:%02d hh:mm ", (sol.DailyOpTm / 12), ((sol.DailyOpTm % 12) * 5)); // so stimmt's
  Serial.print(timeStr);
  Serial.print(float(sol.Gridoutput) / 100);
  Serial.println(F("kWh"));
}


void SPrintflag() {
  if ( sol.Flag & 0x0001 )Serial.println(F("Usolar too high"));
  if ( sol.Flag & 0x0002 )Serial.println(F("Usolar too low"));
  if ( sol.Flag & 0x0004 )Serial.println(F("No Grid"));
  if ( sol.Flag & 0x0008 )Serial.println(F("Uac too high"));
  if ( sol.Flag & 0x0010 )Serial.println(F("Uac too low"));
  if ( sol.Flag & 0x0020 )Serial.println(F("Fac too high"));
  if ( sol.Flag & 0x0040 )Serial.println(F("Fac too low"));
  if ( sol.Flag & 0x0080 )Serial.println(F("Temperature to high"));
  if ( sol.Flag & 0x0100 )Serial.println(F("Hardware failure"));
  if ( sol.Flag & 0x0200 )Serial.println(F("Starting"));
  if ( sol.Flag & 0x0400 )Serial.println(F("Max Power"));
  if ( sol.Flag & 0x0800 )Serial.println(F("Max current"));
}
/*
  void SPrintbuffer(){
  Serial.print("0x");
  for (int i=0; i<sol.RxLgth ; i++) {
    if (byte(sol.RxBuf[i]) < 0x10) {
      Serial.print("0");
    }
    Serial.print(byte(sol.RxBuf[i]),HEX);
    Serial.print(" ");
  }
  Serial.println();
  Serial.print("BufferSize= ");
  Serial.println(sol.RxLgth);
  Serial.println();
  }
*/


/*
   Aufbereitung der Daten für die Ausgabe am Display
   Version zur Darstellung der Daten nur eines WR
*/
void DisplHD(int j, int Lnx) {
  ucg.setPrintPos(1, Lnx);
  ucg.print(F("-"));
  ucg.print(j);
  ucg.print(F(": "));
  char timeStr[14];
  sprintf(timeStr, "%02d:%02d  ", (sol.DailyOpTm / 12), ((sol.DailyOpTm % 12) * 5)); // so stimmt's
  ucg.print(timeStr);
  ucg.print(float(sol.Gridoutput) / 100, 1);
  ucg.print(F("kWh      "));
}


void DisplDS() {
  ucg.setPrintPos(1, Ln2);
  ucg.print(F("PV: "));
  ucg.print(int((sol.PVvolt) / 10));
  ucg.print(F("V*"));
  ucg.print(float(sol.PVamp) / 100, 1);
  ucg.print(F("A= "));
  int PVa1 = (sol.PVamp);
  ucg.print(word(((sol.PVvolt) / 10)*PVa1 / 100));
  ucg.print(F("W    "));

  ucg.setPrintPos(1, Ln3);
  ucg.print(F("AC: "));
  ucg.print(sol.Gridpower);
  ucg.print(F("W   "));
  ucg.print(sol.Gridvolt);
  ucg.print(F("V    "));

  ucg.setPrintPos(1, Ln4);
  ucg.print(F("Temperatur: "));
  ucg.print(sol.DeviceTemp);
//  ucg.print(" Cels.");   // Achtung: Font
  ucg.print((char)176);    // Achtung: Font; Zeichen ° funktioniert nicht
  ucg.print(F("C "));

  ucg.setPrintPos(1, Ln5);
  ucg.print(F("Energie: "));
  ucg.print(float(sol.Totalpower) / 100, 0);
  ucg.print(F("kWh"));

  ucg.setPrintPos(1, Ln6);
  ucg.print(F("OnTime: "));
  char timeStr[14];
  sprintf(timeStr, "%04dh ", (sol.TotalOperaTime / 60));
  ucg.print(timeStr);
}


void DisplFlag() {          // Fehler!: es können mehrere Flags gleichzeitig auftreten; angezeigt wird immer nur das letzte
  ucg.setPrintPos(1, Ln7);
  if ( sol.Flag & 0x0001 ){ucg.print(F("! Usolar zu hoch    ")); ucg.setPrintPos(1, Ln8); delay(500);}
  if ( sol.Flag & 0x0002 ){ucg.print(F("! Usolar zu niedrig ")); ucg.setPrintPos(1, Ln8); delay(500);}
  if ( sol.Flag & 0x0004 ){ucg.print(F("! Kein Netz         ")); ucg.setPrintPos(1, Ln8); delay(500);}
  if ( sol.Flag & 0x0008 ){ucg.print(F("! Uac zu hoch       ")); ucg.setPrintPos(1, Ln8); delay(500);}
  if ( sol.Flag & 0x0010 ){ucg.print(F("! Uac zu niedrig    ")); ucg.setPrintPos(1, Ln8); delay(500);}
  if ( sol.Flag & 0x0020 ){ucg.print(F("! Fac zu hoch       ")); ucg.setPrintPos(1, Ln8); delay(500);}
  if ( sol.Flag & 0x0040 ){ucg.print(F("! Fac zu niedrig    ")); ucg.setPrintPos(1, Ln8); delay(500);}
  if ( sol.Flag & 0x0080 ){ucg.print(F("! Temperatur zu hoch")); ucg.setPrintPos(1, Ln8); delay(500);}
  if ( sol.Flag & 0x0100 ){ucg.print(F("! Hardware Fehler   ")); ucg.setPrintPos(1, Ln8); delay(500);}
  if ( sol.Flag & 0x0200 ){ucg.print(F("! Start-Phase       ")); ucg.setPrintPos(1, Ln8); delay(500);}
  if ( sol.Flag & 0x0400 ){ucg.print(F("! Max. Leistung     ")); ucg.setPrintPos(1, Ln8); delay(500);}
  if ( sol.Flag & 0x0800 ){ucg.print(F("! Max. Strom        ")); ucg.setPrintPos(1, Ln8); delay(500);}
}


/*
   EIGENE ROUTINEN
*/
void SolCon() {
  while (connect) {
    if (sol.query(PRB)) {
      break;
    } else {
      connect = false;
      prevMil = 0;
      loop();
    }
  }
  while (!connect) {                  // Try to connect to Soladin
    ucg.setPrintPos(1, Ln3);
    ucg.print("Verbindungsaufbau:  ");
    Serial.print("Cmd: Probe");
    ucg.setPrintPos(1, Ln4);
    ucg.print("                     ");
    ucg.setPrintPos(1, Ln4);
    for (int i = 0 ; i < 8 ; i++) {
      if (sol.query(PRB)) {          // Try connecting to slave
        connect = true;
        ucg.setPrintPos(1, Ln4);
        ucg.print(" erfolgreich");
        Serial.println("...Connected");
        break;
      }
      ucg.print(".");
      Serial.print(".");
      if (i == 7) {
        ucg.print(" Pause (30s)");
        Serial.println(" sleeping");
        delay(30000);              // Pause für 30 Sekunden da WR nicht erreichbar
      }
      delay(1000);
    }
    break;
  }
  delay(200);
}

void doFW() {
  if (connect)  {               // already connected
    SPrintCmd(3);
    for (int i = 0 ; i < 4 ; i++) {     // give me some time to return by hand
      if (sol.query(FWI)) {       // request firware information
        SPrintFW();
        break;
      }
    }
  }
}


void doMP() {
  if (connect ) {
    SPrintCmd(4);
    for (int i = 0 ; i < 4 ; i++) {
      if (sol.query(RMP)) {       // request maximum power so far
        SPrintMP();
        break;
      }
    }
  }
}

void doDS1() {
  // diese Routine mit ausführlichen Daten nur für Ausgabe über serielle Schnittstelle
  if (connect) {
    SPrintCmd(2);
    for (int i = 0 ; i < 4 ; i++) {
      if (sol.query(DVS)) {       // request Device status
        SPrintDS();
        if (sol.Flag != 0x00) {       // Print error flags
          SPrintflag();
        }
        break;
      }
    }
  }
}


void doDS2() {
  if (connect) {
    for (int i = 0 ; i < 4 ; i++) {
      if (sol.query(DVS)) {       // request Device status
        DisplDS();
        if (sol.Flag != 0x00) {       // Print error flags
          ucg.setColor(255, 0, 255);    // Zeichenfarbe magenta für Alarm-Flags
          DisplFlag();
          ucg.setColor(255, 255, 255);    // Zeichenfarbe weiß
        }
        break;
      }
    }
  }
}


void doRMP() {
  if (connect) {
    SPrintCmd(5);
    sol.query(ZMP);         // Reset Maximum power register
  }
}


void doHD1() {
  if (connect) {
    SPrintCmd(6);
    for (int i = 0 ; i < 10 ; i++) {    // loop this for the last 10 days
      if (sol.query(HSD, i)) {          // Read history data
        SPrintHD();
      }
    }
    Serial.println();
  }
}


void doHD2() {
  if (connect) {
    ucg.setPrintPos(1, Ln2);
    ucg.print("Daten Historie");
    ucg.setPrintPos(1, Ln3);
    ucg.print("Beginn heute: ");
    for (int i = 0 ; i < 10 ; i++) {    // loop this for the last 10 days
      if (sol.query(HSD, i)) {          // Read history data
        if (i < 5) {
          Lnx = i * 16;
          Lnx += Ln4;
        } else {
          Lnx = i - 5;
          Lnx *=  16;
          Lnx += Ln4;
        }
        DisplHD(i, Lnx);
      }
      if (i == 4) delay(5000);
    }
    delay(5000);
  }
}


/*
   Los geht's mit code:
*/

void setup() {
  // put your setup code here, to run once:
  // initialize serial communications at 9600 bps:
  Serial.begin(9600);
  Serial.println();
  Serial.println(F("Soladin 600 Monitor"));
  Serial.print(F("## Sys.Version: "));
  Serial.println(SysVer);
  Serial.println(F("Setup..."));
  solcom.begin(9600);
  sol.begin(&solcom);

  // initialisiere PINs:
  pinMode(HDQ, INPUT_PULLUP);
  pinMode(DispLED, OUTPUT);
  analogWrite(DispLED, 196);

  // initialize Display:
  ucg.begin(UCG_FONT_MODE_SOLID); // damit ist überschreiben von Zeichen möglich
  //  ucg.setFont(ucg_font_unifont_mr);   // Jede Zeile beginnt bei Pixel 0; kein °C!
  ucg.setFont(ucg_font_8x13_mf);        // Jede Zeile beginnt bei Pixel 1; hat mehr Zeichen!

  if (DoDpClr != 0 ) {
    ucg.undoRotate(); ucg.clearScreen();
    ucg.setRotate90(); ucg.clearScreen();
    ucg.setRotate180(); ucg.clearScreen();
    ucg.setRotate270(); ucg.clearScreen();
  } else {
    ucg.clearScreen();
  }

  ucg.setRotate270();             // Querformat
  ucg.setColor(1, 0, 0, 0);       // Hintergrund schwarz
  ucg.setPrintPos(1, Ln1);
  ucg.setColor(0, 200, 255);      // Zeichenfarbe cyan für Überschrift
  ucg.print(F("Soladin 600  Monitor"));   // Überschrift
  ucg.drawHLine(0, 15, 160);           // unterstrichen
  ucg.setColor(255, 255, 255);    // Zeichenfarbe weiß
  ucg.setPrintPos(1, Ln2);
  ucg.print(F("Sys.Version: "));
  ucg.print(SysVer);

  Serial.println(F("------menu------------"));
  Serial.println(F("f - Firmware"));
  Serial.println(F("r - Read Max power"));
  Serial.println(F("o - Reset Max power"));
  Serial.println(F("h - Read hystory data"));
  Serial.println(F("d - Read device status"));
  Serial.println(F("s - goto Setup"));

  SolCon();

  ucg.setPrintPos(1, Ln6);
  ucg.print(F("Soladin Firmware:"));
  ucg.setPrintPos(1, Ln7);
  ucg.print(F("Version = "));
  (sol.query(FWI));
  ucg.print(word(sol.FW_version), HEX);

  ucg.setPrintPos(1, Ln8);
  ucg.print(F("Setup ..... done."));
  delay(5000);
  Serial.println(F("...done"));
}   // end setup

/*
  HAUPT-ROUTINE
*/

void loop() {
  unsigned long curMil = millis();
  unsigned long curDisp = millis();
  if (curMil < prevMil) prevMil = 0;
  if (curMil - prevMil >= interval1) {
    prevMil = curMil;
    ucg.clearScreen();
    ucg.setPrintPos(1, Ln1);
    ucg.setColor(0, 200, 255);      // Zeichenfarbe cyan für Überschrift
    ucg.print("Soladin 600  Monitor");   // Überschrift
    ucg.drawHLine(0, 15, 160);           // unterstrichen
    ucg.setColor(255, 255, 255);    // Zeichenfarbe weiß
    if (!normalDS) {
      doHD2();
      normalDS = true;
    } else {
      doDS2();
    }
  }

  if (curDisp - prevDsip >= interval2) analogWrite(DispLED, 16);

  SolCon();
  if (!digitalRead(HDQ)) {              // Taster wurde gedrückt (=GND)
    normalDS = false;
    analogWrite(DispLED, 196);
    ucg.setColor(255, 0, 0);           // Zeichenfarbe rot
    ucg.setPrintPos(145, Ln8);
    ucg.print("??");
    ucg.setColor(255, 255, 255);       // Zeichenfarbe weiß
    prevDsip = millis();
  }

  if (Serial.available() > 0) {       // read serial comands
    char incomingByte = Serial.read();
    switch (incomingByte) {
      case 'f': doFW(); break;            // read firmware
      case 'r': doMP(); break;            // read max power
      case 'd': doDS1(); break;           // read device status
      case 'o': doRMP(); break;           // reset max power
      case 'h': doHD1(); break;           // read historical data
      case 's': setup(); break;           // Software Reset
      default:
        Serial.println("------menu------------");
        Serial.println("f - Firmware");
        Serial.println("r - Read Max power");
        Serial.println("o - Reset Max power");
        Serial.println("h - Read hystory data");
        Serial.println("d - Read device status");
        Serial.println("s - goto Setup");
    }
  }
}   // end loop

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Universal uC Color Graphics Library

  Copyright (c) 2014, olikraus@gmail.com
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification,
               are permitted provided that the following conditions are met:

               Redistributions of source code must retain the above copyright notice, this list
               of conditions and the following disclaimer.

               Redistributions in binary form must reproduce the above copyright notice, this
               list of conditions and the following disclaimer in the documentation and / or other
               materials provided with the distribution.

               THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
               CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
               INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
               MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
               DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
               CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
               SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
                   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
                   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
               CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
               STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
               ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
               ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
