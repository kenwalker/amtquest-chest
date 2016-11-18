#include "AssetTracker.h"
#include "PowerStatus.h"

// Global variables
const int BATTERY_LED_PIN = D0;
int BATTERY_LED_ON = false;
const int CELLULAR_LED_PIN = D1;
int CELLULAR_LED_ON = false;
const int LOCATION_LED_PIN = D2;
int LOCATION_LED_ON = false;
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

	if (should_flash_led && BATTERY_LED_ON) {
		digitalWrite(BATTERY_LED_PIN, LOW);
		BATTERY_LED_ON = false;
	} else {
		digitalWrite(BATTERY_LED_PIN, HIGH);
		BATTERY_LED_ON = true;
	}
}

void setCellularsStatus() {
	Serial.println("Cell Connecting? " + String::format("%d", Cellular.connecting()));
	Serial.println("Cell Ready? " + String::format("%d", Cellular.ready()));
	bool should_flash_led = Cellular.connecting();

	if (should_flash_led && CELLULAR_LED_ON) {
		digitalWrite(CELLULAR_LED_PIN, LOW);
		CELLULAR_LED_ON = false;
	} else {
		digitalWrite(CELLULAR_LED_PIN, HIGH);
		CELLULAR_LED_ON = true;
	}
}

void setLocationStatus() {
	bool should_flash_led = !LOCATION_SENT;

	if (should_flash_led && LOCATION_LED_ON) {
		digitalWrite(LOCATION_LED_PIN, LOW);
		LOCATION_LED_ON = false;
	} else {
		digitalWrite(LOCATION_LED_PIN, HIGH);
		LOCATION_LED_ON = true;
	}
}

Timer batteryLEDTimer(1000, setBatteryStatus);
Timer cellularLEDTimer(1000, setCellularsStatus);
Timer locationTimer(1000, setLocationStatus);

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
	Particle.connect();
  Particle.function("gps", gpsPublish);
}

void loop() {
	gpsLocation.updateGPS();
	if (millis() - lastCheck > 2000) {
		lastCheck = millis();
		if (!LOCATION_SENT && gpsLocation.gpsFix()) {
			Particle.publish("G", gpsLocation.readLatLon(), 60, PRIVATE);
			LOCATION_SENT = true;
		}
	}
}
