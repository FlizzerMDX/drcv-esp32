#include <WiFi.h>
#include "time.h"

#include <HTTPClient.h>
//#include "esp_eap_client.h"
#include "sdkconfig.h"
#if CONFIG_ESP_WIFI_REMOTE_ENABLED
#error "WPA-Enterprise is only supported in SoCs with native Wi-Fi support"
#endif
#include <WiFiClientSecure.h>
#include "secrets.h"  // Include the secrets.h file for WiFi credentials

#include "xml_parser.h" // Include my xml parser method
#include "split.h" // Include my Split method
#include "event.h" // Include Event custom class
#include "datetime.h" // Include DateTime custom class

#include <Base64.h>
#include <ArduinoHttpClient.h>

#include <SPI.h>
//#include <WiFi101.h>

#include <Arduino.h>        // Include the core library for Arduino platform development
#include "EPD.h"            // Include the EPD library to control the E-Paper Display
//#include "EPD_GUI.h"        // Include the EPD_GUI library which provides GUI functionalities
#include "pic_scenario.h"   // Include the header file containing image data
//#include "INN011_header.h"   // Include the header file containing image data
//#include "refresh_icon.h"

// Define button pins
#define BUTTON_DOWN 1
#define BUTTON_UP 2
#define BUTTON_SCROLL_UP 6
#define BUTTON_SCROLL_DOWN 4
#define BUTTON_SCROLL_CLICK 5

bool boutton_clicked = false;
bool manualRefresh = false;

int counter = 0;

bool firstLaunch = true;
int autoRefreshMinutes = 60;
const int MIN_HOUR_REFRESH = 5;
const int MAX_HOUR_REFRESH = 22;

// NTP Server settings
const char* ntpServer = "pool.ntp.org";
unsigned long lastUpdateTime = 0;  // The timestamp of the last update time

// Adjust for your timezone
const long gmtOffset_sec = 7200;
const int daylightOffset_sec = 0;

struct tm timeinfo;

DateTime* current_date = new DateTime("2025-05-26T08:30:00");
DateTime* before_refresh_date = new DateTime("2025-05-26T08:30:00");

uint8_t Image_BW[15000];    // Declare an array of 15000 bytes to store black and white image data

int startX = 0; // Starting horizontal axis
int startY = 0;  // Starting vertical axis
int fontSize = 24; // Font size
int endX = 400;    // End horizontal axis
int endY = 300;    // End vertical axis

const char *Loading_Message = "Wifi Connection in progress,please wait...";
String APIText = "Default";
String response = "Default";


String xmlRequest = R"rawliteral(<?xml version="1.0" encoding="utf-8"?>
  <soap:Envelope xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
      xmlns:m="http://schemas.microsoft.com/exchange/services/2006/messages"
      xmlns:t="http://schemas.microsoft.com/exchange/services/2006/types"
      xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
    <soap:Header>
      <t:RequestServerVersion Version="Exchange2016" />
    </soap:Header>
    <soap:Body>
      <m:FindItem Traversal="Shallow">
        <m:ItemShape>
          <t:BaseShape>IdOnly</t:BaseShape>
          <t:AdditionalProperties>
            <t:FieldURI FieldURI="item:Subject" />
            <t:FieldURI FieldURI="calendar:Start" />
            <t:FieldURI FieldURI="calendar:End" />
          </t:AdditionalProperties>
        </m:ItemShape>
        <m:CalendarView StartDate="{start}.000Z" EndDate="{end}T23:59:59Z" />
        <m:ParentFolderIds>
          <t:DistinguishedFolderId Id="calendar">
            <t:Mailbox>
              <t:EmailAddress>{email}</t:EmailAddress>
            </t:Mailbox>
          </t:DistinguishedFolderId>
        </m:ParentFolderIds>
      </m:FindItem>
    </soap:Body>
  </soap:Envelope>)rawliteral";


