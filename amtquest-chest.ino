#include "AssetTracker.h"
#include "PowerStatus.h"

// Global variables
const int BATTERY_LOW_VALUE = 40;
bool CONNECTED_MESSAGE_SENT = false;

// Rebroadcase every 30 minutes
const int REBROADCAST_INTERVAL = 7200000;

unsigned long LAST_REBROADCAST_TIME = 0;

const int MAX_LED_BRIGHTNESS = 255;
const int MIN_LED_BRIGHTNESS = 0;

// Use the same LED values for any light flashing
int LED_FADE_AMOUNT = 5;
int LED_BRIGHTNESS = 0;

// Battery LED variables
const int BATTERY_LED_PIN = D0;
int BATTERY_LED_SOLID = false;

// Cellular LED variables
const int CELLULAR_LED_PIN = D1;
int CELLULAR_LED_SOLID = false;

// Location LED variables
const int LOCATION_LED_PIN = D2;
int LOCATION_LED_SOLID = false;

// GPS variables around sending
bool LOCATION_SENT = false;
// When should the TAG be sent after getting a solid GPS signal
// Adding this in because there were some 0's seen if calling
// too quickly after getting a solid signal
unsigned long SEND_TAG_TIME = -1;
// Wait at least 5 seconds after getting a GPS signal
const int SEND_TAG_GAP_TIME = 5000;

unsigned long lastCheck = 0;
char lastStatus[256];

// For checking power state
PowerStatus powerCheck;
// A FuelGauge named for checking on the battery state
FuelGauge fuelGauge;
// GPS Location Tracker
AssetTracker gpsLocation = AssetTracker();

// We want the ability to run our loop BEFORE connecting to the cloud
SYSTEM_MODE(SEMI_AUTOMATIC);

// Actively ask for a GPS reading if you're impatient. Only publishes if there's
// a GPS fix, otherwise returns '0'
int gpsPublish(String command){
    if(gpsLocation.gpsFix()){
        Particle.publish("GPS", locationString(), 60, PRIVATE);

        // uncomment next line if you want a manual publish to reset delay counter
        // lastPublish = millis();
        return 1;
    }
    else { return 0; }
}

// Lets you remotely check the battery status by calling the function "batt"
// Triggers a publish with the info (so subscribe or watch the dashboard)
// and also returns a '1' if there's >10% battery left and a '0' if below
int batteryStatus(String command){
    // Publish the battery voltage and percentage of battery remaining
    // if you want to be really efficient, just report one of these
    // the String::format("%f.2") part gives us a string to publish,
    // but with only 2 decimal points to save space
    Particle.publish("BATT", batteryString(), 60, PRIVATE);
    // if there's more than 10% of the battery left, then return 1
    if(fuelGauge.getSoC()>=BATTERY_LOW_VALUE){ return 1;}
    // if you're running out of battery, return 0
    else { return 0;}
}

/**
 * Create a JSON.parsable string representing the battery and power state.
 * s represents the state of charge of the battery 0.0 to 100.0
 * c represents whether the battery is charging (should be paired with power state)
 * p represents whether the USB cable is plugged in
 * So a couple of examples:
 * {"s":79.44,"c": 0,"p": 0}
 * {"s":81.24,"c": 1,"p": 1}
 */
String batteryString() {
  return
    "{\"s\":" + String::format("%.2f",fuelGauge.getSoC()) +
    ",\"c\": " + powerCheck.getIsCharging() +
    ",\"p\": " + powerCheck.getHasPower() +
    "}";
}

/**
 * Create a JSON.parsable string representing the GPS location.
 * lat represents latitude
 * lon represents the longitude
 * So an example:
 * {"lat":45.372843,"lon":-75.69791979.44}
 */
 String locationString() {
     return
       "{\"lat\":" + String::format("%f",gpsLocation.readLatDeg()) +
       ",\"lon\":" + String::format("%f",gpsLocation.readLonDeg()) +
       "}";
 }

// Send a message indicating starting up
void sendConnectedMessage() {
  Particle.publish("CONN", batteryString(), 60, PRIVATE);
}

