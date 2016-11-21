/*

Power Logger by Louis Saint-Raymond - 2016

Record current and voltage on uSD card with:
- PCF8523 RealTime Clock (I2C)
- INA219 current and voltage sensor (I2C)
- uSD card ()
- LCD (4-wires parallel)

TODO:
- Add LCD: log_interval and recording status when recording
- Add pushbutton: modify recording, open/close file
*/

#include <LiquidCrystal.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_INA219.h>
#include <SPI.h>
#include <SD.h>
#include "Clock.h"

//Global variabless
const int chipSelect = 10; // for SD through SPI, we use digital pin 10 for the SD cs line
const char header[] = "Date, Voltage (mV), Current (mA)";

bool Debug = false; //Debug is true if Serial is connected
bool recording = true;
byte record_pin = 1; //Pushbutton to activate the recording
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
unsigned long log_interval = 2000; // Time between two measurements in ms
File logfile; // the logging file

//Objects
RTC_PCF8523 rtc;
Adafruit_INA219 ina219;
Clock log_clock;
Clock measure_clock;
Clock display_clock;
LiquidCrystal lcd(9, 8, 4, 5, 6, 7);//RS, Enable, D4:7


void setup(void)
{
	pinMode(record_pin, INPUT_PULLUP);
	delay(3000);
	
	// Init LCD
	lcd.begin(16, 2); //16 Columns, 2 rows
	lcd.print("PowerLogger");
	lcd.setCursor(5, 0); //columns, rows
	lcd.print("mA");
	lcd.setCursor(5, 1); //columns, rows
	lcd.print("V");

	// Init serial port
	if(Serial)
	{
		Debug = true; //Enable Debug
		if(Debug)
		{
			Serial.begin(115200);
			Serial.println("\n\nPowerLogger\n");
		}
	}

	//Init RTC
	if (! rtc.begin()) 
	{
		if(Debug)
		{
			Serial.println("RTC: Couldn't find RTC");
		}
		while (1);
	}

	//Init SD
	//SD_setup();
	if(Debug)
	{
		ReadTime(); //Prompt user to input time between measurements	
	}

	//Init INA219
	//ina219.begin();

	//Init timers
	measure_clock.begin(100);
	display_clock.begin(500);
	log_clock.begin(log_interval);

}

/* Initialize SD card, create new log file and save header*/
void SD_setup(void)
{
	
	if(Debug)
	{
		Serial.print("Initializing SD card...");
	}
	// make sure that the default chip select pin is set to
	// output, even if you don't use it:
	pinMode(chipSelect, OUTPUT);

	// see if the card is present and can be initialized:
	if (!SD.begin(chipSelect)) 
	{
		Serial.println("SD: Card failed, or not present");
	}
	if(Debug)
	{
		Serial.println("card initialized.");
	}
	// create a new file
	char filename[] = "Logger00.CSV";
	DateTime now;
	now = rtc.now();
	// 151224 : YYMMDD
	filename[0] = (now.year() - 2000)/10 + '0';
	filename[1] = (now.year() - 2000)%10 + '0';
	filename[2] = now.month()/10 + '0';
	filename[3] = now.month()%10 + '0';
	filename[4] = now.day()/10 + '0';
	filename[5] = now.day()%10 + '0';

	for (uint8_t i = 0; i < 100; i++) 
	{
		filename[6] = i/10 + '0';
		filename[7] = i%10 + '0';

		if (! SD.exists(filename)) 
		{
			// only open a new file if it doesn't exist
			logfile = SD.open(filename, FILE_WRITE); 
			break;  // leave the loop!
		}
	}
	
	if (logfile)
	{
		logfile.println(header);  //Header for the log file

		if(Debug)
		{
			Serial.print("Filename: ");
			Serial.println(filename);
		}
	}
	else
	{
		if(Debug)
		{
			Serial.println("SD: Couldnt create file");
		}
	}
}


