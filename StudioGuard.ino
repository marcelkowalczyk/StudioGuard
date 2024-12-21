#include <Wire.h>
#include <RTClib.h>
#include <U8x8lib.h>
#include "DHT.h"
#include <EEPROM.h>
//#include <SPI.h>
//#include <SPIFlash.h>

// Definicje pinów i komponentów
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3
#define ENCODER_BUTTON 9
#define GROVE_BUTTON 6
#define DHTTYPE DHT10   // DHT 20
#define FLASH_CS_PIN 10
#define LED_PIN 4
#define BUZZER_PIN 5
#define POTENTIOMETER_PIN A0
#define OLED_MIN_BRIGHTNESS 0
#define OLED_MAX_BRIGHTNESS 255


RTC_DS1307 rtc;

//SPIFlash flash(FLASH_CS_PIN);
DHT dht(DHTTYPE);  

// Inicjalizacja OLED SSD1306 w trybie U8X8
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);

// Zmienne globalne
//
unsigned long buttonPressTime = 0;
bool longPressActive = false;
// Zmienne enkodera
enum Screen :uint8_t {
    SCREEN_DATA,     // Ekran danych
    SCREEN_MENU,     // Menu główne
    SCREEN_SETTINGS  // Ustawienia
};
// Struktura przechowująca ustawienia
struct Settings {
    float tempMin;
    float tempMax;
    float humMin;
    float humMax;
    bool isCelsius = true;
};
// Globalna zmienna do pracy z ustawieniami
Settings deviceSettings;

// Adres początkowy ustawień w EEPROM
const int EEPROM_SETTINGS_ADDR = 0;

bool isEdit = false;
Screen currentScreen = SCREEN_DATA;  // Ekran startowy
int currentMenuOption = 0;
int currentSettingsOption = 0;
int lastScreen = -1; // Śledzenie ostatniego ekranu, aby uniknąć niepotrzebnego odświeżania
//bool isCelsius = true; // Flaga do przełączania jednostek
int oldGroveButtonState = 0;


int oldEncoderPinA = 0;
int oldEncoderPinB = 0;
int encoderDirection = 0;

// Przykladowe dane dla temperatury i wilgotnosci
float temperature = 22.5;
float humidity = 55.0;

const uint32_t FLASH_SIZE = 8388608; // Rozmiar pamięci 8 MB (dla W25Q64)
const uint32_t BLOCK_SIZE = 32;     // Blok danych na wpis: 32 bajty
// Adresy pamięci Flash
uint32_t writeAddress = 0;
char logBuffer[BLOCK_SIZE];

volatile uint16_t refreshCounter = 0;
volatile uint16_t saveCounter = 0;
volatile bool refreshFlag = false;
volatile bool saveFlag = false;
//float tempMin = 10;
//float tempMax = 40;
//float humMin = 30;
//float humMax = 90;
// Ikona termometru (32x32 pikseli)
const uint8_t ICON_THERMOMETER[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xfe, 0x7e, 0xfc, 
	0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 
	0xff, 0x00, 0x00, 0xc0, 0xe0, 0xf8, 0xf0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xe0, 0x30, 0x3f, 0x1f, 0x00, 0x1f, 
	0x3f, 0x70, 0xe0, 0xc7, 0x0f, 0x1f, 0xff, 0xff, 0xff, 0xfe, 0xf8, 0xe0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x1f, 0x38, 0x30, 0x30, 0x30, 0x30, 
	0x30, 0x38, 0x1f, 0x07, 0x00, 0x70, 0x3f, 0x3f, 0x3f, 0x1f, 0x0f, 0x07, 0x00, 0x00, 0x00, 0x00
};