/*
*---------Function description: Display text content locally------------
*----Parameter introduction:
      content: Text content
      startX: Starting horizontal axis
      startY: Starting vertical axis
      fontSize: Font size
      color: Font color
      endX: End horizontal axis
      endY: End vertical axis
*/
void Part_Text_Display(const char* content, int startX, int &startY, int fontSize, int color, int endX, int endY) {
    int length = strlen(content);
    int i = 0;
    char line[(endX - startX) / (fontSize/2) + 1]; // Calculate the maximum number of characters per line based on the width of the area
    int currentX = startX;
    int currentY = startY;
    int lineHeight = fontSize;

    while (i < length) {
        int lineLength = 0;
        memset(line, 0, sizeof(line));

        // Fill the line until it reaches the width of the region or the end of the string
        while (lineLength < (endX - startX) / (fontSize/2) && i < length) {
            line[lineLength++] = content[i++];
        }

        while (lineLength < (endX - startX) / (fontSize/2)) {
            line[lineLength++] = ' ';
        }

        // If the current Y coordinate plus font size exceeds the area height, stop displaying
        if (currentY + lineHeight > endY) {
            break;
        }
        // Display this line
        EPD_ShowString(currentX, currentY, line, fontSize, color); 

        // Update the Y coordinate for displaying the next line
        currentY += lineHeight;

        // If there are still remaining strings but they have reached the bottom of the area, stop displaying them
        if (currentY + lineHeight > endY) {
            break;
        }
    }
    startY = currentY;
}




void refreshDateTime(DateTime* &datetime){
  // Configure NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);



  int retry = 0;
  while (!getLocalTime(&timeinfo) && retry < 10) {
    Serial.println("Waiting for time...");
    delay(1000);
    retry++;
  }

  if (retry == 10) {
    Serial.println("Failed to get time from NTP.");
  } else {
    Serial.println("Time synced successfully!");
//    Serial.print("Date: ");
//    Serial.print(timeinfo.tm_year + 1900); Serial.print("-");
//    Serial.print(timeinfo.tm_mon + 1);     Serial.print("-");
//    Serial.print(timeinfo.tm_mday);        Serial.print("  ");
//  
//    Serial.print("Time: ");
//    Serial.print(timeinfo.tm_hour);   Serial.print(":");
//    Serial.print(timeinfo.tm_min);    Serial.print(":");
//    Serial.println(timeinfo.tm_sec);

    char timeString[20];
    strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    datetime = new DateTime(timeString);
    Serial.println("Time defined successfully!");
  }
}





void Update_Display(String APIText) {
    Serial.print("Inside update_display : ");
    Serial.println(APIText);
    Serial.println(startX);
    Serial.println(startY);
    //EPD_ShowPicture(0, 0, 400, 80, EPFL_INN011_header, BLACK);
  
    startY = 80 + 20;

    if (APIText != "No more events today"){
      if (APIText.indexOf(",") != -1){
        int len = 0;
        int maxElem = 2;
        String* APITextArray = Split(APIText, ",", len);
        maxElem = len < maxElem ? len : maxElem;
        Serial.print("len : ");
        Serial.println(len);
        Serial.print("maxElem : ");
        Serial.println(maxElem);
        if (len > 1){
          for (int i = 0; i < maxElem; i++){
            Serial.println("a");
            Serial.println(APITextArray[i]);
            Serial.println(APITextArray[i]);
            Part_Text_Display(APITextArray[i].c_str(), startX, startY, fontSize, BLACK, endX, endY);
          }
        }
        else if (len == 1){
          Serial.println("b");
          Serial.println(APIText);
          Serial.println(" > " + APIText);
          Part_Text_Display(APIText.c_str(), startX, startY, fontSize, BLACK, endX, endY);
        }
        else{
          Serial.println("c");
          Part_Text_Display("error", startX, startY, fontSize, BLACK, endX, endY);
        }
      }
      else{
        Serial.println("d");
        Serial.println(APIText);
        Serial.println(APIText);
        Part_Text_Display(APIText.c_str(), startX, startY, fontSize, BLACK, endX, endY);
        //EPD_ShowString(startX, startY, (" > " + APIText).c_str(), fontSize, BLACK);//  Printing time
      }
    }
    else{
      Serial.println("e");
      Part_Text_Display(APIText.c_str(), startX, startY, fontSize, BLACK, endX, endY);
    }

    if (WiFi.status() == WL_CONNECTED){
      Serial.println("Update_Display before refresh date");
      refreshDateTime(before_refresh_date);
      String current_date_refresh = current_date->year + "-" + current_date->month + "-" + current_date->day + " " + current_date->hour + ":" + current_date->minute + ":" + current_date->second;
      EPD_ShowString(0, 11 * fontSize, current_date_refresh.c_str(), fontSize, BLACK);
      EPD_DrawLine(0, 11 * fontSize - 5, 250, 11 * fontSize - 5, BLACK); // Horizontal line
      EPD_DrawLine(250, 11 * fontSize - 5, 251, 390, BLACK); // Vertical line
      Serial.println("Update_Display after refresh date");
    }

    
    //EPD_Display_Part(0, 0, 400, 300, Image_BW); 
    Serial.println("Update_Display before refresh");
    EPD_Display(Image_BW);
    Serial.println("Update_Display middle refresh");
    EPD_Update_Part();
    Serial.println("Update_Display after refresh");
  
    //EPD_Sleep();                // Set the screen to sleep mode to save power
}