void CheckButton(void)
{
	static bool lastButtonState = LOW;
	bool buttonState = digitalRead(record_pin);
	if(lastButtonState != buttonState)//Button state has changed
	{
		char string_t[7];
		if(buttonState)//Recording is now on 
		{
			recording = true;
			if(log_interval<1000)//log_interval is shorter than 1s so we display it in milliseconds
			{
				sprintf(string_t,"R%3dms",log_interval);
			}
			else if(log_interval<60000) //log_interval is shorter than 1min so we display it in seconds
			{
				sprintf(string_t,"R%2d.%1ds",log_interval/1000, (log_interval%1000)/100);
			}
			else //log_interval is longer than 1min so we display it in minutes
			{
				sprintf(string_t,"R%2d.%1dm",log_interval/60000, (log_interval%60000)/6000);
			}
			
		}
		else // Recording is now off
		{
			recording = false;
			sprintf(string_t,"      ");
		}
		lcd.setCursor(10, 0); //columns, rows
		lcd.print(string_t);
		lastButtonState = buttonState;
	}
}

void loop(void)
{
	static int voltage = -18000, current = -1234;

	//Measure current and voltage
	if(measure_clock.isItTime())
	{
		//voltage = ina219.getBusVoltage_mV(); //Voltage in mV
		//current = ina219.getCurrent_mA(); //Current in mA
		if(Debug)
		{
			Serial.print("Voltage: "); Serial.print(voltage); Serial.println(" mV");
			Serial.print("Current: "); Serial.print(current); Serial.println(" mA");
		}
	}


	if(display_clock.isItTime())
	{
		Display(voltage, current);
	}

	//Write to SD card
	if(recording && log_clock.isItTime())
	{
		int data[2];
		data[0] = voltage;
		data[1] = current;
		//WriteToFile(data, 2);
	}
	CheckButton();
	delay(100);
}

/* 
Write time+data to the log file
data : array of values to write
data_length : length of the array
*/
void WriteToFile(int* data, byte data_length)
{
	DateTime now;

	// fetch the time
	now = rtc.now();

	// log time
	logfile.print('"');
	logfile.print(now.year(), DEC);
	logfile.print("/");
	logfile.print(now.month(), DEC);
	logfile.print("/");
	logfile.print(now.day(), DEC);
	logfile.print(" ");
	logfile.print(now.hour(), DEC);
	logfile.print(":");
	logfile.print(now.minute(), DEC);
	logfile.print(":");
	logfile.print(now.second(), DEC);
	logfile.print('"');
	logfile.print(", ");

	for(byte i=0; i<data_length; i++)
	{
		logfile.print(data[i]);
		logfile.print(", ");
	}
	logfile.println(); //end of line

	//sync data to the card & update FAT!
	logfile.flush();
}

//Ask user to enter time between measurements
void ReadTime(void)
{
	unsigned int start_time;
	Serial.print("Enter time in ms bewteen two logs: ");
	start_time = millis();
	while(Serial.available()==0 && ( millis()-start_time<2000 )); //Timeout 2s
	if(Serial.available()>0)
	{
		log_interval = Serial.read();
	}
	Serial.print(log_interval);
	Serial.println("ms");
}

void Display(int voltage, int current)
{
	char string_t[7]; // sign +  5 digits + \0 char
	
	//Display current
	sprintf(string_t, "% 5d", current);
	lcd.setCursor(0, 0); //columns, rows
	lcd.print(string_t);
	if(Debug)
	{
		Serial.print(string_t); Serial.println("mA");
	}
	
	//Display voltage
	sprintf(string_t, "% 2d.%1d", voltage/1000, (abs(voltage)%1000)/100); //sprintf on Arduino does not support float number
	lcd.setCursor(0, 1); //columns, rows
	lcd.print(string_t);
	if(Debug)
	{
		Serial.print(string_t); Serial.println("V");
	}
}