// Ikona koła zębatego (32x32 pikseli)
const uint8_t ICON_GEAR[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0x80, 0x00, 0x00, 0x80, 0xf0, 0xf0, 
	0xf0, 0xf0, 0xe0, 0x00, 0x00, 0x80, 0xc0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xc0, 0xc0, 0xc2, 0xe7, 0xff, 0xff, 0xff, 0x3f, 0x0f, 0x0f, 0x07, 0x07, 
	0x07, 0x07, 0x0f, 0x0f, 0x3f, 0xff, 0xff, 0xff, 0xe7, 0xc2, 0xc0, 0xc0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x43, 0xe7, 0xff, 0xff, 0xff, 0xfc, 0xf0, 0xf0, 0xe0, 0xe0, 
	0xe0, 0xe0, 0xf0, 0xf0, 0xfc, 0xff, 0xff, 0xff, 0xe7, 0x43, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x01, 0x00, 0x00, 0x01, 0x0f, 0x0f, 
	0x0f, 0x0f, 0x07, 0x00, 0x00, 0x01, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void setup() {
  Serial.begin(9600);
  // Inicjalizacja OLED
  u8x8.begin();
  u8x8.setFont(u8x8_font_amstrad_cpc_extended_f);
  dht.begin();
  // Inicjalizacja Flash
  /*if (!flash.begin()) {
      Serial.println("Błąd inicjalizacji pamięci Flash!");
      while (1);
  }
  Serial.println("Pamięć Flash gotowa.");
*/
  // Inicjalizacja RTC
  if (!rtc.begin()) {
      Serial.println("Błąd inicjalizacji RTC!");
      while (1);
  }
  //rtc.adjust(DateTime(2024, 12, 21, 1, 51, 0)); // Rok, miesiąc, dzień, godzina, minuta, sekunda
  // Konfiguracja pinów enkodera i przycisku
  pinMode(ENCODER_PIN_A, INPUT);
  pinMode(ENCODER_PIN_B, INPUT);
  digitalWrite(ENCODER_PIN_A, LOW);
  digitalWrite(ENCODER_PIN_B, LOW);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), readEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), readEncoder, CHANGE);
  pinMode(ENCODER_BUTTON, INPUT_PULLUP);
  pinMode(GROVE_BUTTON, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(POTENTIOMETER_PIN, INPUT);

  delay(30);
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  loadSettingsFromEEPROM();

  displaySensorData();
  
  /// Inicjalizacja Timer1
  TCCR1A = 0;                     // Normal mode
  TCCR1B = (1 << CS12) | (1 << CS10); // Prescaler 1024
  TIMSK1 = (1 << TOIE1);          // Włącz przerwanie przepełnienia
  TCNT1 = 0;

}

void loop() {
  // Odświeżanie danych co 10 sekund
  if (refreshFlag) {
      refreshFlag = false; // Zresetuj flagę
      humidity = dht.readHumidity();
      temperature = dht.readTemperature();

      if (currentScreen == SCREEN_DATA) { // Aktualizuj dane tylko na ekranie danych
          if (!isnan(humidity) && !isnan(temperature)) {
              updateDataScreen(); // Aktualizacja wyświetlacza
              Serial.print(rtc.now().day());
              Serial.print(".");
              Serial.print(rtc.now().month());
              Serial.print(".");
              Serial.println(rtc.now().year());
              Serial.print(rtc.now().hour());
              Serial.print(":");
              Serial.print(rtc.now().minute());
              Serial.print(":");
              Serial.println(rtc.now().second());
              Serial.println(deviceSettings.tempMin);
              Serial.println(deviceSettings.tempMax);
              Serial.println(deviceSettings.humMin);
              Serial.println(deviceSettings.humMax);
              Serial.println(deviceSettings.isCelsius);
          } else {
              Serial.println("Błąd odczytu czujnika.");
          }
      }
  }

  checkLimits(); // Sprawdzanie limitów i alarmu
  adjustBrightness(); // Regulacja jasności ekranu
  // Obsługa enkodera
  if (encoderDirection != 0) {
    if (currentScreen == SCREEN_MENU) {
      currentMenuOption = (currentMenuOption + encoderDirection + 2) % 2; // Menu ma 2 opcje
      displayMenu();
    } else if (currentScreen == SCREEN_SETTINGS) {
      handleSettingsNavigation(encoderDirection);
    }
    encoderDirection = 0;
  }

  // Obsługa przycisku Grove
  int newGroveButtonState = digitalRead(GROVE_BUTTON);
  if (newGroveButtonState != oldGroveButtonState) {
    if (newGroveButtonState == HIGH) {
      deviceSettings.isCelsius = !deviceSettings.isCelsius;
      updateDataScreen();
      //errorMsg = "LOL";
    }
    oldGroveButtonState = newGroveButtonState;
    delay(50);
  }

  // Obsługa przycisku enkodera
  if (digitalRead(ENCODER_BUTTON) == LOW) {
    if (!longPressActive) {
      buttonPressTime = millis();
      longPressActive = true;
    }
  } else {
    if (longPressActive && (millis() - buttonPressTime > 1500)) {
      // Długo przytrzymany przycisk - przejście do menu
      isEdit = false;
      currentScreen = SCREEN_MENU;
      displayMenu();
    } else if (longPressActive) {
      // Krótkie kliknięcie
      handleEncoderButton();
    }
    longPressActive = false;
  }
}
void handleEncoderButton() {
  if (currentScreen == SCREEN_MENU) {
    if (currentMenuOption == 0) {
      currentScreen = SCREEN_DATA;
      displaySensorData();
    } else if (currentMenuOption == 1) {
      currentScreen = SCREEN_SETTINGS;
      displaySettingsScreen();
    }
  } else if (currentScreen == SCREEN_SETTINGS) {
    handleSettingsClick();
  }
}