void clear_all() {
  // Function to clear the screen content
  EPD_Clear();                 // Clear the screen content, restoring it to its default state
  Paint_NewImage(Image_BW, EPD_W, EPD_H, 0, WHITE); // Create a new image buffer, size EPD_W x EPD_H, background color white
  Paint_Clear(WHITE);           // Fill the entire canvas with white
  EPD_Display_Part(0, 0, EPD_W, EPD_H, Image_BW); // Display the image stored in the Image_BW array
}


bool isCurrentEvent(Event* event, DateTime* current_date){
  return (current_date->hour >= (event->startDateTime).hour) ? 
      (current_date->hour == (event->startDateTime).hour) ? 
        (current_date->minute >= (event->startDateTime).minute) ?
          (current_date->hour == (event->endDateTime).hour) ?
            (current_date->minute <= (event->startDateTime).minute)
            :
            true
          :
          false
        : 
        (current_date->hour == (event->endDateTime).hour) ?
          (current_date->minute <= (event->startDateTime).minute)
          :
          ((current_date->hour >= (event->startDateTime).hour) && (current_date->hour <= (event->endDateTime).hour))
      :
      false;
}



void bouton_click_handler(String button){
  boutton_clicked = true;
  Serial.print(button);
  Serial.println(" Pressed");
  delay(600);
  boutton_clicked = false;
  if (button == "BUTTON_UP" || button == "BUTTON_DOWN" || button == "BUTTON_SCROLL_CLICK"){
    manualRefresh = true;
  }
}




String replaceAccentChar(String text){
  struct KeyValue {
    const char* key;
    const char* value;
  };

  //This dict is made by AI (ask it before it easy but take too much time to do handmade)
  KeyValue dict[] = {
    {"à", "a"}, {"á", "a"}, {"â", "a"}, {"ã", "a"}, {"ä", "a"}, {"å", "a"}, {"ā", "a"},
    {"À", "A"}, {"Á", "A"}, {"Â", "A"}, {"Ã", "A"}, {"Ä", "A"}, {"Å", "A"}, {"Ā", "A"},

    {"è", "e"}, {"é", "e"}, {"ê", "e"}, {"ë", "e"}, {"ē", "e"}, {"ė", "e"}, {"ę", "e"},
    {"È", "E"}, {"É", "E"}, {"Ê", "E"}, {"Ë", "E"}, {"Ē", "E"}, {"Ė", "E"}, {"Ę", "E"},

    {"ì", "i"}, {"í", "i"}, {"î", "i"}, {"ï", "i"}, {"ī", "i"}, {"į", "i"}, {"ı", "i"},
    {"Ì", "I"}, {"Í", "I"}, {"Î", "I"}, {"Ï", "I"}, {"Ī", "I"}, {"Į", "I"},

    {"ò", "o"}, {"ó", "o"}, {"ô", "o"}, {"õ", "o"}, {"ö", "o"}, {"ø", "o"}, {"ō", "o"},
    {"Ò", "O"}, {"Ó", "O"}, {"Ô", "O"}, {"Õ", "O"}, {"Ö", "O"}, {"Ø", "O"}, {"Ō", "O"},

    {"ù", "u"}, {"ú", "u"}, {"û", "u"}, {"ü", "u"}, {"ū", "u"}, {"ů", "u"}, {"ų", "u"},
    {"Ù", "U"}, {"Ú", "U"}, {"Û", "U"}, {"Ü", "U"}, {"Ū", "U"}, {"Ů", "U"}, {"Ų", "U"},

    {"ç", "c"}, {"ć", "c"}, {"č", "c"},
    {"Ç", "C"}, {"Ć", "C"}, {"Č", "C"},

    {"ñ", "n"}, {"ń", "n"}, {"ň", "n"},
    {"Ñ", "N"}, {"Ń", "N"}, {"Ň", "N"},

    {"ß", "ss"}, {"œ", "oe"}, {"Œ", "OE"}, {"æ", "ae"}, {"Æ", "AE"}
  };

  int len = sizeof(dict) / sizeof(dict[0]);


  for (int i = 0; i < len; i++){
    text.replace(dict[i].key, dict[i].value);
  }

  return text;
}











