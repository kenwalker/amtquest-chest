#include "AssetTracker.h"
#include "PowerStatus.h"

// Global variables

const int MAX_LED_BRIGHTNESS = 255;
const int MIN_LED_BRIGHTNESS = 0;

// Use the same LED values for any light flashing
int LED_FADE_AMOUNT = 5;
int LED_BRIGHTNESS = 0;

// Global flashing LED values

// Battery LED variables
const int BATTERY_LED_PIN = D0;
int BATTERY_LED_SOLID = false;

// Cellular LED variables
const int CELLULAR_LED_PIN = D1;
int CELLULAR_LED_SOLID = false;

const int LOCATION_LED_PIN = D2;
int LOCATION_LED_SOLID = false;
bool LOCATION_SENT = false;
bool LOCATION_AVAILABLE = false;

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

Timer batteryLEDTimer(1000, setBatteryStatus);
Timer cellularLEDTimer(1000, setCellularsStatus);
Timer locationTimer(1000, setLocationStatus);
Timer ledFadeTimer(10, setCurrentFadeValue);

// Actively ask for a GPS reading if you're impatient. Only publishes if there's
// a GPS fix, otherwise returns '0'
int gpsPublish(String command){
    if(gpsLocation.gpsFix()){
        Particle.publish("G", gpsLocation.readLatLon(), 60, PRIVATE);

        // uncomment next line if you want a manual publish to reset delay counter
        // lastPublish = millis();
        return 1;
    }
    else { return 0; }
}

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
	gpsLocation.gpsOn();
	batteryLEDTimer.start();
	cellularLEDTimer.start();
	locationTimer.start();
	ledFadeTimer.start();

	Particle.connect();
  Particle.function("gps", gpsPublish);
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
	gpsLocation.updateGPS();

	if (millis() - lastCheck > 2000) {
		lastCheck = millis();
		if (!LOCATION_SENT && gpsLocation.gpsFix()) {
			// Important: You can only send the publish messages as part of something
			// in or invocated from the loop()
			Particle.publish("G", gpsLocation.readLatLon(), 60, PRIVATE);
			LOCATION_SENT = true;
		}
	}
}

void setBatteryStatus() {
	bool should_flash_led = true;
	Serial.println("Charging? " + String::format("%d", powerCheck.getIsCharging()));
	Serial.println("Battery Level: " + String::format("%.2f",fuelGauge.getSoC()) + "%");
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
	CELLULAR_LED_SOLID = !Cellular.connecting();
}

void setLocationStatus() {
	bool LOCATION_LED_SOLID = !LOCATION_SENT;
}
