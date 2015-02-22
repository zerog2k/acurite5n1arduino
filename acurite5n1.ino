/*****************************************
 accurite 5n1 weather station decoder
  
  for arduino and 433 MHz OOK RX module
  Note: use superhet (with xtal) rx board
  the regen rx boards are too noisy

 Jens Jensen, (c)2015
*****************************************/

// pulse timings
// SYNC
#define SYNC_HI      700
#define SYNC_LO      550

// HIGH == 1
#define LONG_HI      450
#define LONG_LO      350

// SHORT == 0
#define SHORT_HI     250
#define SHORT_LO     150

#define RESETTIME    10000

// other settables
#define PIN           2  // data pin from 433 RX module
#define MAXBITS      65  // max framesize

#define DEBUG         0
#define METRIC_UNITS  0  // select display of metric or imperial units

// sync states
#define RESET     0   // no sync yet
#define INSYNC    1   // sync pulses detected 
#define SYNCDONE  2   // complete sync header received 

volatile unsigned int    pulsecnt = 0; 
volatile unsigned long   risets = 0;     // track rising edge time
volatile unsigned int    syncpulses = 0; // track sync pulses
volatile byte            state = RESET;  
volatile byte            buf[8] = {0,0,0,0,0,0,0,0};  // msg frame buffer
volatile bool            reading = false;            // have valid reading

unsigned int   raincounter = 0; 
// wind directions:
// { "NW", "WSW", "WNW", "W", "NNW", "SW", "N", "SSW",
//   "ENE", "SE", "E", "ESE", "NE", "SSE", "NNE", "S" };
const float winddirections[] = { 315.0, 247.5, 292.5, 270.0, 
                                 337.5, 225.0, 0.0, 202.5,
                                 67.5, 135.0, 90.0, 112.5,
                                 45.0, 157.5, 22.5, 180.0 };

// wx message types
#define  MT_WS_WD_RF  49    // wind speed, wind direction, rainfall
#define  MT_WS_T_RH   56    // wind speed, temp, RH

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); 
  Serial.println(F("Starting Acurite5n1 433 WX Decoder v0.1 ..."));
  attachInterrupt(0, My_ISR, CHANGE);

}

void loop() {
  if (reading)
  {
    // reading found
    noInterrupts();
    if (acurite_crc(buf, sizeof(buf))) {
      // passes crc, good message      
      if (DEBUG) {
        int i;
        for (i=0; i<8; i++) {
          Serial.print(buf[i],HEX);
          Serial.print(" ");
        }
        Serial.println(F("CRC OK"));
      }

      float windspeedkph = getWindSpeed(buf[3], buf[4]);
      Serial.print("windspeed: ");
      if (METRIC_UNITS) {
        Serial.print(windspeedkph, 1);
        Serial.print(" km/h, ");
      } else {
        Serial.print(convKphMph(windspeedkph),1);
        Serial.print(" mph, ");
      }       
      
      int msgtype = (buf[2] & 0x3F);
      if (msgtype == MT_WS_WD_RF) {
        // wind speed, wind direction, rainfall
	float rainfall = 0.00;
	unsigned int curraincounter = getRainfallCounter(buf[5], buf[6]);
	if (raincounter > 0) {
	  // track rainfall difference after first run
	  rainfall = (curraincounter - raincounter) * 0.01;
	}

	// capture starting counter
	raincounter = curraincounter;
        float winddir = getWindDirection(buf[4]);
        Serial.print("wind direction: ");
        Serial.print(winddir, 1);
	Serial.print(", rain gauge: ");
        if (METRIC_UNITS) {
          Serial.print(convInMm(rainfall), 1);
          Serial.print(" mm");
        } else {
          Serial.print(rainfall, 2);
          Serial.print(" inches");
        }
        
      } else if (msgtype == MT_WS_T_RH) {
	// wind speed, temp, RH
        float tempf = getTempF(buf[4], buf[5]);
        int humidity = getHumidity(buf[6]);
	bool batteryok = ((buf[2] & 0x40) >> 6);
    
        Serial.print("temp: ");
        if (METRIC_UNITS) {
          Serial.print(convFC(tempf), 1);
          Serial.print(" C, ");
        } else {
          Serial.print(tempf, 1);
          Serial.print(" F, ");
        }
        Serial.print("humidity: ");
        Serial.print(humidity);
        Serial.print(" %RH, battery: ");
        if (batteryok) {
          Serial.print("OK");
        } else {
          Serial.print("LOW");
        }
      }
      // time
      unsigned int timesincestart = millis()/60/1000;
      Serial.print(", mins since start: ");
      Serial.print(timesincestart);     
      Serial.println();
        
    } else if (DEBUG) {
      // failed CRC
        Serial.println(F("CRC BAD"));
      }
    reading=false;
    interrupts();
  }

  delay(100);
}