void setup() {
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_SCROLL_UP, INPUT_PULLUP);
  pinMode(BUTTON_SCROLL_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SCROLL_CLICK, INPUT_PULLUP);
  
  Serial.begin(115200);
  delay(10);
  xmlRequest.replace("{email}", API_EMAIL_TARGET); // Replace the {email} with the real email target in secrets.h


  // Initialization settings, executed only once when the program starts
  // Configure pin 7 as output mode and set it to high level to activate the screen power
  pinMode(7, OUTPUT);            // Set pin 7 as output mode
  digitalWrite(7, HIGH);         // Set pin 7 to high level, activating the screen power

  EPD_GPIOInit(); 
  EPD_Init();
  EPD_Clear(); 
  Paint_Clear(WHITE);
  EPD_Update();     
                    
  Paint_NewImage(Image_BW, EPD_W, EPD_H, 0, WHITE); 
  Paint_Clear(WHITE); 
  EPD_Display(Image_BW);
  EPD_Clear_R26H(Image_BW);
  EPD_Update();

  // The SPI initialization part is commented out
  // SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
  // SPI.begin ();








    
    
  
    
  
    //EPD_ShowPicture(0, 0, 312, 152, gImage_1, WHITE); // Display image gImage_1, starting coordinates (0, 0), width 312, height 152, background color white
    

    








  

    //Paint_NewImage(Image_BW, EPD_W, EPD_H, 0, WHITE); // Create a new image buffer, size EPD_W x EPD_H, background color white
    
  ///Paint_NewImage(Image_BW, EPD_W, EPD_H, 0, BLACK); // Create a new image buffer, size EPD_W x EPD_H, background color white
  ///EPD_Full(WHITE);  // Fill the entire canvas with white
  
  //EPD_Display_Part(0, 0, EPD_W, EPD_H, Image_BW); // Display the image stored in the Image_BW array
  //EPD_Display_Part(0, 0, EPD_W, EPD_H, Image_BW); // Display the image stored in the Image_BW array
  
  ///EPD_Display(Image_BW); // Display the image stored in the Image_BW array
  
  //EPD_Init_Fast(Fast_Seconds_1_5s); // Quickly initialize the EPD screen, setting it to 1.5 second fast mode
  
  ///EPD_Init_Fast(Fast_Seconds_1_5s); // Quickly initialize the EPD screen, setting it to 1.5 second fast mode
  EPD_ShowPicture(0, 0, 400, 80, EPFL_INN011_header, BLACK);
  EPD_ShowPicture(400 - 32, 300 - 32 - 16, 32, 32, epd_bitmap_refresh, BLACK);

  Serial.println("eee");
  Serial.println(sizeof(EPFL_INN011_header));
  Serial.println(sizeof(epd_bitmap_refresh));
  Serial.println("eee");
  

  Serial.println("Before Wifi text");
  Update_Display(Loading_Message);
  Serial.println("After Wifi text");
  





  Serial.println();
  Serial.print("Connecting to network: ");
  Serial.println(ssid);
  WiFi.disconnect(true);  //disconnect form wifi to set new wifi connection
  WiFi.mode(WIFI_STA);    //init wifi mode

  // Example1 (most common): a cert-file-free eduroam with PEAP (or TTLS)
  WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD);

  // Example 2: a cert-file WPA2 Enterprise with PEAP
  //WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD, ca_pem, client_cert, client_key);

  // Example 3: TLS with cert-files and no password
  //WiFi.begin(ssid, WPA2_AUTH_TLS, EAP_IDENTITY, NULL, NULL, ca_pem, client_cert, client_key);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    counter++;
    if (counter >= 60) {  //after 30 seconds timeout - reset board
      ESP.restart();
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address set: ");
  Serial.println(WiFi.localIP());  //print LAN IP

  refreshDateTime(current_date);
  refreshDateTime(before_refresh_date);
  
  //clear_all();               // Call the clear_all function to clear the screen content
}





