/*void updateScreen() {
    if (currentScreen == SCREEN_DATA) {
        displaySensorData();
    } else if (currentScreen == SCREEN_MENU) {
        displayMenu();
    } else if (currentScreen == SCREEN_SETTINGS) {
        displaySettingsScreen();
    }
}*/
void drawDataScreen() {
  u8x8.clear();
  
  // Rysowanie stałego układu ekranu
  u8x8.drawString(2, 2, "Temp:");
  u8x8.drawString(2, 4, "Wilg:");
}
void updateDataScreen() {
  char buffer[10];

  // Aktualizacja temperatury
  float temperatureToShow = deviceSettings.isCelsius ? temperature : (temperature * 9 / 5) + 32;
  const char* tempUnit = deviceSettings.isCelsius ? "C" : "F";

  dtostrf(temperatureToShow, 5, 1, buffer);  // Konwersja temperatury do tekstu
  strcat(buffer, tempUnit);                 // Dodanie jednostki (C/F)
  u8x8.drawString(8, 2, buffer);            // Wyświetlenie temperatury w wierszu 2, kolumna 8

  // Aktualizacja wilgotności
  dtostrf(humidity, 5, 1, buffer);      // Konwersja wilgotności do tekstu
  strcat(buffer, "%");                      // Dodanie znaku procenta
  u8x8.drawString(8, 4, buffer);            // Wyświetlenie wilgotności w wierszu 4, kolumna 8
  
}

void displaySensorData() {
  drawDataScreen();   // Rysuje stały układ ekranu
  updateDataScreen(); // Aktualizuje dynamiczne dane
}

void displayMenu() {
  u8x8.clear();

  if (currentMenuOption == 0) {
    // Ekran "Dane"
    u8x8.drawTile(1, 2, 4, ICON_THERMOMETER);
    u8x8.drawTile(1, 3, 4, ICON_THERMOMETER + 32);
    u8x8.drawTile(1, 4, 4, ICON_THERMOMETER + 64);
    u8x8.drawTile(1, 5, 4, ICON_THERMOMETER + 96);  // Wyświetlenie dużej ikony koła zębatego
    u8x8.drawString(5, 4, "DANE");           // Tekst w jednej linii
  } else if (currentMenuOption == 1) {
    // Ekran "Ustawienia"
    u8x8.drawTile(1, 2, 4, ICON_GEAR);
    u8x8.drawTile(1, 3, 4, ICON_GEAR + 32);
    u8x8.drawTile(1, 4, 4, ICON_GEAR + 64);
    u8x8.drawTile(1, 5, 4, ICON_GEAR + 96);  // Wyświetlenie dużej ikony koła zębatego
    u8x8.drawString(5, 4, "USTAWIENIA");     // Tekst w jednej linii
  }
}

void readEncoder() {
  int newEncoderPinA = digitalRead(ENCODER_PIN_A);
  int newEncoderPinB = digitalRead(ENCODER_PIN_B);

  if (newEncoderPinA != oldEncoderPinA || newEncoderPinB != oldEncoderPinB) {
      // Sprawdzenie zmiany stanu dla każdego pinu enkodera
      if (!oldEncoderPinA && !oldEncoderPinB && newEncoderPinA && !newEncoderPinB) {
          encoderDirection = 1; // Obrót w prawo
          delay(5);
      } else if (!oldEncoderPinA && !oldEncoderPinB && !newEncoderPinA && newEncoderPinB) {
          encoderDirection = -1; // Obrót w lewo
          delay(0);
      }

      // Aktualizacja poprzednich stanów
      oldEncoderPinA = newEncoderPinA;
      oldEncoderPinB = newEncoderPinB;

      delay(5); // Małe opóźnienie dla de-bouncingu
  }
}


