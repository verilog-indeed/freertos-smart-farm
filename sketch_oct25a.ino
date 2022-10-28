#include <LiquidCrystal_I2C.h>
#include <WiFi.h>


#define valve 26
#define fan 25
#define waterlvl 32
#define temp 16

#define treshold_temp 50
#define treshold_water 60

#define lcdColumns 20
#define lcdRows 4

bool update_lcd = 1;

//Make sure to change the wifi name and password to match the one you're using at the expo!
const char* ssid     = "ISC-WiFi";
const char* password = "ISCwifiC001";

//static LAN address
IPAddress local_IP(192, 168, 1, 42);
//gateway & subnet defaults
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);

WiFiServer server(80);

int Vtemp, Vwaterlvl, prevtemp = 0, prevwater = 0; // value temp / value water level
bool Sfan, Svalve;     // state fan / state valve

int i = 0;

LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);
void setup() {
  // put your setup code here, to run once:
  pinMode(valve, OUTPUT);
  pinMode(fan, OUTPUT);
  pinMode(waterlvl, INPUT);
  pinMode(temp, INPUT);
  // initialize LCD
  lcd.init();
  // turn on LCD backlight
  lcd.backlight();

  //set fixed LAN IP address
  WiFi.config(local_IP, gateway, subnet);

  xTaskCreate(
    updateLCDTask, //name of function that gets turned into a task
    "LCD refresh", //Task name for debugging
    2048, //stack size in bytes
    NULL, //parameters, ignored
    10, //priority number, higher number means task is more important and will run sooner than less important tasks
    NULL //task handle, ignored
  );
  xTaskCreate(
    updateMeasuresTask,
    "measurment update",
    2048,
    NULL,
    1,
    NULL
  );
  xTaskCreate(
    controlActuatorsTask,
    "actuator update",
    2048,
    NULL,
    5,
    NULL
  );
  xTaskCreate(
    keepWifiAwakeTask,
    "Wifi connection management",
    2048,
    NULL,
    10,
    NULL
  );
  xTaskCreate(
    updateWebpageTask,
    "Server management",
    2048,
    NULL,
    7,
    NULL
  );
}

void loop() {} //in FreeRTOS, usually the loop is left empty

void controlActuatorsTask(void* parameters) {
  while (1) {
    if (Vtemp > treshold_temp) {
      Sfan = true;
    } else {
      Sfan = false;
    }
    if (Vwaterlvl < treshold_water) {
      Svalve = true;
    } else {
      Svalve = false;
    }

    if (Svalve) {
      digitalWrite(valve, HIGH);
    } else {
      digitalWrite(valve, LOW);
    }
    if (Sfan) {
      digitalWrite(fan, HIGH);
    } else {
      digitalWrite(fan, LOW);
    }

    vTaskDelay(100 / portTICK_PERIOD_MS); //sleep task for 100ms
  }
}

void updateMeasuresTask(void* parameters) {
  while (1) {
    prevtemp = Vtemp;
    prevwater = Vwaterlvl;

    // reading input
    Vtemp = analogRead(temp);
    Vwaterlvl = analogRead(waterlvl);
    Vwaterlvl = map(Vwaterlvl, 0, 4095, 0, 100);

    vTaskDelay(100 / portTICK_PERIOD_MS); //sleep task for 100ms
  }
}

void updateLCDTask(void* parameters) {
  //updates LCD contents, sleeps to allow other tasks to run, then repeats
  while (1) {
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("Temperature: ");
    lcd.print(Vtemp);
    lcd.print((char)223);
    lcd.print("C");
    lcd.setCursor(1, 1);
    lcd.print("Water level: ");
    lcd.print(Vwaterlvl);
    vTaskDelay(500 / portTICK_PERIOD_MS); //sleep task for 500ms
  }
}

//Wifi connection/reconnection, check top level constants for wifi name and password!
void keepWifiAwakeTask(void* parameters) {
  while (1) {
    if (WiFi.status() != WL_CONNECTED) {
      //Wifi connection starts here
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(500 / portTICK_PERIOD_MS); //sleep task for 500ms
        Serial.print(".");
      }
      //Wifi connection success
      Serial.println("WiFi connected.");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      server.begin();
      vTaskDelay(500 / portTICK_PERIOD_MS); //sleep task for 500ms
    }
  }
}

void updateWebpageTask(void* parameters) {
  while (1) {
    while (WiFi.status() != WL_CONNECTED) {
      //yield and hope that the wifi task will be back on
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    WiFiClient client = server.available();
    if (client) {
      while (client.connected()) {
        if (client.available()) {             // if there's bytes to read from the client,
          char c = client.read();
          if (c == '\n') {                    // if the byte is a newline character

            // if two newline characters in a row
            // that's the end of the client HTTP request, so send a response:
            c = client.read();
            if (c == '\n') {
              char measurementFormatBuffer[128];
              // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
              // and a content-type so the client knows what's coming, then a blank line:
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println();
              //print current temperature with <br> html tag as newline/linebreak
              sprintf(measurementFormatBuffer, "Temperature: %d°C. <br>", Vtemp);
              client.print(measurementFormatBuffer);
              //print current water level
              sprintf(measurementFormatBuffer, "Water level: %d°C. <br>", Vwaterlvl);
              client.print(measurementFormatBuffer);
              // The HTTP response ends with another blank line:
              client.println();
              // break out of the while loop:
              break;
            }
          }
        }
      }
      client.stop();
    }
    vTaskDelay(500 / portTICK_PERIOD_MS); //sleep task for 500ms
  }
}
