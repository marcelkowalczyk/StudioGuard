#include <Wire.h>
#include <RTClib.h>
#include <U8x8lib.h>
#include "DHT.h"
#include <EEPROM.h>
#include <SPI.h>
#include <Adafruit_SPIFlash.h>

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
#define ADDRESS_SECTOR 0x0000        // Sektor do przechowywania adresu
#define DATA_START_SECTOR 0x1000    // Sektor danych (sektor 1 dla 4kB sektora)

RTC_DS1307 rtc;

Adafruit_FlashTransport_SPI flashTransport(FLASH_CS_PIN, SPI);
Adafruit_SPIFlash flash(&flashTransport);
DHT dht(DHTTYPE);  

// Inicjalizacja OLED SSD1306 w trybie U8X8
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);

// Zmienne globalne
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
// Struktura przechowująca dane do zapisu
struct SensorData {
    uint16_t year;   // 2 bajty
    uint8_t month;   // 1 bajt
    uint8_t day;     // 1 bajt
    uint8_t hour;    // 1 bajt
    uint8_t minute;  // 1 bajt
    uint8_t second;  // 1 bajt
    int16_t temperature; // 2 bajty (przechowujemy temperaturę *10, np. 23.4 jako 234)
    uint8_t humidity;    // 1 bajt
} __attribute__((packed));

// Globalna zmienna do pracy z ustawieniami
Settings deviceSettings;

// Adres początkowy ustawień w EEPROM
const int EEPROM_SETTINGS_ADDR = 0;

bool isEdit = false;
Screen currentScreen = SCREEN_DATA;  // Ekran startowy
int currentMenuOption = 0;
int currentSettingsOption = 0;

int oldGroveButtonState = 0;
int oldEncoderPinA = 0;
int oldEncoderPinB = 0;
int encoderDirection = 0;

// Przykladowe dane dla temperatury i wilgotnosci
float temperature = 22.5;
float humidity = 55.0;

volatile uint16_t refreshCounter = 0;
volatile bool refreshFlag = false;

volatile bool isTransferActive = false; // Flaga dla trybu transferu danych
volatile bool saveDataFlag = false;    // Flaga zapisu danych do Flash
volatile uint16_t saveDataCounter = 0; // Licznik przerwań dla zapisu danych
const uint16_t DATA_SAVE_INTERVAL_TICKS = 6; // 10 minut = 146 przerwań (~4,096 s/przerwanie)
volatile uint32_t flashDataAddress = 0;         // Adres startowy dla zapisu danych na Flash
const uint32_t FLASH_SIZE = 8388608; // Rozmiar pamięci 8 MB (dla W25Q64)

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

  // Inicjalizacja OLED
  u8x8.begin();
  u8x8.setFont(u8x8_font_amstrad_cpc_extended_f);
  dht.begin();
  
  // Inicjalizacja RTC
  if (!rtc.begin()) {
      Serial.println("Błąd inicjalizacji RTC!");
      while (1);
  }
  // Inicjalizacja pamięci Flash
  if (!flash.begin()) {
      Serial.println("Błąd inicjalizacji pamięci Flash!");
      while (1);
  }
  initializeFlashDataAddress(); // Odczytaj ostatni adres zapisu

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  loadSettingsFromEEPROM();

  displaySensorData();
  
  /// Inicjalizacja Timer1
  TCCR1A = 0;                     // Normal mode
  TCCR1B = (1 << CS12) | (1 << CS10); // Prescaler 1024
  TIMSK1 = (1 << TOIE1);          // Włącz przerwanie przepełnienia
  TCNT1 = 0;
  //testFlashWrite(4);
  //testFlashRead(4);
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
          } else {
              Serial.println("Błąd odczytu czujnika.");
          }
      }
  }
  // Zapis danych co 10 minut
  if (saveDataFlag) {
      saveDataFlag = false;
      DateTime now = rtc.now();
      saveDataToFlash(temperature, humidity, now);
  }

  // Sprawdzenie, czy jest sygnał 'T' do rozpoczęcia transferu danych
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'T') { // Sygnał 'T' wyzwala transfer
      // Wyłącz alarmy natychmiast
      digitalWrite(LED_PIN, LOW);
      noTone(BUZZER_PIN);

      // Wykonaj transfer danych
      transferDataToPC();
    }
  }

  // Jeśli transfer trwa, zakończ bieżące wykonanie pętli
  if (isTransferActive) {
      return; // Blokuj pozostałe operacje
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
      saveSettingsToEEPROM();
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
        //Serial.println("Ustawienia zapisane do EEPROM.");
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
    //Serial.println("Ustawienia załadowane z EEPROM.");
}
void saveDataToFlash(float temperature, float humidity, DateTime now) {
  SensorData data;
  data.year = now.year();
  data.month = now.month();
  data.day = now.day();
  data.hour = now.hour();
  data.minute = now.minute();
  data.second = now.second();
  data.temperature = (int16_t)(temperature * 10);
  data.humidity = (uint8_t)humidity;

  // Zapis danych do pamięci Flash
  if (!flash.writeBuffer(flashDataAddress, (uint8_t *)&data, sizeof(SensorData))) {
      Serial.println("Błąd zapisu danych do pamięci Flash!");
      return;
  }

  // Dodanie jednego bajtu przerwy
  uint8_t padding = 0xFF; // Domyślnie wypełniamy 0xFF
  flash.writeBuffer(flashDataAddress + sizeof(SensorData), &padding, 1);

  // Aktualizacja adresu zapisu
  flashDataAddress += sizeof(SensorData) + 1; // Przesuwamy o rozmiar struktury + 1 bajt

  if (flashDataAddress >= FLASH_SIZE) {
      flashDataAddress = DATA_START_SECTOR; // Reset do początku danych (po kontrolnym)
  }

  saveFlashDataAddress(); // Zapis aktualnego adresu zapisu
}