/*void handleEncoderMovement(int direction) {
  if (currentScreen == SCREEN_MENU) {
    // Zakładamy, że są 2 opcje w menu
    currentMenuOption = (currentMenuOption + direction + 2) % 2; // Modulo 2 dla dwóch opcji
  } else if (currentScreen == SCREEN_SETTINGS) {
      // Zakładamy, że są 5 opcji w ustawieniach (indeksy od 0 do 4)
      currentSettingsOption = (currentSettingsOption + direction + 5) % 5; // Modulo 5 dla pięciu opcji
  }
  updateScreen();
}*/

void displaySettingsScreen() {
  u8x8.clear();
    // Wyświetlanie całego ekranu tylko raz
    for (int i = 0; i < 5; i++) {
        drawSettingLine(i, false);
    }
    // Zaznaczenie aktywnej linii
    drawSettingLine(currentSettingsOption, true);
}
// Funkcja obsługująca poruszanie się po opcjach ustawień
void handleSettingsNavigation(int direction) {
    if (!isEdit) {
        // Przemieszczanie się między wierszami
        drawSettingLine(currentSettingsOption, false); // Odznacz poprzednią linię
        currentSettingsOption = (currentSettingsOption + direction + 5) % 5; // Modulo 5 dla 5 opcji
        drawSettingLine(currentSettingsOption, true);  // Zaznacz nową linię
    } else {
        // Zmiana wartości w trybie edycji
        switch (currentSettingsOption) {
            case 0: { // Temp min
                float tempMinToEdit = deviceSettings.isCelsius ? deviceSettings.tempMin : (deviceSettings.tempMin * 9 / 5 + 32);
                tempMinToEdit = round(tempMinToEdit) + direction; // Zmiana w wybranej jednostce
                tempMinToEdit = constrain(tempMinToEdit, (deviceSettings.isCelsius ? -20 : -4), (deviceSettings.isCelsius ? deviceSettings.tempMax - 1 : (deviceSettings.tempMax * 9 / 5 + 32 - 1)));
                deviceSettings.tempMin = deviceSettings.isCelsius ? tempMinToEdit : ((tempMinToEdit - 32) * 5 / 9);
                break;
            }
            case 1: { // Temp max
                float tempMaxToEdit = deviceSettings.isCelsius ? deviceSettings.tempMax : (deviceSettings.tempMax * 9 / 5 + 32);
                tempMaxToEdit = round(tempMaxToEdit) + direction; // Zmiana w wybranej jednostce
                tempMaxToEdit = constrain(tempMaxToEdit, (deviceSettings.isCelsius ? deviceSettings.tempMin + 1 : (deviceSettings.tempMin * 9 / 5 + 32 + 1)), (deviceSettings.isCelsius ? 100 : 212));
                deviceSettings.tempMax = deviceSettings.isCelsius ? tempMaxToEdit : ((tempMaxToEdit - 32) * 5 / 9);
                break;
            }
            case 2: // Wilg min
                deviceSettings.humMin = constrain(deviceSettings.humMin + direction, 0, deviceSettings.humMax - 1);
                break;
            case 3: // Wilg max
                deviceSettings.humMax = constrain(deviceSettings.humMax + direction, deviceSettings.humMin + 1, 100);
                break;
            case 4: // Jednostki
                if (direction != 0) {
                    deviceSettings.isCelsius = !deviceSettings.isCelsius; // Przełącz jednostki
                    drawSettingLine(0, false);
                    drawSettingLine(1, false);
                }
                break;
        }
        drawSettingLine(currentSettingsOption, true); // Odśwież tylko zmienioną linię
    }
}