bool acurite_crc(volatile byte row[], int cols) {
    	// sum of first n-1 bytes modulo 256 should equal nth byte
    	cols -= 1; // last byte is CRC
        int sum = 0;
    	for (int i = 0; i < cols; i++) {
    	  sum += row[i];
    	}    
    	if (sum != 0 && sum % 256 == row[cols]) {
    	  return true;
    	} else {
    	  return false;
    	}
}

float getTempF(byte hibyte, byte lobyte) {
	// range -40 to 158 F
	int highbits = (hibyte & 0x0F) << 7;
	int lowbits = lobyte & 0x7F;
	int rawtemp = highbits | lowbits;
	float temp = (rawtemp - 400) / 10.0;
	return temp;
}

float getWindSpeed(byte hibyte, byte lobyte) {
	// range: 0 to 159 kph
	int highbits = (hibyte & 0x7F) << 3;
	int lowbits = (lobyte & 0x7F) >> 4;
	float speed = highbits | lowbits;
	// speed in m/s formula according to empirical data
	if (speed > 0) {
		speed = speed * 0.23 + 0.28;
	}
	float kph = speed * 60 * 60 / 1000;
	return kph;
}

float getWindDirection(byte b) {
	// 16 compass points, ccw from (NNW) to 15 (N), 
        // { "NW", "WSW", "WNW", "W", "NNW", "SW", "N", "SSW",
        //   "ENE", "SE", "E", "ESE", "NE", "SSE", "NNE", "S" };
	int direction = b & 0x0F;
	return winddirections[direction];
}

int getHumidity(byte b) {
	// range: 1 to 99 %RH
	int humidity = b & 0x7F;
	return humidity;
}

int getRainfallCounter(byte hibyte, byte lobyte) {
	// range: 0 to 99.99 in, 0.01 increment rolling counter
	int raincounter = ((hibyte & 0x7f) << 7) | (lobyte & 0x7F);
	return raincounter;
}

float convKphMph(float kph) {
  return kph * 0.62137;
}

float convFC(float f) {
  return (f-32) / 1.8;
}

float convInMm(float in) {
  return in * 25.4;
}

void My_ISR()
{
  // decode the pulses
  unsigned long timestamp = micros();
  if (digitalRead(PIN) == HIGH) {
    // going high, start timing
    if (timestamp - risets > RESETTIME) {
      // detect reset condition
      state=RESET;
      syncpulses=0;
      pulsecnt=0;
    }
    risets = timestamp;
    return;
  }
  
  // going low
  unsigned long duration = timestamp - risets;

  if (state == RESET || state == INSYNC) {
    // looking for sync pulses
    if ((SYNC_LO) < duration && duration < (SYNC_HI))  {
      // start counting sync pulses
      state=INSYNC;
      syncpulses++;
      if (syncpulses > 3) {
        // found complete sync header
        state = SYNCDONE;
        syncpulses = 0;
        pulsecnt=0;
      }
      return; 
      
    } else { 
      // not interested, reset  
      syncpulses=0;
      pulsecnt=0;
      state=RESET;
      return; 
    }
  } else {
    // SYNCDONE, now look for message 
    // detect if finished here
    if ( pulsecnt > MAXBITS ) {
      state = RESET;
      pulsecnt = 0;
      reading = true;
      return;
    }
    // stuff buffer with message
    byte bytepos = pulsecnt / 8;
    byte bitpos = 7 - (pulsecnt % 8); // reverse bitorder
    if ( LONG_LO < duration && duration < LONG_HI) {
      bitSet(buf[bytepos], bitpos);
      pulsecnt++;
    }
    else if ( SHORT_LO < duration && duration < SHORT_HI) {
      bitClear(buf[bytepos], bitpos);
      pulsecnt++;
    }
  
  }
}
    