Timer batteryLEDTimer(3000, setBatteryStatus);
Timer cellularLEDTimer(3000, setCellularsStatus);
Timer locationTimer(3000, setLocationStatus);
Timer ledFadeTimer(10, setCurrentFadeValue);

void setup() {
	Serial.println("Setup");
	pinMode(BATTERY_LED_PIN, OUTPUT);
  pinMode(CELLULAR_LED_PIN, OUTPUT);
	pinMode(LOCATION_LED_PIN, OUTPUT);
	Serial.begin(9600);
	powerCheck.setup();
	// Sets up all the necessary AssetTracker bits
	gpsLocation.begin();

	// Enable the GPS module. Defaults to off to save power.
	// Takes 1.5s or so because of delays.
	batteryLEDTimer.start();
	cellularLEDTimer.start();
	locationTimer.start();
	ledFadeTimer.start();

  // Startup the GPS
	gpsLocation.gpsOn();

  // Connect to the Particle Cloud - nessesary when you use SYSTEM_MODE(SEMI_AUTOMATIC);
	Particle.connect();

  // Setup the remote APIs
  Particle.function("gps", gpsPublish);
  Particle.function("batt", batteryStatus);
}

void setCurrentFadeValue() {
	LED_BRIGHTNESS = LED_BRIGHTNESS + LED_FADE_AMOUNT;
	if (LED_BRIGHTNESS <= MIN_LED_BRIGHTNESS || LED_BRIGHTNESS >= MAX_LED_BRIGHTNESS) {
		LED_FADE_AMOUNT = -LED_FADE_AMOUNT;
	}
	setLEDBrightness(BATTERY_LED_PIN, BATTERY_LED_SOLID);
	setLEDBrightness(CELLULAR_LED_PIN, CELLULAR_LED_SOLID);
	setLEDBrightness(LOCATION_LED_PIN, LOCATION_LED_SOLID);
}

void setLEDBrightness(int ledPin, bool isSolid) {
	if (isSolid) {
		analogWrite(ledPin, MAX_LED_BRIGHTNESS);
	} else {
		analogWrite(ledPin, LED_BRIGHTNESS);
	}
}

void loop() {
  unsigned long now = millis();
  gpsLocation.updateGPS();

  // Send the battery value when we connect, so we have the data
  if (!CONNECTED_MESSAGE_SENT && Cellular.ready()) {
    sendConnectedMessage();
    CONNECTED_MESSAGE_SENT = true;
  }

	if (now - lastCheck > 2000) {
		lastCheck = now;
		if (!LOCATION_SENT) {
      if (gpsLocation.gpsFix() && SEND_TAG_TIME == -1) {
        SEND_TAG_TIME = now + SEND_TAG_GAP_TIME;
        Serial.println("Setting the SEND_TAG_TIME");
      }
    }
    if (!LOCATION_SENT && SEND_TAG_TIME != -1) {
      if (now > SEND_TAG_TIME) {
        Serial.println("Sending TAG " + locationString());
  			Particle.publish("TAG", locationString(), 60, PRIVATE);
  			LOCATION_SENT = true;
        LAST_REBROADCAST_TIME = millis();
      }
		}
	}
  if (LOCATION_SENT && (millis() - LAST_REBROADCAST_TIME > REBROADCAST_INTERVAL)) {
    LAST_REBROADCAST_TIME = millis();
    gpsPublish("");
    batteryStatus("");
  }
}

void setBatteryStatus() {
	bool should_flash_led = true;
	Serial.println(batteryString());
	if (powerCheck.getHasPower()) {
		// On USB power
		should_flash_led = powerCheck.getIsCharging();
	}  else {
		// Not on power check bettery level
		if (fuelGauge.getSoC() > 50.00f) {
			should_flash_led = false;
		}
	}
	BATTERY_LED_SOLID = !should_flash_led;
}

void setCellularsStatus() {
	Serial.println("Cell Connecting? " + String::format("%d", Cellular.connecting()));
	Serial.println("Cell Ready? " + String::format("%d", Cellular.ready()));
	CELLULAR_LED_SOLID = Cellular.ready();
}

// Yes this seems redundant but may use these two separate state
void setLocationStatus() {
	LOCATION_LED_SOLID = LOCATION_SENT;
}