void drawSettingLine(int index, bool highlight) {
    char buffer[10];
    u8x8.setInverseFont(highlight && !isEdit); // Inwersja całej linii tylko w trybie nawigacji

    switch (index) {
        case 0: { // Temp min
            u8x8.setCursor(1, 1);
            u8x8.print("Temp min: ");
            float displayTempMin = deviceSettings.isCelsius ? deviceSettings.tempMin : (deviceSettings.tempMin * 9 / 5 + 32); // Przelicz na F jeśli potrzeba
            //displayTempMin = round(displayTempMin); // Zawsze pokazuj pełne liczby
            if (isEdit && index == currentSettingsOption) u8x8.setInverseFont(true); // Inwersja wartości w trybie edycji
            dtostrf(displayTempMin, 5, 1, buffer); // Wyświetlanie jako całość
            u8x8.print(buffer);
            break;
        }
        case 1: { // Temp max
            u8x8.setCursor(1, 2);
            u8x8.print("Temp max: ");
            float displayTempMax = deviceSettings.isCelsius ? deviceSettings.tempMax : (deviceSettings.tempMax * 9 / 5 + 32); // Przelicz na F jeśli potrzeba
            //displayTempMax = round(displayTempMax); // Zawsze pokazuj pełne liczby
            if (isEdit && index == currentSettingsOption) u8x8.setInverseFont(true);
            dtostrf(displayTempMax, 5, 1, buffer); // Wyświetlanie jako całość
            u8x8.print(buffer);
            break;
        }
        case 2: { // Wilg min
            u8x8.setCursor(1, 4);
            u8x8.print("Wilg min: ");
            if (isEdit && index == currentSettingsOption) u8x8.setInverseFont(true);
            dtostrf(deviceSettings.humMin, 5, 0, buffer); // Wyświetlanie jako całość
            u8x8.print(buffer);
            break;
        }
        case 3: { // Wilg max
            u8x8.setCursor(1, 5);
            u8x8.print("Wilg max: ");
            if (isEdit && index == currentSettingsOption) u8x8.setInverseFont(true);
            dtostrf(deviceSettings.humMax, 5, 0, buffer); // Wyświetlanie jako całość
            u8x8.print(buffer);
            break;
        }
        case 4: { // Jednostki
            u8x8.setCursor(1, 7);
            u8x8.print("Jednostki: ");
            if (isEdit && index == currentSettingsOption) u8x8.setInverseFont(true);
            u8x8.print(deviceSettings.isCelsius ? "C" : "F");
            break;
        }
    }

    u8x8.setInverseFont(false); // Wyłączenie inwersji
}


/*
// Funkcja obsługująca poruszanie się po opcjach ustawień
void handleSettingsNavigation(int direction) {
    if (!isEdit) {
        // Przemieszczanie się między wierszami
        drawSettingLine(currentSettingsOption, false); // Odznacz poprzednią linię
        currentSettingsOption = (currentSettingsOption + direction + 5) % 5; // Modulo 5 dla 5 opcji
        drawSettingLine(currentSettingsOption, true);  // Zaznacz nową linię
    } else {
        // Zmiana wartości w trybie edycji
        switch (currentSettingsOption) {
            case 0:
                tempMin = constrain(tempMin + direction, -20, tempMax - 1); // Ograniczenie do zakresu i tempMax
                break;
            case 1:
                tempMax = constrain(tempMax + direction, tempMin + 1, 100); // Ograniczenie do tempMin i maksymalnej wartości
                break;
            case 2:
                humMin = constrain(humMin + direction, 0, humMax - 1); // Ograniczenie do wilgMax
                break;
            case 3:
                humMax = constrain(humMax + direction, humMin + 1, 100); // Ograniczenie do wilgMin i maksymalnej wartości
                break;
            case 4:
                if (direction != 0) isCelsius = !isCelsius; // Przełącz jednostki tylko przy zmianie
                break;
        }
        drawSettingLine(currentSettingsOption, true); // Odśwież tylko zmienioną linię
    }
}


void drawSettingLine(int index, bool highlight) {
    char buffer[10];
    u8x8.setInverseFont(highlight && !isEdit); // Inwersja całej linii tylko w trybie nawigacji

    switch (index) {
        case 0:
            u8x8.setCursor(1, 1);
            u8x8.print("Temp min: ");
            if (isEdit && index == currentSettingsOption) u8x8.setInverseFont(true); // Inwersja wartości w trybie edycji
            dtostrf(tempMin, 5, 1, buffer);
            u8x8.print(buffer);
            break;
        case 1:
            u8x8.setCursor(1, 2);
            u8x8.print("Temp max: ");
            if (isEdit && index == currentSettingsOption) u8x8.setInverseFont(true);
            dtostrf(tempMax, 5, 1, buffer);
            u8x8.print(buffer);
            break;
        case 2:
            u8x8.setCursor(1, 4);
            u8x8.print("Wilg min: ");
            if (isEdit && index == currentSettingsOption) u8x8.setInverseFont(true);
            dtostrf(humMin, 5, 1, buffer);
            u8x8.print(buffer);
            break;
        case 3:
            u8x8.setCursor(1, 5);
            u8x8.print("Wilg max: ");
            if (isEdit && index == currentSettingsOption) u8x8.setInverseFont(true);
            dtostrf(humMax, 5, 1, buffer);
            u8x8.print(buffer);
            break;
        case 4:
            u8x8.setCursor(1, 7);
            u8x8.print("Jednostki: ");
            if (isEdit && index == currentSettingsOption) u8x8.setInverseFont(true);
            u8x8.print(isCelsius ? "C" : "F");
            break;
    }

    u8x8.setInverseFont(false); // Wyłączenie inwersji
}
*/

