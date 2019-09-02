// https://randomnerdtutorials.com/wifimanager-with-esp8266-autoconnect-custom-parameter-and-manage-your-ssid-and-password/
// https://randomnerdtutorials.com/esp8266-web-server/
// https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer
#include <NixieDriver_ESP.h>
#include <ESP8266WiFi.h>

// https://github.com/tzapu/WiFiManager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

const int dataPin = 12;
const int clockPin = 14;
const int oePin = 16;

const unsigned long displayRefreshInterval = 20;
unsigned long displayRefreshedAt;

nixie_esp nixie(dataPin, clockPin);

// Set web server port number to 80
ESP8266WebServer server(80);

// Variable to store the HTTP request
String header;

// Minify, then replace " with \", and % with %% in CSS, but keep %s in form
const char* indexHtml = "<!DOCTYPE html><html><head> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> <link rel=\"icon\" href=\"data:,\"> <title>Nixie</title> <link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/meyer-reset/2.0/reset.min.css\"/> <link href=\"https://fonts.googleapis.com/css?family=Nixie+One&display=swap\" rel=\"stylesheet\"> <style>*{box-sizing: border-box;}html{background: #ddd; font-family: sans-serif; text-align: center;}div{padding-top: 1.5em; background: #eee; border-top: 3px solid #ccc; border-bottom: 3px solid #ccc;}h1{padding: 2rem; font-size: 2rem; font-weight: 600; font-family: 'Nixie One', sans-serif;}label{display: block; margin-bottom: 0.5rem; margin-left: 1rem; text-align: left;}input[type=text]{width: 100%%; margin-bottom: 1.5rem; padding: .75rem 1rem; border: none; color: #555; font-size: .9rem;}input[type=submit]{cursor: pointer; margin: 1.5em; padding: .5rem 2rem; background: #636363; border: none; color: #fff; font-size: 1rem;}</style></head><body> <h1>Nixie</h1> <form action=\"/update\" method=\"get\"> <div> <label for=\"host\">Host</label> <input type=\"text\" name=\"host\" value=\"%s\" maxlength=\"39\"/> <label for=\"path\">Path</label> <input type=\"text\" name=\"path\" value=\"%s\" maxlength=\"254\"/> </div><input type=\"submit\" value=\"Update\"/> </form></body></html>";

// Generally, host could be up to 254 = 253 max for domain + 1 for null terminator
// https://webmasters.stackexchange.com/questions/16996/maximum-domain-name-length
char api_host[40] = "nocache.aggregator-data.artic.edu";
char api_path[255] = "/api/v1/artworks/search?cache=false&query[range][timestamp][gte]=now-1d";

void setup()
{
  // Uncomment to see WiFiManager data:
  // Serial.setDebugOutput(true);
  Serial.begin(115200);

  // Set initial pin states
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  digitalWrite(dataPin, LOW);
  digitalWrite(clockPin, LOW);

  pinMode(oePin, OUTPUT);
  digitalWrite(oePin, HIGH);

  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  
  // Uncomment and run it once, if you want to erase all the stored information
  //wifiManager.resetSettings();
  
  // Check "Network Connections > Wi-Fi > Properties > TCP/IPv4 > Properties"
  wifiManager.setAPStaticIPConfig(IPAddress(10, 0, 0, 1), IPAddress(10, 0, 0, 1), IPAddress(255, 255, 255, 0));

  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the specified name
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("Nixie");
  
  // if you get here you have connected to the WiFi
  Serial.println("Connected.");

  Serial.println(WiFi.localIP());
  
  // Bind routes
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  
  server.begin();
}

void loop()
{
  server.handleClient();
}

void handleRoot()
{
  char temp[2000];
  snprintf(temp, sizeof(temp), indexHtml, api_host, api_path);
  server.send(200, "text/html", temp);
}

// https://github.com/esp8266/Arduino/blob/e9d052c/libraries/ESP8266WebServer/examples/HelloServer/HelloServer.ino#L24
void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}
