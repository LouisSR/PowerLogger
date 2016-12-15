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

//Global variables
const byte cardDetect = 1; // for SD through SPI, we use digital pin 10 for the SD cs line
const byte record_pin = 5; //Pushbutton to activate the recording
const byte contrast_pin = 6; //To control the contrast of the LCD
const char header[] = "Date, Voltage (mV), Current (mA)";

bool Debug = false; //Debug is true if Serial is connected
bool recording = false;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
unsigned long log_interval = 10000; // Time between two measurements in ms
File logfile; // the logging file

//Objects
RTC_PCF8523 rtc;
Adafruit_INA219 ina219;
Clock log_clock;
Clock measure_clock; //measurement timer
Clock display_clock; //display timer
Clock time_clock; //time (hours and minutes) timer
LiquidCrystal lcd(12, 11, 10, 9, 8, 7);//RS, Enable, D4:7


void setup(void)
{
	pinMode(record_pin, INPUT_PULLUP);
	pinMode(contrast_pin, OUTPUT);
	//digitalWrite(contrast_pin, LOW);
	analogWrite(contrast_pin, 100); //set contrast of LCD
	
	// Init LCD
	lcd.begin(16, 2); //16 Columns, 2 rows
	lcd.print("  PowerLogger  ");
	delay(2000);

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
	SD_setup();
	if(Debug)
	{
		ReadTime(); //Prompt user to input time between measurements	
	}

	//Init INA219
	ina219.begin();

	//Init timers
	measure_clock.begin(200);
	display_clock.begin(500);
	time_clock.begin(10000);
	log_clock.begin(log_interval);

	//Display permanent text
	lcd.clear();
	lcd.setCursor(6, 0); //columns, rows
	lcd.print("mA");
	lcd.setCursor(6, 1); //columns, rows
	lcd.print("V");
	DisplayTime();
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
	pinMode(SS, OUTPUT);

	// see if the card is present and can be initialized:
	if (!SD.begin(SS)) 
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

void loop(void)
{
	static int voltage = 18000, current = -200;

	//Measure current and voltage
	if(measure_clock.isItTime())
	{
		voltage = ina219.getBusVoltage_mV(); //Voltage in mV
		current = ina219.getCurrent_mA(); //Current in mA
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

	if(time_clock.isItTime())
	{
		DisplayTime();
	}

	//Write to SD card
	if(recording && log_clock.isItTime())
	{
		int data[2];
		data[0] = voltage;
		data[1] = current;
		WriteToFile(data, 2);
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

//Display time
void DisplayTime()
{
	char string_t[6]; // sign +  5 digits + \0 char
	DateTime now;
	
	now = rtc.now(); // fetch the time
	
	sprintf(string_t, "%02d:%02d", now.hour(),	now.minute() );

	lcd.setCursor(11, 0);
	lcd.print(string_t);
}

//Ask user to enter time between measurements
void ReadTime(void)
{
	unsigned int start_time;
	Serial.print("Enter time in ms bewteen two logs: ");
	start_time = millis();
	while(Serial.available()==0 && ( millis()-start_time<5000 )); //Timeout 5s
	if(Serial.available()>0)
	{
		log_interval = Serial.parseInt();
	}
	Serial.print(log_interval);
	Serial.println("ms");
}

//Display measurements
void Display(int voltage, int current)
{
	char string_t[7]; // sign +  5 digits + \0 char
	
	//Display current
	if(current >= 10000 || current <= -10000) // value is too long to print
	{
		lcd.setCursor(0, 0); //columns, rows
		//lcd.print("     "); 
		sprintf(string_t, "     ");
	}
	else
	{
		sprintf(string_t, "% 5d", current);
		lcd.setCursor(0, 0); //columns, rows
		lcd.print(string_t);
	}

	if(Debug)
	{
		Serial.print(string_t); Serial.println("mA");
	}

	//Display voltage
	sprintf(string_t, "%3d.%1d", voltage/1000, (abs(voltage)%1000)/100); //sprintf on Arduino does not support float number
	lcd.setCursor(0, 1); //columns, rows
	lcd.print(string_t);

	if(Debug)
	{
		Serial.print(string_t); Serial.println("V");
	}

}

//Read button. If pressed, toggle recording state
void CheckButton(void)
{
	static bool lastButtonState = HIGH;
	bool buttonState = digitalRead(record_pin);
	if(lastButtonState == LOW && buttonState == HIGH)//Low to High transistion
	{
		char string_t[9];

		recording = !recording; //toggle recording state

		if(recording)//Recording is now on 
		{
			if(log_interval<1000)//log_interval is shorter than 1s so we display it in milliseconds
			{
				sprintf(string_t,"REC%3dms",log_interval);
			}
			else //log_interval is longer than 1s so we display it in seconds
			{
				sprintf(string_t," REC%3ds",log_interval/1000);
			}
			
		}
		else // Recording is now off, so we clear recording text
		{
			sprintf(string_t,"        ");
		}
		lcd.setCursor(8, 1); //columns, rows
		lcd.print(string_t);
		
		if(Debug)
		{
			Serial.print("Record: ");
			Serial.println(recording);
			Serial.println(string_t);
		}
	}
	lastButtonState = buttonState;
}