// Funkcja obsługująca kliknięcie enkodera w ustawieniach
void handleSettingsClick() {
    if (isEdit) {
        isEdit = false; // Wyjście z trybu edycji
        saveSettingsToEEPROM();
    } else {
        isEdit = true; // Wejście w tryb edycji
    }
    drawSettingLine(currentSettingsOption, true); // Odśwież aktualną linię
}

void checkLimits() {
    bool alarm = false;

    if (temperature < deviceSettings.tempMin || temperature > deviceSettings.tempMax) {
        alarm = true;
    }

    if (humidity < deviceSettings.humMin || humidity > deviceSettings.humMax) {
        alarm = true;
    }

    if (alarm) {
        // Włącz alarm: migająca dioda i zmienny ton buzzera
        static bool ledState = false;
        static unsigned long lastToggle = 0;
        static unsigned long lastToneChange = 0;
        static int buzzerTone = 440; // Startowy ton (440 Hz)
        const int toneChangeInterval = 200; // Zmieniaj ton co 200 ms
        unsigned long currentMillis = millis();

        // Miganie diody co 500 ms
        if (currentMillis - lastToggle >= 500) {
            lastToggle = currentMillis;
            ledState = !ledState;
            digitalWrite(LED_PIN, ledState);
        }

        // Zmieniaj ton buzzera co 200 ms
        if (currentMillis - lastToneChange >= toneChangeInterval) {
            lastToneChange = currentMillis;
            buzzerTone = (buzzerTone == 440) ? 880 : 440; // Przełączaj między 440 Hz a 880 Hz
            tone(BUZZER_PIN, buzzerTone); // Generuj ton na pinie buzzera
        }
    } else {
        // Wyłącz alarm
        digitalWrite(LED_PIN, LOW);
        noTone(BUZZER_PIN); // Wyłącz buzzer
    }
}

void adjustBrightness() {
    // Odczytaj wartość z potencjometru (zakres 0-1023)
    int potValue = analogRead(POTENTIOMETER_PIN);

    // Przeskaluj wartość potencjometru na zakres jasności OLED
    int brightness = map(potValue, 0, 1023, OLED_MIN_BRIGHTNESS, OLED_MAX_BRIGHTNESS);

    // Ustaw jasność ekranu
    u8x8.setContrast(brightness);
}
// Funkcja zapisująca ustawienia do EEPROM
void saveSettingsToEEPROM() {
    // Pobierz aktualne dane z EEPROM
    Settings currentSettings;
    EEPROM.get(EEPROM_SETTINGS_ADDR, currentSettings);

    // Porównaj dane i zapisz tylko, jeśli są różne
    if (memcmp(&currentSettings, &deviceSettings, sizeof(Settings)) != 0) {
        EEPROM.put(EEPROM_SETTINGS_ADDR, deviceSettings);
        Serial.println("Ustawienia zapisane do EEPROM.");
    } else {
        Serial.println("Brak zmian w ustawieniach. Nie zapisano do EEPROM.");
    }
}

// Funkcja odczytu ustawień z EEPROM
void loadSettingsFromEEPROM() {
    EEPROM.get(EEPROM_SETTINGS_ADDR, deviceSettings);

    // Sprawdź, czy EEPROM zawiera prawidłowe dane
    if (deviceSettings.tempMin < -20 || deviceSettings.tempMax > 100 ||
        deviceSettings.humMin < 0 || deviceSettings.humMax > 100) {
        // Przywróć wartości domyślne w przypadku błędnych danych
        deviceSettings.tempMin = 10;
        deviceSettings.tempMax = 40;
        deviceSettings.humMin = 30;
        deviceSettings.humMax = 90;
        deviceSettings.isCelsius = true;
        saveSettingsToEEPROM(); // Zapisz domyślne ustawienia
    }
    Serial.println("Ustawienia załadowane z EEPROM.");
}


ISR(TIMER1_OVF_vect) {
    refreshCounter++;
    
    if (refreshCounter >= 3) { // ~12.57 sekundy przy preskalerze 1024
        refreshFlag = true;
        refreshCounter = 0;
    }

}
//void handleEncoderButton() {
//  if (inMenu) {
//    if (currentScreen == 0) {
//      inMenu = false; // Wyjście do ekranu danych
//    } else if (currentScreen == 1) {
//      // Przejście do ustawień
//      Serial.println("Przejście do ustawień.");
//    }
//  }
//}