void transferDataToPC() {
  isTransferActive = true;
  detachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A));
  detachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B));

  u8x8.clear();
  u8x8.drawString(2, 3, "Transfer");
  u8x8.drawString(2, 5, "danych");
  uint32_t readAddress = DATA_START_SECTOR;

  while (readAddress < flashDataAddress) {
      SensorData data;
      flash.readBuffer(readAddress, (uint8_t *)&data, sizeof(SensorData));
      // Walidacja danych
      if (data.year > 2000 && data.year < 2100 && data.month >= 1 && data.month <= 12) {
          Serial.print(data.year);
          Serial.print(",");
          Serial.print(data.month);
          Serial.print(",");
          Serial.print(data.day);
          Serial.print(",");
          Serial.print(data.hour);
          Serial.print(",");
          Serial.print(data.minute);
          Serial.print(",");
          Serial.print(data.second);
          Serial.print(",");
          Serial.print(data.temperature / 10.0, 1);
          Serial.print(",");
          Serial.println(data.humidity);
      } else {
          Serial.println("Nieprawidłowe dane w pamięci Flash.");
      }

      // Przesuwamy adres odczytu o rozmiar struktury + 1 bajt
      readAddress += sizeof(SensorData) + 1;
      delay(50); // Unikaj przeciążenia łącza USB
  }

  flash.eraseChip(); // Czyszczenie pamięci Flash po transferze
  flashDataAddress = DATA_START_SECTOR; // Reset adresu zapisu
  saveFlashDataAddress(); // Zapisz zaktualizowany adres

  u8x8.clear();
  u8x8.drawString(2, 3, "Transfer");
  u8x8.drawString(2, 5, "zakonczony");
  delay(2000);

  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), readEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), readEncoder, CHANGE);
  isTransferActive = false;
  displaySensorData();
}

void saveFlashDataAddress() {
  uint32_t addressToSave = flashDataAddress;

  // Wyczyszczenie sektora, w którym przechowywany jest adres
  flash.eraseSector(ADDRESS_SECTOR); 

  // Zapisz adres w pierwszych 4 bajtach sektora 0
  flash.writeBuffer(ADDRESS_SECTOR, (uint8_t *)&addressToSave, sizeof(addressToSave));
}
void initializeFlashDataAddress() {
    uint32_t savedAddress = 0;

    // Odczytaj zapisany adres z pierwszych 4 bajtów sektora 0
    flash.readBuffer(ADDRESS_SECTOR, (uint8_t *)&savedAddress, sizeof(savedAddress));

    // Walidacja adresu
    if (savedAddress >= DATA_START_SECTOR && savedAddress < FLASH_SIZE) {
        flashDataAddress = savedAddress;
    } else {
        flashDataAddress = DATA_START_SECTOR; // Rozpocznij zapis od początku sektora danych
        saveFlashDataAddress(); // Zapisz domyślny adres
    }
}

ISR(TIMER1_OVF_vect) {
  refreshCounter++;
  saveDataCounter++;

  // Odświeżanie co ~12,57 sekundy
  if (refreshCounter >= 3) {
      refreshFlag = true;
      refreshCounter = 0;
  }

  // Zapis danych co 10 minut (146 przerwań)
  if (saveDataCounter >= DATA_SAVE_INTERVAL_TICKS) {
      if (!isTransferActive) { // Zapis danych tylko, gdy transfer nie jest aktywny
          saveDataFlag = true;
      }
      saveDataCounter = 0; // Reset licznika zapisu
  }
}