void loop() {
  Serial.print("current_date-> minute.toInt() (" + current_date-> minute + ") - before_refresh_date->minute.toInt() (" + before_refresh_date->minute + ") == autoRefreshMinutes " + String(autoRefreshMinutes) + " =>");
  Serial.println(current_date-> minute.toInt() - before_refresh_date->minute.toInt() == autoRefreshMinutes);
  unsigned long currentTime = millis();
  delay(100);
  
  if (digitalRead(BUTTON_DOWN) == LOW && !boutton_clicked) {
    bouton_click_handler("BUTTON_DOWN");
  }
  
  if (digitalRead(BUTTON_UP) == LOW && !boutton_clicked) {
    bouton_click_handler("BUTTON_UP");
  }
  
  if (digitalRead(BUTTON_SCROLL_UP) == LOW && !boutton_clicked) {
    bouton_click_handler("BUTTON_SCROLL_UP");
  }
  
  if (digitalRead(BUTTON_SCROLL_DOWN) == LOW && !boutton_clicked) {
    bouton_click_handler("BUTTON_SCROLL_DOWN");
  }
  
  if (digitalRead(BUTTON_SCROLL_CLICK) == LOW && !boutton_clicked) {
    bouton_click_handler("BUTTON_SCROLL_CLICK");
  }
  
//  Serial.print("WWW: ");
//  Serial.println(current_date->hour.toInt());
  
  if ((current_date->hour.toInt() >= MIN_HOUR_REFRESH && current_date->hour.toInt() <= MAX_HOUR_REFRESH && currentTime - lastUpdateTime >= 60000 * autoRefreshMinutes) || manualRefresh || firstLaunch){
    lastUpdateTime = currentTime;
    Update_Display(" , , , , , "); //Refresh partial replace (replacing all writing area with space)
    Serial.println("Start");
    Serial.print("firstLaunch : ");
    Serial.println(firstLaunch);
    Serial.print("manualRefresh : ");
    Serial.println(manualRefresh);
    Serial.print("(current_date->hour.toInt() >= MIN_HOUR_REFRESH && current_date->hour.toInt() <= MAX_HOUR_REFRESH && currentTime - lastUpdateTime >= 1000 * autoRefreshMinutes) : ");
    Serial.println((current_date->hour.toInt() >= MIN_HOUR_REFRESH && current_date->hour.toInt() <= MAX_HOUR_REFRESH && currentTime - lastUpdateTime >= 1000 * autoRefreshMinutes));
  //if ((current_date->hour.toInt() >= MIN_HOUR_REFRESH && current_date->hour.toInt() <= MAX_HOUR_REFRESH) || manualRefresh){
    lastUpdateTime = currentTime;
    firstLaunch = false;





    //Paint_NewImage(Image_BW, EPD_W, EPD_H, 0, WHITE); // Create a new image buffer, size EPD_W x EPD_H, background color white
    //EPD_Full(WHITE);              // Fill the entire canvas with white
    //EPD_Display_Part(0, 0, EPD_W, EPD_H, Image_BW); // Display the image stored in the Image_BW array
  
    //EPD_Init_Fast(Fast_Seconds_1_5s); // Quickly initialize the EPD screen, setting it to 1.5 second fast mode

    //EPD_ShowString(0, 0 * fontSize, "Make API request", fontSize, BLACK); 
    //EPD_ShowString(0, 1 * fontSize, "Please wait...", fontSize, BLACK); 
    Serial.println("Before API request text");
    Update_Display("Make API request,Please wait...");
    Serial.println("After API request text");
    //EPD_Display_Fast(Image_BW); // Quickly display the image stored in the Image_BW array

    //EPD_Update_Part();
  
    //EPD_Sleep();                // Set the screen to sleep mode to save power







    

    Serial.println("Before hour request");
    if (WiFi.status() == WL_CONNECTED) {  //if we are connected to Eduroam network
      counter = 0;                        //reset counter
      Serial.println("Wifi is still connected with IP: ");
      Serial.println(WiFi.localIP());            //inform user about his IP address
    } else if (WiFi.status() != WL_CONNECTED) {  //if we lost connection, retry
      WiFi.begin(ssid);
    }
    while (WiFi.status() != WL_CONNECTED) {  //during lost connection, print dots
      delay(500);
      Serial.print(".");
      counter++;
      if (counter >= 60) {  //30 seconds timeout - reset board
        ESP.restart();
      }
    }
    Serial.println("Connecting to NTP server: ");
  
//      // Configure NTP
//    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
//  
//  
//  
//    int retry = 0;
//    while (!getLocalTime(&timeinfo) && retry < 10) {
//      Serial.println("Waiting for time...");
//      delay(1000);
//      retry++;
//    }
//  
//    if (retry == 10) {
//      Serial.println("Failed to get time from NTP.");
//    } else {
//      Serial.println("Time synced successfully!");
//      Serial.print("Date: ");
//      Serial.print(timeinfo.tm_year + 1900); Serial.print("-");
//      Serial.print(timeinfo.tm_mon + 1);     Serial.print("-");
//      Serial.print(timeinfo.tm_mday);        Serial.print("  ");
//    
//      Serial.print("Time: ");
//      Serial.print(timeinfo.tm_hour);   Serial.print(":");
//      Serial.print(timeinfo.tm_min);    Serial.print(":");
//      Serial.println(timeinfo.tm_sec);
//  
//      char timeString[20];
//      strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%S", &timeinfo);
//      current_date = new DateTime(timeString);
//      Serial.print("Current time: ");
//      Serial.println(timeString);
//    }
//  
//    char dateTimeString[20];
//    char dateString[20];
//    strftime(dateTimeString, sizeof(dateTimeString), "%Y-%m-%dT%H:%M:%S", &timeinfo);
//    strftime(dateString, sizeof(dateString), "%Y-%m-%d", &timeinfo);
//    current_date = new DateTime(dateTimeString);


    refreshDateTime(current_date);
    Serial.println("After hour request");
  

    String dateString = "" + current_date->year + "-" + current_date->month + "-" + current_date->day;
    String dateTimeString = dateString + "T" + current_date->hour + ":" + current_date->minute + ":" + current_date->second;
  
    //xmlRequest.replace("{start}", dateTimeString); // Replace the {start} with the current datetime
    //xmlRequest.replace("{end}", dateString); // Replace the {end} with the current date
    xmlRequest.replace("{start}", "2025-06-02T00:30:24"); // Replace the {start} with the current datetime
    xmlRequest.replace("{end}", "2025-06-02"); // Replace the {end} with the current date
    //xmlRequest.replace("{start}", "2025-01-23T00:30:24"); // Replace the {start} with the current datetime
    //xmlRequest.replace("{end}", "2025-01-23"); // Replace the {end} with the current date
  
    Serial.print("Connecting to website: ");
    Serial.println(API_SERVICE_ENDPOINT);

    Serial.println("Before API request");
    HTTPClient https;
    https.begin(API_SERVICE_ENDPOINT);
    https.addHeader("Content-Type", "text/xml");
    https.setAuthorization(API_USERNAME, API_PASSWORD);
    int httpResponseCode = https.POST(xmlRequest);
    Serial.println("After API request");
  
    if (httpResponseCode == 200) {
      Serial.println("API request Success");
      String body = https.getString();
  //    Serial.print("200 got: ");
  //    Serial.println(body);
      APIText = body;
      response = body;
      int itemsLength = 0;
      String* items = XMLParser(body, "<t:Items>", "</t:Items>", itemsLength);
      int calendarItemLength = 0;
      String* calendarItem = XMLParser(items[0], "<t:CalendarItem>", "</t:CalendarItem>", calendarItemLength);
      Event *eventList[calendarItemLength];
      for (int i = 0; i < calendarItemLength; i++){
  //      Serial.print(i);
  //      Serial.print(" : : ");
  //      Serial.print(XMLGetter(calendarItem[i], "<t:Subject>", "</t:Subject>"));
  //      Serial.print(", ");
  //      Serial.print(XMLGetter(calendarItem[i], "<t:Start>", "</t:Start>"));
  //      Serial.print(", ");
  //      Serial.println(XMLGetter(calendarItem[i], "<t:End>", "</t:End>"));
        Serial.println(XMLGetter(calendarItem[i], "<t:Subject>", "</t:Subject>"));
        Serial.println();
        eventList[i] = new Event(
          replaceAccentChar(XMLGetter(calendarItem[i], "<t:Subject>", "</t:Subject>")),
          DateTime(XMLGetter(calendarItem[i], "<t:Start>", "</t:Start>")),
          DateTime(XMLGetter(calendarItem[i], "<t:End>", "</t:End>"))
        );
      }
      for (int j = 0; j < calendarItemLength; j++){
        Serial.print(j);
        Serial.print(" =>>> ");
        Serial.println(eventList[j]->subject);
        Serial.print((eventList[j]->startDateTime).hour);
        Serial.print(":");
        Serial.println((eventList[j]->startDateTime).minute);
        Serial.print((eventList[j]->endDateTime).hour);
        Serial.print(":");
        Serial.println((eventList[j]->endDateTime).minute);
      }
  
      if (calendarItemLength != 0){
        Serial.println(isCurrentEvent(eventList[0], new DateTime("2025-01-23T10:30:00")) ? "is current" : "is not current");
        Serial.println(isCurrentEvent(eventList[0], new DateTime("2025-01-23T11:00:00")) ? "is current" : "is not current");
        Serial.println(isCurrentEvent(eventList[0], new DateTime("2025-01-23T11:01:00")) ? "is current" : "is not current");
        //bool isCurrent = isCurrentEvent(eventList[0], new DateTime("2025-01-23T10:30:00"));
        bool isCurrent = isCurrentEvent(eventList[0], current_date);
        String prefix = isCurrent ? "current : " : "next : ";
        if (calendarItemLength == 1){
          //Check if current or not for text
          //APIText = (eventList[0]->startDateTime).hour +":" + (eventList[0]->startDateTime).minute + " -> " + (eventList[0]->endDateTime).hour +":" + (eventList[0]->endDateTime).minute + "\n" + eventList[0]->subject;
          APIText = " > " + String((eventList[0]->startDateTime).hour.toInt() + 2 < 10 ? "0" + String((eventList[0]->startDateTime).hour.toInt() + 2) : (eventList[0]->startDateTime).hour.toInt() + 2) +":" + (eventList[0]->startDateTime).minute + "-" + ((eventList[0]->endDateTime).hour.toInt() + 2 < 10 ? "0" + String((eventList[0]->endDateTime).hour.toInt() + 2) : (eventList[0]->endDateTime).hour.toInt() + 2) +":" + (eventList[0]->endDateTime).minute + " " + eventList[0]->subject;
        }
        else{
          //APIText = (eventList[0]->startDateTime).hour +":" + (eventList[0]->startDateTime).minute + " -> " + (eventList[0]->endDateTime).hour +":" + (eventList[0]->endDateTime).minute + "\n" + eventList[0]->subject;
//          APIText = prefix + eventList[0]->subject + " - " + ((eventList[0]->startDateTime).hour.toInt() + 2) +":" + (eventList[0]->startDateTime).minute + " -> " + ((eventList[0]->endDateTime).hour.toInt() + 2) +":" + (eventList[0]->endDateTime).minute +
//          " and after : " + eventList[1]->subject + " - " + ((eventList[1]->startDateTime).hour.toInt() + 2) +":" + (eventList[1]->startDateTime).minute + " -> " + ((eventList[1]->endDateTime).hour.toInt() + 2) +":" + (eventList[1]->endDateTime).minute;
          APIText = "";
          for (int i = 0; i < calendarItemLength; i++){
            String separator = i == 0 ? " > " : ",   ";
            Serial.println("dd");
            String res = separator + ((eventList[i]->startDateTime).hour.toInt() + 2 < 10 ? "0" + String((eventList[i]->startDateTime).hour.toInt() + 2) : (eventList[i]->startDateTime).hour.toInt() + 2) +":" + (eventList[i]->startDateTime).minute + "-" + ((eventList[i]->endDateTime).hour.toInt() + 2 < 10 ? "0" + String((eventList[i]->endDateTime).hour.toInt() + 2) : (eventList[i]->endDateTime).hour.toInt() + 2) +":" + (eventList[i]->endDateTime).minute + " " + eventList[i]->subject;
            Serial.println(res);
            APIText += res;
          }
        }
      }
      else{
        APIText = "No more events today";
        Serial.println(APIText);
      }
  
      //2025-02-14T10:30:00
      DateTime* date = new DateTime("2025-02-14T10:30:00");
      Serial.println(date->year);
      Serial.println(date->month);
      Serial.println(date->day);
  
      Serial.println(date->hour);
      Serial.println(date->minute);
      Serial.println(date->second);
  
      Serial.println("current_date");
      Serial.println(current_date->year);
      Serial.println(current_date->month);
      Serial.println(current_date->day);
  
      Serial.println(current_date->hour);
      Serial.println(current_date->minute);
      Serial.println(current_date->second);
    }
    else {
      Serial.print("Err: ");
      Serial.println(httpResponseCode);
      APIText = "Error check Serial";
      response = "Error check Serial";
    }
  
  
  
    Serial.println("Before Clear");
  
    
    //EPD_Clear();                   // Clear the screen content, restoring it to its default state
//    Paint_NewImage(Image_BW, EPD_W, EPD_H, 0, WHITE); // Create a new image buffer, size EPD_W x EPD_H, background color white
//    EPD_Full(WHITE);              // Fill the entire canvas with white
//    EPD_Display_Part(0, 0, EPD_W, EPD_H, Image_BW); // Display the image stored in the Image_BW array
//  
//    EPD_Init_Fast(Fast_Seconds_1_5s); // Quickly initialize the EPD screen, setting it to 1.5 second fast mode













//    Paint_NewImage(Image_BW, EPD_W, EPD_H, 0, BLACK); // Create a new image buffer, size EPD_W x EPD_H, background color white
//    EPD_Full(WHITE);              // Fill the entire canvas with white
//    EPD_Display_Part(0, 0, EPD_W, EPD_H, Image_BW); // Display the image stored in the Image_BW array
//  
//    EPD_Init_Fast(Fast_Seconds_1_5s); // Quickly initialize the EPD screen, setting it to 1.5 second fast mode
//  
//    //EPD_ShowPicture(0, 0, 312, 152, gImage_1, WHITE); // Display image gImage_1, starting coordinates (0, 0), width 312, height 152, background color white
//    EPD_ShowPicture(0, 0, 400, 80, EPFL_INN011_header, BLACK);
//
//    startY = 80 + 20;


      Update_Display(" , , , , , "); //Refresh partial replace (replacing all writing area with space)
      Update_Display(APIText);
//    if (APIText != "No more events today"){
//      if (APIText.indexOf(",") != -1){
//        int len = 0;
//        int maxElem = 2;
//        maxElem = len < maxElem ? len : maxElem;
//        String* APITextArray = Split(APIText, ",", len);
//        if (len > 1){
//          for (int i = 0; i < len; i++){
//            Serial.println("a");
//            Serial.println(APITextArray[i]);
//            Serial.println(( i == 0 ? " > " : "   ") + APITextArray[i]);
//            Part_Text_Display((( i == 0 ? " > " : "   ") + APITextArray[i]).c_str(), startX, startY, fontSize, BLACK, endX, endY);
//          }
//        }
//        else if (len == 1){
//          Serial.println("b");
//          Serial.println(APIText);
//          Serial.println(" > " + APIText);
//          Part_Text_Display((" > " + APIText).c_str(), startX, startY, fontSize, BLACK, endX, endY);
//        }
//        else{
//          Serial.println("c");
//          Part_Text_Display("error", startX, startY, fontSize, BLACK, endX, endY);
//        }
//      }
//      else{
//        Serial.println("d");
//        Serial.println(APIText);
//        Serial.println(" > " + APIText);
//        Part_Text_Display((" > " + APIText).c_str(), startX, startY, fontSize, BLACK, endX, endY);
//      }
//    }
//    else{
//      Serial.println("e");
//      Part_Text_Display(APIText.c_str(), startX, startY, fontSize, BLACK, endX, endY);
//    }
//    //Part_Text_Display(APIText.c_str(), startX, startY, fontSize, BLACK, endX, endY);
//
//    String current_date_refresh = current_date->year + "-" + current_date->month + "-" + current_date->day + " " + current_date->hour + ":" + current_date->minute + ":" + current_date->second;
//    EPD_ShowString(0, 11 * fontSize, ("last refresh: " + current_date_refresh).c_str(), fontSize, BLACK); 
//    EPD_Display_Fast(Image_BW); // Quickly display the image stored in the Image_BW array
//  
//    EPD_Sleep();                // Set the screen to sleep mode to save power
//
//    Serial.println("After Clear");
//
    refreshDateTime(before_refresh_date);
    manualRefresh = false;
  }
}
