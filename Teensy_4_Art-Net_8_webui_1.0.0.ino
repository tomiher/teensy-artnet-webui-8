/*
 * Teensy 4 Art-Net LED Controller v1.0.0
 * 
 * Stable version without ArtPoll support
 * - 8 independent outputs (1020 LEDs each)
 * - Web configuration interface
 * - EEPROM settings storage
 * - Test modes and button control
 * - Universe mapping: 0, 6, 12, 18, 24, 30, 36, 42
 * - Serial debug messages removed for production use
 */

// Teensy natív library-k használata
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <SPI.h>
#include <OctoWS2811.h>  
#include <EEPROM.h>

// Art-Net implementáció Teensy natív library-kkal
EthernetUDP udp;
EthernetServer webServer(80);

// Art-Net konstansok
const int ART_NET_PORT = 6454;
const int ART_DMX = 0x5000;

// Konfigurációs struktúra
struct Config {
  int ledsPerPin[8] = {1020, 1020, 1020, 1020, 1020, 1020, 1020, 1020}; // 1020 pixel/kimenet
  int startUniverse[8] = {0, 6, 12, 18, 24, 30, 36, 42}; // Start Universe minden kimenethez (6 universe/kimenet)
  int startAddress[8] = {1, 1, 1, 1, 1, 1, 1, 1}; // Start Address minden kimenethez (1-510)
  int brightness[8] = {255, 255, 255, 255, 255, 255, 255, 255}; // Brightness minden kimenethez (0-255)
  int ledType = WS2811_GRB | WS2811_800kHz;  // Kombinált érték (kompatibilitás)
  int ledTypeChoice = 2;      // Webfelületi választás (1-5)
  int colorOrderChoice = 18;  // Színsorrend választás (2,6,10,14,18,22)
  byte ip[4] = {10, 10, 0, 42};
  byte subnet[4] = {255, 255, 255, 0};
  byte mac[6] = {0x04, 0xE5, 0xE9, 0x00, 0x69, 0xEC};
  bool testMode = false;
  int testModeType = 0;  // 0=Rainbow, 1=Red, 2=Green, 3=Blue
};

Config currentConfig;

// EEPROM címek
const int EEPROM_CONFIG_ADDR = 0;
const int EEPROM_MAGIC = 0xABD7; // Magic number a konfiguráció ellenőrzéshez (1020 pixel/kimenet, független vezérlés!)

// Egyszerű memória ellenőrző (Teensy 4.1)
extern "C" char* sbrk(int incr);
uint32_t freeMemory() {
  char top;
  char* heap_end = (char*)sbrk(0);
  return (uint32_t)&top - (uint32_t)heap_end;
}

// Nyomógomb és mód kezelés
const int buttonPin = 30;
const int statusLedPin = 13; // Beépített LED
bool isRainbowMode = false;
unsigned long buttonPressStart = 0;
bool buttonPressed = false;
bool lastButtonState = HIGH;

// Teszt mód típusok
enum TestModeType {
  TEST_ARTNET = -1,    // Art-Net mód
  TEST_RAINBOW = 0,    // Rainbow animáció
  TEST_RED = 1,        // Piros szín
  TEST_GREEN = 2,      // Zöld szín
  TEST_BLUE = 3        // Kék szín
};

int currentTestMode = TEST_ARTNET; // Alapértelmezett: Art-Net mód
bool buttonTriggeredTestMode = false; // Gombbal aktivált teszt mód jelzése

// Gomb állapot kezelés
enum ButtonState {
  BUTTON_IDLE,           // Nincs nyomva
  BUTTON_PRESSED_SHORT,  // 0-3 sec
  BUTTON_PRESSED_3SEC,   // 3-10 sec (dupla villogás)
  BUTTON_PRESSED_10SEC,  // 10-20 sec (tripla villogás)
  BUTTON_PRESSED_20SEC   // 20+ sec (folyamatos világítás)
};

ButtonState currentButtonState = BUTTON_IDLE;
unsigned long lastRainbowUpdate = 0;
float rainbowHue = 0.0;

// LED villogás kezelés
unsigned long lastLedUpdate = 0;
bool ledState = false;

const int numPins = 8;
byte pinList[numPins] = {2, 3, 4, 5, 6, 7, 8, 9};


// Array to specify number of LEDs for each pin (maximum values for memory allocation)
const int ledsPerPin[numPins] = {1020, 1020, 1020, 1020, 1020, 1020, 1020, 1020}; // 1020 pixel/kimenet

// Calculate total number of LEDs (must be constexpr for array size)
constexpr int calculateTotalLeds() {
    int total = 0;
    for (int i = 0; i < numPins; i++) {
        total += ledsPerPin[i];
    }
    return total;
}

// Dinamikus LED szám kalkuláció a konfiguráció alapján
int calculateCurrentTotalLeds() {
    int total = 0;
    for (int i = 0; i < numPins; i++) {
        total += currentConfig.ledsPerPin[i];
    }
    return total;
}

const int numLeds = calculateTotalLeds();

// Dinamikus csatorna és byte számítás RGBW támogatással
int getBytesPerLED() {
  return isRGBWLedType() ? 4 : 3;
}

int getNumberOfChannels() {
  return numLeds * getBytesPerLED();
}

// Statikus értékek a memória allokációhoz (worst case: RGBW)
const int bytesPerLED = 4;  // Maximum 4 byte/LED (RGBW) 

// Calculate maximum LEDs per strip (must be constexpr for array size)
constexpr int calculateMaxLedsPerStrip() {
    int max = 0;
    for (int i = 0; i < numPins; i++) {
        if (ledsPerPin[i] > max) {
            max = ledsPerPin[i];
        }
    }
    return max;
}

const int maxLedsPerStrip = calculateMaxLedsPerStrip();

// Calculate memory size (must be constexpr for array size)
constexpr int calculateMemorySize() {
    return maxLedsPerStrip * numPins * bytesPerLED / 4;
}

DMAMEM int displayMemory[calculateMemorySize()];
int drawingMemory[calculateMemorySize()];
// Statikus objektum, de a tényleges configot setup()-ban a leds.begin(...) hívással állítjuk be
OctoWS2811 leds(maxLedsPerStrip, displayMemory, drawingMemory, WS2811_GRB, numPins, pinList);

// Art-Net / DMX alapértékek
const int DMX_CHANNELS_PER_UNIVERSE = 512; // ArtDMX payload tipikusan 512 csatorna

// RGBW LED típus ellenőrzése
bool isRGBWLedType() {
  // Web UI: 5 = SK6812W (RGBW), illetve RGBW színsorrend opciók 100+
  return (currentConfig.ledTypeChoice == 5) || (currentConfig.colorOrderChoice >= 100);
}

// OctoWS2811 config (színsorrend + frekvencia) a web UI választások alapján
uint8_t getOctoWs2811Config() {
  // Frekvencia
  uint8_t freq = WS2811_800kHz;
  if (currentConfig.ledTypeChoice == 3) { // WS2813
    freq = WS2813_800kHz;
  }

  // Színsorrend
  uint8_t order = WS2811_GRB; // default
  switch (currentConfig.colorOrderChoice) {
    // RGB
    case 6:  order = WS2811_RGB; break;
    case 33: order = WS2811_RBG; break;
    case 18: order = WS2811_GRB; break;
    case 36: order = WS2811_GBR; break;
    case 24: order = WS2811_BRG; break;
    case 9:  order = WS2811_BGR; break;

    // RGBW
    case 100: order = WS2811_RGBW; break;
    case 102: order = WS2811_RBGW; break;
    case 101: order = WS2811_GRBW; break;
    case 103: order = WS2811_GBRW; break;
    case 105: order = WS2811_BRGW; break;
    case 104: order = WS2811_BGRW; break;
    case 106: order = WS2811_WRGB; break;
    default: break;
  }

  // Ha RGBW LED van választva, de RGB színsorrend maradt, adjunk értelmes defaultot
  if (isRGBWLedType() && order < WS2811_RGBW) {
    order = WS2811_GRBW; // tipikus SK6812 RGBW
  }

  return order | freq;
}

// End Universe számítása Start Universe, Start Address és Pixel szám alapján
int calculateEndUniverse(int startUniverse, int startAddress, int numPixels) {
  if (numPixels == 0) return startUniverse;
  
  int channelsPerPixel = isRGBWLedType() ? 4 : 3;
  int totalChannels = numPixels * channelsPerPixel;
  int availableChannelsInFirstUniverse = DMX_CHANNELS_PER_UNIVERSE - (startAddress - 1); // Elérhető csatornák az első universe-ben
  
  if (totalChannels <= availableChannelsInFirstUniverse) {
    // Minden pixel belefér az első universe-be
    return startUniverse;
  } else {
    // Több universe szükséges
    int remainingChannels = totalChannels - availableChannelsInFirstUniverse;
    int additionalUniverses = (remainingChannels + (DMX_CHANNELS_PER_UNIVERSE - 1)) / DMX_CHANNELS_PER_UNIVERSE; // Felfelé kerekítés
    return startUniverse + additionalUniverses;
  }
}

// Overload a régi hívásokhoz (kompatibilitás)
int calculateEndUniverse(int startUniverse, int numPixels) {
  return calculateEndUniverse(startUniverse, 1, numPixels); // Alapértelmezett Start Address = 1
}

// End Address számítása
int calculateEndAddress(int startAddress, int numPixels) {
  if (numPixels == 0) return startAddress;
  
  int channelsPerPixel = isRGBWLedType() ? 4 : 3;
  int channelsUsed = numPixels * channelsPerPixel;
  int availableInFirst = DMX_CHANNELS_PER_UNIVERSE - (startAddress - 1);
  
  if (channelsUsed <= availableInFirst) {
    // Minden pixel belefér az első universe-be
    return startAddress + channelsUsed - 1;
  } else {
    // Több universe szükséges - az utolsó universe-ben lévő utolsó cím
    int remainingChannels = channelsUsed - availableInFirst;
    return ((remainingChannels - 1) % DMX_CHANNELS_PER_UNIVERSE) + 1;
  }
}
void setPixelWithColorOrder(int pixel, int r, int g, int b, int w = 0);

// Brightness alkalmazása RGB értékekre
void applyBrightness(int &r, int &g, int &b, int brightness) {
  r = (r * brightness) / 255;
  g = (g * brightness) / 255;
  b = (b * brightness) / 255;
}

// Pixel beállítása brightness-szel és színsorrenddel (kimenet alapján)
void setPixelWithBrightnessAndColorOrder(int pixel, int r, int g, int b, int outputPin, int w = 0) {
  // Brightness alkalmazása
  int brightness = currentConfig.brightness[outputPin];
  applyBrightness(r, g, b, brightness);
  
  // Színsorrend alkalmazása
  setPixelWithColorOrder(pixel, r, g, b, w);
}

// Színsorrend konverzió a currentConfig.ledType alapján (RGB és RGBW)
void setPixelWithColorOrder(int pixel, int r, int g, int b, int w) {
  // RGB színsorrend értékek: 6, 9, 18, 24, 33, 36
  // RGBW színsorrend értékek: 100-105
  
  if (currentConfig.ledType >= 100) {
    // RGBW LED kezelés
    switch (currentConfig.ledType) {
      case 100: // RGBW
        leds.setPixel(pixel, r, g, b, w);
        break;
      case 101: // GRBW
        leds.setPixel(pixel, g, r, b, w);
        break;
      case 102: // RBGW
        leds.setPixel(pixel, r, b, g, w);
        break;
      case 103: // GBRW
        leds.setPixel(pixel, g, b, r, w);
        break;
      case 104: // BGRW
        leds.setPixel(pixel, b, g, r, w);
        break;
      case 105: // BRGW
        leds.setPixel(pixel, b, r, g, w);
        break;
      case 106: // WRGB
        leds.setPixel(pixel, w, r, g, b);
        break;
      default:  // Alapértelmezett RGBW
        leds.setPixel(pixel, r, g, b, w);
        break;
    }
  } else {
    // RGB LED kezelés (eredeti logika)
    switch (currentConfig.ledType) {
      case 6:   // RGB
        leds.setPixel(pixel, r, g, b);
        break;
      case 9:   // BGR
        leds.setPixel(pixel, b, g, r);
        break;
      case 18:  // GRB (alapértelmezett)
        leds.setPixel(pixel, g, r, b);
        break;
      case 24:  // BRG
        leds.setPixel(pixel, b, r, g);
        break;
      case 33:  // RBG
        leds.setPixel(pixel, r, b, g);
        break;
      case 36:  // GBR
        leds.setPixel(pixel, g, b, r);
        break;
      default:  // Alapértelmezett GRB
        leds.setPixel(pixel, g, r, b);
        break;
    }
  }
}

// Artnet settings
// Art-Net buffer
byte artnetPacket[530];
const int startUniverse = 0;

// Universe tracking for proper leds.show() timing - működő kód logikája
const int maxChannels = numLeds * 4;  // Maximum 4 csatorna/LED (RGBW)
const int maxUniverses = maxChannels / DMX_CHANNELS_PER_UNIVERSE + ((maxChannels % DMX_CHANNELS_PER_UNIVERSE) ? 1 : 0);
bool universesReceived[64]; // Maximum 64 universe support
bool sendFrame = true;
int previousDataLength = 0;

// Change ip and mac address for your setup 
byte ip[] = {10, 10, 0, 42};
byte broadcast[] = {255, 255, 255, 0};
byte mac[] = {0x04, 0xE5, 0xE9, 0x00, 0x69, 0xEC};

void setup()
{
  delay(1000); // Várunk a Serial kapcsolatra
  
  // Memóriahiány ellenőrzés - EEPROM reset ha szükséges
  checkMemoryAndResetIfNeeded();
  
  // Nyomógomb beállítása pullup ellenállással
  pinMode(buttonPin, INPUT_PULLUP);
  
  // Status LED beállítása
  pinMode(statusLedPin, OUTPUT);
  digitalWrite(statusLedPin, LOW);
  
  // Konfiguráció betöltése EEPROM-ból
  
  if (!loadConfigFromEEPROM()) {
    // Alapértelmezett értékek beállítása - kimenetenkénti független vezérlés
    for (int i = 0; i < 8; i++) {
      currentConfig.ledsPerPin[i] = 1020; // 1020 pixel/kimenet
    }
    for (int i = 0; i < 4; i++) {
      currentConfig.ip[i] = ip[i];
      currentConfig.subnet[i] = broadcast[i];
    }
    for (int i = 0; i < 6; i++) {
      currentConfig.mac[i] = mac[i];
    }
    currentConfig.ledType = WS2811_GRB;  // Alapértelmezett GRB színsorrend
    currentConfig.ledTypeChoice = 2;     // WS2812/B
    currentConfig.colorOrderChoice = 18; // GRB
    currentConfig.testMode = isRainbowMode;
    currentConfig.testModeType = 0;      // Rainbow
    
    // Start Universe, Start Address és Brightness alapértelmezett értékek
    for (int i = 0; i < 8; i++) {
      currentConfig.startUniverse[i] = i * 6;   // 0, 6, 12, 18, 24, 30, 36, 42
      currentConfig.startAddress[i] = 1;    // Minden kimenet 1-es címről indul
      currentConfig.brightness[i] = 255;    // Teljes fényerő
    }
    
    // Alapértelmezett konfiguráció mentése
    saveConfigToEEPROM();
  }
  
  // Konfiguráció alkalmazása
  applyConfiguration();
  
  
  // Debug üzenetek eltávolítva
  
  // ArtNet inicializálása (ez már kezeli az Ethernet-et)
  
  // LED konfiguráció info eltávolítva
  
  // Natív Ethernet inicializálás
  Ethernet.begin(mac, ip);
  udp.begin(ART_NET_PORT);
  
  // Több univerzum támogatás beállítása
  for (int i = 0; i < 8; i++) {
  }
  
  // LED driver init a web UI választások alapján (RGB vs RGBW + színsorrend + frekvencia)
  leds.begin(maxLedsPerStrip, displayMemory, drawingMemory, getOctoWs2811Config(), numPins, pinList);
  
  // Várunk egy kicsit az Ethernet inicializálására
  delay(2000);

  // Art-Net callback már nincs szükség rá - közvetlen UDP olvasás
  
  // Webszerver inicializálása
  webServer.begin();
  
  // Memória státusz
}

void loop()
{
  // Art-Net csomagok olvasása natív UDP-vel
  readArtNet();
  
  // Webszerver kérések kezelése
  handleWebServer();
  
  // Státusz kiírás eltávolítva
  
  // Gomb kezelés
  handleButton();
  
  // Status LED villogás
  updateStatusLed();
  
  // Teszt mód animációk frissítése
  if (currentTestMode == TEST_RAINBOW || (isRainbowMode && !buttonTriggeredTestMode)) {
    updateRainbow();
  } else if (currentTestMode >= TEST_RED && currentTestMode <= TEST_BLUE) {
    updateSolidColor();
  }
}

void readArtNet() {
  int packetSize = udp.parsePacket();
  if (packetSize >= 18) {  // Minimum Art-Net header méret
    udp.read(artnetPacket, min(packetSize, 530));
    
    // Art-Net header ellenőrzés: "Art-Net\0"
    if (memcmp(artnetPacket, "Art-Net\0", 8) == 0) {
      // OpCode ellenőrzés (ART_DMX = 0x5000)
      uint16_t opcode = artnetPacket[8] | (artnetPacket[9] << 8);
      if (opcode == ART_DMX) {
        // Universe kinyerése
        uint16_t universe = artnetPacket[14] | (artnetPacket[15] << 8);
        uint16_t length = (artnetPacket[16] << 8) | artnetPacket[17];
        
        // DMX adatok a 18. byte-tól kezdődnek
        onDmxFrame(universe, length, 0, &artnetPacket[18], udp.remoteIP());
      }
    }
  }
}

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data, IPAddress remoteIP)
{
  if (currentTestMode != TEST_ARTNET) return; // Skip Art-Net data in test mode
  
  // Debug: logoljuk a beérkező univerzumokat (ritkábban)
  static unsigned long lastUniverseLog = 0;
  if (millis() - lastUniverseLog > 10000) { // 10 másodpercenként
    lastUniverseLog = millis();
  }
  
  bool dataChanged = false;
  
  // Kimenetenkénti független vezérlés logika
  // Minden kimenet saját universe tartománnyal rendelkezik
  const int channelsPerPixel = isRGBWLedType() ? 4 : 3;
  const int pixelsPerFullUniverse = DMX_CHANNELS_PER_UNIVERSE / channelsPerPixel; // RGB: 170, RGBW: 128

  for (int pin = 0; pin < numPins; pin++) {
    if (currentConfig.ledsPerPin[pin] == 0) continue; // Skip empty outputs
    
    int pinStartUni = currentConfig.startUniverse[pin];
    int pinStartAddr = currentConfig.startAddress[pin];
    int pinEndUni = calculateEndUniverse(pinStartUni, pinStartAddr, currentConfig.ledsPerPin[pin]);
    
    // Ellenőrizzük, hogy ez a universe ehhez a kimenethez tartozik-e
    if (universe < pinStartUni || universe > pinEndUni) continue;
    
    // Számítsuk ki, hogy melyik pixeltől kezdjük ezen a universe-en
    int pixelOffset = 0;
    int channelOffset = pinStartAddr - 1; // 0-based indexing
    
    if (universe > pinStartUni) {
      // Nem az első universe - számítsuk ki az offset-et
      int channelsInFirstUni = DMX_CHANNELS_PER_UNIVERSE - (pinStartAddr - 1);
      int pixelsInFirstUni = channelsInFirstUni / channelsPerPixel;
      pixelOffset = pixelsInFirstUni + (universe - pinStartUni - 1) * pixelsPerFullUniverse;
      channelOffset = 0; // Következő universe-ek 0-tól kezdődnek
    }
    
    // Feldolgozzuk a pixeleket ebben a universe-ben
    int maxPixelsInThisUni = (DMX_CHANNELS_PER_UNIVERSE - channelOffset) / channelsPerPixel;
    int pixelsToProcess = min(maxPixelsInThisUni, currentConfig.ledsPerPin[pin] - pixelOffset);
    
    for (int i = 0; i < pixelsToProcess && (channelOffset + i * channelsPerPixel + (channelsPerPixel - 1)) < (int)length; i++) {
      int pixelIndex = pixelOffset + i;
      if (pixelIndex >= currentConfig.ledsPerPin[pin]) break;
      
      int dataIndex = channelOffset + i * channelsPerPixel;
      if (dataIndex + 2 < (int)length) {
        const int w = (channelsPerPixel == 4 && (dataIndex + 3) < (int)length) ? data[dataIndex + 3] : 0;
        setPixelWithBrightnessAndColorOrder(
          pin * maxLedsPerStrip + pixelIndex, 
          data[dataIndex], 
          data[dataIndex + 1], 
          data[dataIndex + 2], 
          pin,
          w
        );
        dataChanged = true; // Jelezzük, hogy változtak az adatok
      }
    }
  }
  
  if (dataChanged) {
    leds.show();
    // Reset universeReceived to false
    memset(universesReceived, 0, sizeof(universesReceived));
  }
}


// Új gomb kezelés - felengedéskor aktiválódik a funkció
void handleButton() {
  bool buttonReading = digitalRead(buttonPin);
  
  // Gomb lenyomás kezdete
  if (lastButtonState == HIGH && buttonReading == LOW) {
    buttonPressStart = millis();
    buttonPressed = true;
    currentButtonState = BUTTON_PRESSED_SHORT;
  }
  
  // Gomb felengedés - funkció aktiválása a tartam alapján
  if (lastButtonState == LOW && buttonReading == HIGH && buttonPressed) {
    // Funkció aktiválása a gomb állapot alapján
    switch (currentButtonState) {
      case BUTTON_PRESSED_SHORT:
        // Rövid nyomás (0-3 sec)
        if (currentTestMode == TEST_ARTNET) {
        } else {
          // Teszt módban - körbe lépés a módokon
          if (currentTestMode == TEST_BLUE) {
            currentTestMode = TEST_RAINBOW;
            isRainbowMode = true;
          } else {
            currentTestMode++;
            isRainbowMode = (currentTestMode == TEST_RAINBOW);
          }
        }
      break;
        
      case BUTTON_PRESSED_3SEC:
        // 3-10 sec - teszt mód váltás
        if (currentTestMode == TEST_ARTNET) {
          currentTestMode = TEST_RAINBOW;
          buttonTriggeredTestMode = true;
          isRainbowMode = true;
          rainbowHue = 0.0;
        } else {
          currentTestMode = TEST_ARTNET;
          buttonTriggeredTestMode = false;
          isRainbowMode = false;
          clearAllLeds();
        }
        break;
        
      case BUTTON_PRESSED_10SEC:
        // 10-20 sec - network reset
        resetNetworkToDefault();
        break;
        
      case BUTTON_PRESSED_20SEC:
        // 20+ sec - teljes reset
        resetConfigToDefault();
        break;
        
      default:
        break;
    }
    
    // Reset állapot
    buttonPressed = false;
    currentButtonState = BUTTON_IDLE;
  }
  
  // Gomb állapot frissítése nyomás közben
  if (buttonPressed) {
    unsigned long pressDuration = millis() - buttonPressStart;
    
    if (pressDuration >= 20000) {
      if (currentButtonState != BUTTON_PRESSED_20SEC) {
        currentButtonState = BUTTON_PRESSED_20SEC;
      }
    } else if (pressDuration >= 10000) {
      if (currentButtonState != BUTTON_PRESSED_10SEC) {
        currentButtonState = BUTTON_PRESSED_10SEC;
      }
    } else if (pressDuration >= 3000) {
      if (currentButtonState != BUTTON_PRESSED_3SEC) {
        currentButtonState = BUTTON_PRESSED_3SEC;
      }
    }
  }
  
  lastButtonState = buttonReading;
}

// LED-ek törlése (fekete)
void clearAllLeds() {
  int currentTotalLeds = calculateCurrentTotalLeds();
  for (int i = 0; i < currentTotalLeds; i++) {
      // Calculate which pin and position this LED belongs to
      int currentLed = 0;
      int pin = 0;
      int position = 0;
      
      // Find the correct pin and position for this LED
    while (pin < numPins && currentLed + currentConfig.ledsPerPin[pin] <= i) {
      currentLed += currentConfig.ledsPerPin[pin];
        pin++;
      }
      
      if (pin < numPins) {
      position = i - currentLed;
      if (position < currentConfig.ledsPerPin[pin]) {
        leds.setPixel(pin * maxLedsPerStrip + position, 0, 0, 0);  // Fekete
      }
    }
  }
  leds.show();
}

// Network beállítások reset alapértékre
void resetNetworkToDefault() {
  
  // Alapértelmezett network értékek
  byte defaultIP[] = {10, 10, 0, 42};
  byte defaultSubnet[] = {255, 255, 255, 0};
  byte defaultMAC[] = {0x04, 0xE5, 0xE9, 0x00, 0x69, 0xEC};
  
  // Network beállítások frissítése
  for (int i = 0; i < 4; i++) {
    currentConfig.ip[i] = defaultIP[i];
    currentConfig.subnet[i] = defaultSubnet[i];
  }
  for (int i = 0; i < 6; i++) {
    currentConfig.mac[i] = defaultMAC[i];
  }
  
  for (int i = 0; i < 4; i++) {
  }
  
  // EEPROM mentés és újraindítás
  saveConfigToEEPROM();
  delay(3000);
  SCB_AIRCR = 0x05FA0004; // Teensy restart
}

// Teljes konfiguráció reset
void resetConfigToDefault() {
  
  // Network beállítások
  byte defaultIP[] = {10, 10, 0, 42};
  byte defaultSubnet[] = {255, 255, 255, 0};
  byte defaultMAC[] = {0x04, 0xE5, 0xE9, 0x00, 0x69, 0xEC};
  
  for (int i = 0; i < 4; i++) {
    currentConfig.ip[i] = defaultIP[i];
    currentConfig.subnet[i] = defaultSubnet[i];
  }
  for (int i = 0; i < 6; i++) {
    currentConfig.mac[i] = defaultMAC[i];
  }
  
  // Output konfiguráció - kimenetenkénti független vezérlés
  for (int i = 0; i < 8; i++) {
    currentConfig.ledsPerPin[i] = 1020;
    currentConfig.startUniverse[i] = i * 6;      // 0, 6, 12, 18, 24, 30, 36, 42
    currentConfig.startAddress[i] = 1;       // Minden kimenet 1-es címről
    currentConfig.brightness[i] = 255;       // Teljes fényerő
  }
  
  // LED beállítások - WS2812, RGB
  currentConfig.ledType = WS2811_GRB;        // GRB színsorrend
  currentConfig.ledTypeChoice = 2;           // WS2812/B
  currentConfig.colorOrderChoice = 18;       // GRB
  
  // Test mód kikapcsolás
  currentConfig.testMode = false;
  currentConfig.testModeType = 0;            // Rainbow
  
  
  // EEPROM mentés és újraindítás
  saveConfigToEEPROM();
  delay(3000);
  SCB_AIRCR = 0x05FA0004; // Teensy restart
}

// Konstans szín megjelenítés (piros, zöld, kék)
void updateSolidColor() {
  uint8_t r = 0, g = 0, b = 0;
  
  // Szín beállítása a teszt mód szerint
  switch (currentTestMode) {
    case TEST_RED:
      r = 255; g = 0; b = 0;
      break;
    case TEST_GREEN:
      r = 0; g = 255; b = 0;
      break;
    case TEST_BLUE:
      r = 0; g = 0; b = 255;
      break;
    default:
      return; // Ismeretlen mód
  }
  
  // Minden LED beállítása a kiválasztott színre
  int currentTotalLeds = calculateCurrentTotalLeds();
  for (int i = 0; i < currentTotalLeds; i++) {
    // Calculate which pin and position this LED belongs to
    int currentLed = 0;
    int pin = 0;
    int position = 0;
    
    // Find the correct pin and position for this LED
    while (pin < numPins && currentLed + currentConfig.ledsPerPin[pin] <= i) {
      currentLed += currentConfig.ledsPerPin[pin];
      pin++;
    }
    
    if (pin < numPins) {
      position = i - currentLed;
      if (position < currentConfig.ledsPerPin[pin]) {
        setPixelWithBrightnessAndColorOrder(pin * maxLedsPerStrip + position, r, g, b, pin);
      }
    }
  }
  leds.show();
}

// Rainbow color effekt
void updateRainbow() {
  unsigned long currentTime = millis();
  
  // Frissítés 50ms-enként (20 FPS)
  if (currentTime - lastRainbowUpdate >= 50) {
    lastRainbowUpdate = currentTime;
    
    int currentTotalLeds = calculateCurrentTotalLeds();
      
    for (int i = 0; i < currentTotalLeds; i++) {
      int r, g, b;
      
      // Test mód típus alapján színek beállítása
      switch (currentConfig.testModeType) {
        case 0: // Rainbow
          {
            // Minden LED különböző árnyalatot kap
            float ledHue = rainbowHue + (i * 360.0 / currentTotalLeds);
            if (ledHue >= 360.0) ledHue -= 360.0;
      hsvToRgb(ledHue, 1.0, 0.5, &r, &g, &b); // 50% fényerő
          }
          break;
        case 1: // Red
          r = 128; g = 0; b = 0;
          break;
        case 2: // Green
          r = 0; g = 128; b = 0;
          break;
        case 3: // Blue
          r = 0; g = 0; b = 128;
          break;
        default: // Alapértelmezett Rainbow
          {
            float ledHue = rainbowHue + (i * 360.0 / currentTotalLeds);
            if (ledHue >= 360.0) ledHue -= 360.0;
            hsvToRgb(ledHue, 1.0, 0.5, &r, &g, &b);
          }
          break;
      }
      
      // Calculate which pin and position this LED belongs to
      int currentLed = 0;
      int pin = 0;
      int position = 0;
      
      // Find the correct pin and position for this LED
      while (pin < numPins && currentLed + currentConfig.ledsPerPin[pin] <= i) {
        currentLed += currentConfig.ledsPerPin[pin];
        pin++;
      }
      
      if (pin < numPins) {
        position = i - currentLed;
        if (position < currentConfig.ledsPerPin[pin]) {
          setPixelWithBrightnessAndColorOrder(pin * maxLedsPerStrip + position, r, g, b, pin);
        }
      }
    }
    
    leds.show();
    
    // Hue léptetése
    rainbowHue += 2.0;
    if (rainbowHue >= 360.0) rainbowHue = 0.0;
  }
}

// HSV to RGB konvertálás
void hsvToRgb(float h, float s, float v, int* r, int* g, int* b) {
  float c = v * s;
  float x = c * (1 - abs(fmod(h / 60.0, 2) - 1));
  float m = v - c;
  
  float r_temp, g_temp, b_temp;
  
  if (h >= 0 && h < 60) {
    r_temp = c; g_temp = x; b_temp = 0;
  } else if (h >= 60 && h < 120) {
    r_temp = x; g_temp = c; b_temp = 0;
  } else if (h >= 120 && h < 180) {
    r_temp = 0; g_temp = c; b_temp = x;
  } else if (h >= 180 && h < 240) {
    r_temp = 0; g_temp = x; b_temp = c;
  } else if (h >= 240 && h < 300) {
    r_temp = x; g_temp = 0; b_temp = c;
  } else {
    r_temp = c; g_temp = 0; b_temp = x;
  }
  
  *r = (int)((r_temp + m) * 255);
  *g = (int)((g_temp + m) * 255);
  *b = (int)((b_temp + m) * 255);
}

// Status LED villogás kezelése új mintákkal
void updateStatusLed() {
  unsigned long currentTime = millis();
  
  if (buttonPressed) {
    // Gomb nyomás alatt - speciális minták
    updateButtonPressLedPattern(currentTime);
  } else {
    // Normál működés - mód szerint
    unsigned long blinkInterval;
    if (currentTestMode != TEST_ARTNET) {
    blinkInterval = 1000; // Lassú villogás teszt módban (1 másodperc)
  } else {
      blinkInterval = 100;  // Kétszer gyorsabb villogás ArtNet módban (0.1 másodperc)
  }
  
    // Egyszerű villogás
  if (currentTime - lastLedUpdate >= blinkInterval) {
    lastLedUpdate = currentTime;
    ledState = !ledState;
    digitalWrite(statusLedPin, ledState ? HIGH : LOW);
    }
  }
}

// Gomb nyomás LED minták
void updateButtonPressLedPattern(unsigned long currentTime) {
  
  switch (currentButtonState) {
    case BUTTON_PRESSED_SHORT:
      // Egyszerű lassú villogás
      if (currentTime - lastLedUpdate >= 500) {
        lastLedUpdate = currentTime;
        ledState = !ledState;
        digitalWrite(statusLedPin, ledState ? HIGH : LOW);
      }
      break;
      
    case BUTTON_PRESSED_3SEC:
      // Dupla villogás fél másodperces szünetekkel
      updateBlinkPattern(currentTime, 2, 200, 500); // 2 villogás, 200ms villogás, 500ms szünet
      break;
      
    case BUTTON_PRESSED_10SEC:
      // Tripla villogás fél másodperces szünetekkel
      updateBlinkPattern(currentTime, 3, 200, 500); // 3 villogás, 200ms villogás, 500ms szünet
      break;
      
    case BUTTON_PRESSED_20SEC:
      // Folyamatos világítás
      digitalWrite(statusLedPin, HIGH);
      break;
      
    default:
      break;
  }
}

// Villogási minta kezelő
void updateBlinkPattern(unsigned long currentTime, int blinkCount, int blinkDuration, int pauseDuration) {
  static unsigned long patternStart = 0;
  static int currentBlink = 0;
  static bool blinkState = false;
  static unsigned long lastBlinkChange = 0;
  
  // Minta újraindítása - unsigned long típusokkal
  unsigned long totalPatternDuration = (unsigned long)(blinkCount * blinkDuration * 2 + pauseDuration);
  if (currentTime - patternStart >= totalPatternDuration) {
    patternStart = currentTime;
    currentBlink = 0;
    blinkState = false;
    lastBlinkChange = currentTime;
  }
  
  if (currentBlink < blinkCount) {
    // Villogás fázis - unsigned long típusokkal
    unsigned long blinkDur = (unsigned long)blinkDuration;
    if (currentTime - lastBlinkChange >= blinkDur) {
      blinkState = !blinkState;
      digitalWrite(statusLedPin, blinkState ? HIGH : LOW);
      lastBlinkChange = currentTime;
      if (!blinkState) {
        currentBlink++;
      }
    }
  } else {
    // Szünet fázis
    digitalWrite(statusLedPin, LOW);
  }
}

// Webszerver kérések kezelése
void handleWebServer() {
  EthernetClient client = webServer.available();
  if (client) {
    // Webszerver kapcsolat log eltávolítva
    String request = "";
    String line = "";
    String postData = "";
    int contentLength = 0;
    
    // HTTP fejlécek olvasása timeout-tal
    unsigned long startTime = millis();
    while (client.connected() && client.available() && (millis() - startTime < 5000)) {
      char c = client.read();
      if (c == '\n') {
        if (line.length() == 0) {
          break; // Üres sor = fejlécek vége
        }
        if (request.length() == 0) {
          request = line; // Első sor a kérés
        }
        // Content-Length keresése
        if (line.startsWith("Content-Length: ")) {
          contentLength = line.substring(16).toInt();
        }
        line = "";
      } else if (c != '\r') {
        line += c;
      }
    }
    
    // POST adatok olvasása ha van, timeout-tal
    if (contentLength > 0 && request.startsWith("POST")) {
      unsigned long postStartTime = millis();
      for (int i = 0; i < contentLength && client.available() && (millis() - postStartTime < 3000); i++) {
        postData += (char)client.read();
      }
    }
    
    // HTTP válasz küldése
    // HTTP kérés log eltávolítva
    
    if (request.startsWith("GET / ")) {
      sendConfigPage(client);
    } else if (request.startsWith("POST /config")) {
      handleConfigPost(client, postData);
    } else if (request.startsWith("POST /testmode")) {
      handleTestModeToggle(client, postData);
    } else if (request.startsWith("GET /api/config")) {
      sendConfigJSON(client);
    } else if (request.startsWith("GET /api/status")) {
      // Status request log eltávolítva
      sendStatusJSON(client);
    } else {
      send404(client);
    }
    
    client.stop();
    // Kapcsolat lezárva log eltávolítva
  }
}

// Konfigurációs oldal küldése
void sendConfigPage(EthernetClient& client) {
  // Debug üzenet eltávolítva
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=utf-8");
  client.println("Cache-Control: no-cache, no-store, must-revalidate");
  client.println("Pragma: no-cache");
  client.println("Expires: 0");
  client.println("Connection: close");
  client.println();
  client.flush(); // Fejlécek azonnali küldése
  
  // HTML oldal küldése - kapcsolat ellenőrzéssel
  if (!client.connected()) return;
  
  client.println("<!DOCTYPE html>");
  client.println("<html><head>");
  client.println("<title>Teensy 4 Art-Net Configuration</title>");
  client.println("<meta charset='utf-8'>");
  client.println("<style>");
  client.println("body { font-family: Arial, sans-serif; margin: 20px; background: #000000; color: #ffffff; }");
  client.println(".container { max-width: 800px; margin: 0 auto; background: #333333; padding: 20px; border-radius: 10px; }");
  client.println("h1 { color: #ffffff; text-align: center; }");
  client.println("h3 { color: #ffffff; margin-top: 0; }");
  client.println(".section { margin: 20px 0; padding: 15px; background: #444444; border-radius: 5px; }");
  client.println("label { display: inline-block; width: 150px; margin-bottom: 5px; color: #ffffff; }");
  client.println("input[type=text], input[type=number], select { width: 100px; padding: 5px; margin: 2px; background: #555555; color: #ffffff; border: 1px solid #666666; }");
  client.println("input[type=checkbox] { margin: 5px; }");
  client.println("button { background: #007cba; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 10px 5px; }");
  client.println("button:hover { background: #005a8b; }");
  client.println(".pixel-inputs { display: grid; grid-template-columns: repeat(4, 1fr); gap: 10px; }");
  client.println(".mac-input { width: 30px !important; }");
  client.println(".status-message { margin: 10px 0; padding: 10px; border-radius: 5px; }");
  client.println(".success { background: #006600; color: white; }");
  client.println(".info { background: #0066cc; color: white; }");
  client.println("</style>");
  client.println("</head><body>");
  client.flush(); // CSS küldése után flush
  
  client.println("<div class='container'>");
  client.println("<h1>Teensy 4 Art-Net Configuration</h1>");
  
  client.println("<form method='POST' action='/config'>");
  
  // Kapcsolat ellenőrzés
  if (!client.connected()) return;
  
  // Pixel számok
  client.println("<div class='section'>");
  client.println("<h3>Output Configuration</h3>");
  
  // Fejléc sor - középre igazított táblázat
  client.println("<div style='display: flex; justify-content: center; margin-bottom: 10px;'>");
  client.println("<div style='display: grid; grid-template-columns: 60px 100px 100px 100px 100px 100px 100px; gap: 8px; font-weight: bold;'>");
  client.println("<div style='text-align: center;'>Output</div>");
  client.println("<div style='text-align: center;'>Start<br>Universe</div>");
  client.println("<div style='text-align: center;'>Start<br>Address</div>");
  client.println("<div style='text-align: center;'>Num<br>Pixels</div>");
  client.println("<div style='text-align: center;'>End<br>Universe</div>");
  client.println("<div style='text-align: center;'>End<br>Address</div>");
  client.println("<div style='text-align: center;'>Brightness</div>");
  client.println("</div>");
  client.println("</div>");
  
  // Adatsorok - középre igazított táblázat
  for (int i = 0; i < 8; i++) {
    client.println("<div style='display: flex; justify-content: center; margin-bottom: 5px;'>");
    client.println("<div style='display: grid; grid-template-columns: 60px 100px 100px 100px 100px 100px 100px; gap: 8px; align-items: center;'>");
    
    // Output szám
    client.print("<div style='text-align: center;'>");
    client.print(i + 1);
    client.println("</div>");
    
    // Start Universe
    client.print("<input type='number' name='startUni");
    client.print(i);
    client.print("' value='");
    client.print(currentConfig.startUniverse[i]);
    client.print("' min='0' max='999' style='width: 80px; text-align: center;' tabindex='");
    client.print(i * 10 + 1);
    client.println("' oninput='updateEndUniverse(" + String(i) + ")' onchange='updateEndUniverse(" + String(i) + ")'>");
    
    // Start Address
    client.print("<input type='number' name='startAddr");
    client.print(i);
    client.print("' value='");
    client.print(currentConfig.startAddress[i]);
    client.print("' min='1' max='510' style='width: 80px; text-align: center;' tabindex='");
    client.print(i * 10 + 2);
    client.println("' oninput='updateEndUniverse(" + String(i) + ")' onchange='updateEndUniverse(" + String(i) + ")'>");
    
    // Num Pixels
    client.print("<input type='number' name='pixels");
    client.print(i);
    client.print("' value='");
    client.print(currentConfig.ledsPerPin[i]);
    client.print("' min='0' max='1020' style='width: 80px; text-align: center;' tabindex='");
    client.print(i * 10 + 3);
    client.println("' oninput='updateEndUniverse(" + String(i) + ")' onchange='updateEndUniverse(" + String(i) + ")'>");
    
    // End Universe (read-only)
    client.print("<input type='text' id='endUni");
    client.print(i);
    client.print("' value='");
    client.print(calculateEndUniverse(currentConfig.startUniverse[i], currentConfig.startAddress[i], currentConfig.ledsPerPin[i]));
    client.println("' readonly tabindex='-1' style='width: 80px; background-color: #000000; color: #ffffff; text-align: center;'>");
    
    // End Address (read-only)
    client.print("<input type='text' id='endAddr");
    client.print(i);
    client.print("' value='");
    client.print(calculateEndAddress(currentConfig.startAddress[i], currentConfig.ledsPerPin[i]));
    client.println("' readonly tabindex='-1' style='width: 80px; background-color: #000000; color: #ffffff; text-align: center;'>");
    
    // Brightness
    client.print("<input type='number' name='brightness");
    client.print(i);
    client.print("' value='");
    client.print(currentConfig.brightness[i]);
    client.print("' min='0' max='255' style='width: 80px; text-align: center;' tabindex='");
    client.print(i * 10 + 4);
    client.println("'>");
    
    client.println("</div>");
    client.println("</div>");
  }
  client.println("</div>");
  client.flush(); // Output Configuration táblázat után flush
  
  // Kapcsolat ellenőrzés
  if (!client.connected()) return;
  
  // LED típus és színsorrend
  client.println("<div class='section'>");
  client.println("<h3>LED Configuration</h3>");
  
  // LED Type kiválasztás - dinamikus selected értékekkel
  client.println("<label>LED Type:</label>");
  client.println("<select name='ledtype' style='width: 150px;'>");
  
  client.print("<option value='1'");
  if (currentConfig.ledTypeChoice == 1) client.print(" selected");
  client.println(">WS2811</option>");
  
  client.print("<option value='2'");
  if (currentConfig.ledTypeChoice == 2) client.print(" selected");
  client.println(">WS2812/WS2812B</option>");
  
  client.print("<option value='3'");
  if (currentConfig.ledTypeChoice == 3) client.print(" selected");
  client.println(">WS2813</option>");
  
  client.print("<option value='4'");
  if (currentConfig.ledTypeChoice == 4) client.print(" selected");
  client.println(">SK6812</option>");
  
  client.print("<option value='5'");
  if (currentConfig.ledTypeChoice == 5) client.print(" selected");
  client.println(">SK6812W</option>");
  
  client.println("</select><br>");
  
  // Color Order kiválasztás - minden opció látható
  client.println("<label>Color Order:</label>");
  client.println("<select name='colororder'>");
  
  // RGB opciók
  client.print("<option value='6'");
  if (currentConfig.colorOrderChoice == 6) client.print(" selected");
  client.println(">RGB</option>");
  
  client.print("<option value='18'");
  if (currentConfig.colorOrderChoice == 18) client.print(" selected");
  client.println(">GRB</option>");
  
  client.print("<option value='33'");
  if (currentConfig.colorOrderChoice == 33) client.print(" selected");
  client.println(">RBG</option>");
  
  client.print("<option value='36'");
  if (currentConfig.colorOrderChoice == 36) client.print(" selected");
  client.println(">GBR</option>");
  
  client.print("<option value='9'");
  if (currentConfig.colorOrderChoice == 9) client.print(" selected");
  client.println(">BGR</option>");
  
  client.print("<option value='24'");
  if (currentConfig.colorOrderChoice == 24) client.print(" selected");
  client.println(">BRG</option>");
  
  // RGBW opciók
  client.print("<option value='100'");
  if (currentConfig.colorOrderChoice == 100) client.print(" selected");
  client.println(">RGBW</option>");
  
  client.print("<option value='101'");
  if (currentConfig.colorOrderChoice == 101) client.print(" selected");
  client.println(">GRBW</option>");
  
  client.print("<option value='102'");
  if (currentConfig.colorOrderChoice == 102) client.print(" selected");
  client.println(">RBGW</option>");
  
  client.print("<option value='103'");
  if (currentConfig.colorOrderChoice == 103) client.print(" selected");
  client.println(">GBRW</option>");
  
  client.print("<option value='104'");
  if (currentConfig.colorOrderChoice == 104) client.print(" selected");
  client.println(">BGRW</option>");
  
  client.print("<option value='105'");
  if (currentConfig.colorOrderChoice == 105) client.print(" selected");
  client.println(">BRGW</option>");
  
  client.print("<option value='106'");
  if (currentConfig.colorOrderChoice == 106) client.print(" selected");
  client.println(">WRGB</option>");
  
  client.println("</select>");
  client.println("</div>");
  
  // Hálózati beállítások
  client.println("<div class='section'>");
  client.println("<h3>Network Settings</h3>");
  client.print("<label>IP Address:</label>");
  for (int i = 0; i < 4; i++) {
    client.print("<input type='number' name='ip");
    client.print(i);
    client.print("' value='");
    client.print(currentConfig.ip[i]);
    client.print("' min='0' max='255'>");
    if (i < 3) client.print(".");
  }
  client.println("<br>");
  
  client.print("<label>Subnet Mask:</label>");
  for (int i = 0; i < 4; i++) {
    client.print("<input type='number' name='subnet");
    client.print(i);
    client.print("' value='");
    client.print(currentConfig.subnet[i]);
    client.print("' min='0' max='255'>");
    if (i < 3) client.print(".");
  }
  client.println("<br>");
  
  client.print("<label>MAC Address:</label>");
  for (int i = 0; i < 6; i++) {
    client.print("<input type='text' class='mac-input' name='mac");
    client.print(i);
    client.print("' value='");
    if (currentConfig.mac[i] < 16) client.print("0");
    client.print(currentConfig.mac[i], HEX);
    client.print("' maxlength='2'>");
    if (i < 5) client.print(":");
  }
  client.println("</div>");
  
  // Test mód
  client.println("<div class='section'>");
  client.println("<h3>Test Mode</h3>");
  
  // Gomb státusz dinamikusan frissül JavaScript-tel
  
  client.println("<div style='display: flex; align-items: center; gap: 10px;'>");
  client.print("<label><input type='checkbox' id='testmode' onchange='toggleTestMode()'");
  if (currentConfig.testMode) client.print(" checked");
  client.println("> Test Mode</label>");
  
  client.println("<select id='testmodeType' onchange='changeTestModeType()'>");
  client.print("<option value='0'");
  if (currentConfig.testModeType == 0) client.print(" selected");
  client.println(">Rainbow</option>");
  client.print("<option value='1'");
  if (currentConfig.testModeType == 1) client.print(" selected");
  client.println(">Red</option>");
  client.print("<option value='2'");
  if (currentConfig.testModeType == 2) client.print(" selected");
  client.println(">Green</option>");
  client.print("<option value='3'");
  if (currentConfig.testModeType == 3) client.print(" selected");
  client.println(">Blue</option>");
  client.println("</select>");
  client.println("</div>");
  
  // Üres hely a státusz üzenetnek
  client.println("<div id='test-status' style='height: 30px; margin-top: 10px;'></div>");
  client.println("</div>");
  
  // Fix pozíciójú gombok az oldal alján - kompakt méret
  client.println("<div id='fixed-buttons' style='position: fixed; bottom: 0; left: 0; right: 0; background-color: #333333; padding: 8px; text-align: center; border-top: 1px solid #555555; z-index: 1000;'>");
  client.println("<button type='submit' id='save-btn' style='background-color: #2196F3; color: white; padding: 6px 12px; border: none; border-radius: 3px; margin-right: 8px; cursor: pointer; font-size: 14px;'>Save Configuration</button>");
  client.println("<button type='button' onclick='window.location.href=\"/\"' style='background-color: #666666; color: white; padding: 6px 12px; border: none; border-radius: 3px; margin-right: 8px; cursor: pointer; font-size: 14px;'>Refresh</button>");
  client.println("<span id='save-status' style='margin-left: 8px; font-weight: bold; color: #ffffff; font-size: 14px;'></span>");
  client.println("</div>");
  
  // Kisebb padding a body alján - kompakt fix mező miatt
  client.println("<div style='height: 50px;'></div>");
  client.println("<div id='status-message' class='status-message' style='display:none;'></div>");
  client.println("</form>");
  
  // JavaScript
  client.println("<script>");
  client.println("function toggleTestMode() {");
  client.println("  var checkbox = document.getElementById('testmode');");
  client.println("  var testType = document.getElementById('testmodeType').value;");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.open('POST', '/testmode', true);");
  client.println("  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');");
  client.println("  xhr.send('testmode=' + (checkbox.checked ? '1' : '0') + '&testtype=' + testType + '&source=web');");
  client.println("  var msg = document.getElementById('test-status');");
  client.println("  if (checkbox.checked) {");
  client.println("    var modes = ['Rainbow', 'Red', 'Green', 'Blue'];");
  client.println("    msg.innerHTML = modes[testType] + ' test mode enabled (Web control)';");
  client.println("    msg.style.color = '#00aa00';");
  client.println("  } else {");
  client.println("    msg.innerHTML = '';");
  client.println("  }");
  client.println("}");
  client.println("function changeTestModeType() {");
  client.println("  var checkbox = document.getElementById('testmode');");
  client.println("  if (checkbox.checked) {");
  client.println("    toggleTestMode(); // Frissítés ha aktív");
  client.println("  }");
  client.println("}");
  
  // End Universe és End Address automatikus számítás
  client.println("function updateEndUniverse(outputIndex) {");
  client.println("  var startUni = parseInt(document.querySelector('input[name=\"startUni' + outputIndex + '\"]').value) || 0;");
  client.println("  var startAddr = parseInt(document.querySelector('input[name=\"startAddr' + outputIndex + '\"]').value) || 1;");
  client.println("  var numPixels = parseInt(document.querySelector('input[name=\"pixels' + outputIndex + '\"]').value) || 0;");
  client.println("  var ledTypeChoice = parseInt((document.querySelector('select[name=\"ledtype\"]') || {}).value) || 2;");
  client.println("  var colorOrderChoice = parseInt((document.querySelector('select[name=\"colororder\"]') || {}).value) || 18;");
  client.println("  var channelsPerPixel = (ledTypeChoice === 5 || colorOrderChoice >= 100) ? 4 : 3;");
  client.println("  var channelsPerUniverse = 512;");
  client.println("  var endUni = startUni;");
  client.println("  var endAddr = startAddr;");
  client.println("  if (numPixels > 0) {");
  client.println("    var totalChannels = numPixels * channelsPerPixel;");
  client.println("    var availableChannelsInFirstUniverse = channelsPerUniverse - (startAddr - 1);");
  client.println("    if (totalChannels <= availableChannelsInFirstUniverse) {");
  client.println("      endUni = startUni;");
  client.println("      endAddr = startAddr + totalChannels - 1;");
  client.println("    } else {");
  client.println("      var remainingChannels = totalChannels - availableChannelsInFirstUniverse;");
  client.println("      var additionalUniverses = Math.ceil(remainingChannels / channelsPerUniverse);");
  client.println("      endUni = startUni + additionalUniverses;");
  client.println("      endAddr = ((remainingChannels - 1) % channelsPerUniverse) + 1;");
  client.println("    }");
  client.println("  }");
  client.println("  document.getElementById('endUni' + outputIndex).value = endUni;");
  client.println("  document.getElementById('endAddr' + outputIndex).value = endAddr;");
  client.println("}");
  // Szerver elérhetőség ellenőrzése és automatikus frissítés
  client.println("function checkServerAndReload() {");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.timeout = 2000;");
  client.println("  xhr.onreadystatechange = function() {");
  client.println("    if (xhr.readyState === 4 && xhr.status === 200) {");
  client.println("      window.location.href='/';");
  client.println("    }");
  client.println("  };");
  client.println("  xhr.onerror = xhr.ontimeout = function() {");
  client.println("    setTimeout(checkServerAndReload, 1000);");
  client.println("  };");
  client.println("  xhr.open('GET', '/', true);");
  client.println("  xhr.send();");
  client.println("}");
  client.println("document.querySelector('form').addEventListener('submit', function(e) {");
  client.println("  e.preventDefault();");
  client.println("  var status = document.getElementById('save-status');");
  client.println("  status.innerHTML = 'Saving...';");
  client.println("  status.style.color = '#ffaa00';");
  client.println("  var formData = new FormData(this);");
  client.println("  console.log('Form data:', Array.from(formData.entries()));"); // Debug
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.onreadystatechange = function() {");
  client.println("    if (xhr.readyState === 4) {");
  client.println("      if (xhr.status === 200) {");
  client.println("        var response = JSON.parse(xhr.responseText);");
  client.println("        if (response.restart) {");
  client.println("          status.innerHTML = 'Saved! Restarting...';");
  client.println("          status.style.color = '#00aa00';");
  client.println("          setTimeout(function() {");
  client.println("            status.innerHTML = 'Waiting for server...';");
  client.println("            checkServerAndReload();");
  client.println("          }, 3000);");
  client.println("        } else {");
  client.println("          status.innerHTML = 'No changes';");
  client.println("          status.style.color = '#0066cc';");
  client.println("          setTimeout(function() { status.innerHTML = ''; }, 3000);");
  client.println("        }");
  client.println("      } else {");
  client.println("        status.innerHTML = 'Error!';");
  client.println("        status.style.color = '#cc0000';");
  client.println("      }");
  client.println("    }");
  client.println("  };");
  client.println("  xhr.open('POST', '/config', true);");
  client.println("  xhr.send(new URLSearchParams(formData));");
  client.println("});");
  
  // Automatikus státusz frissítés
  client.println("// Státusz ellenőrzés és frissítés");
  client.println("function checkButtonStatus() {");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.onreadystatechange = function() {");
  client.println("    if (xhr.readyState === 4 && xhr.status === 200) {");
  client.println("      try {");
  client.println("        var status = JSON.parse(xhr.responseText);");
  client.println("        updateButtonStatus(status);");
  client.println("      } catch(e) {");
  client.println("        console.log('Status parse error:', e);");
  client.println("      }");
  client.println("    }");
  client.println("  };");
  client.println("  xhr.open('GET', '/api/status', true);");
  client.println("  xhr.send();");
  client.println("}");
  client.println("");
  client.println("function updateButtonStatus(status) {");
  client.println("  // Frissítjük a státusz megjelenítést a test-status div-ben");
  client.println("  var statusDiv = document.getElementById('test-status');");
  client.println("  if (!statusDiv) return;");
  client.println("  ");
  client.println("  if (status.buttonTriggered || status.webTestMode) {");
  client.println("    var modeName = 'Art-Net';");
  client.println("    var isButtonControl = status.buttonTriggered;");
  client.println("    ");
  client.println("    if (isButtonControl) {");
  client.println("      // Gomb kontroll - currentTestMode használata");
  client.println("      if (status.currentTestMode >= 0 && status.currentTestMode < status.testModeNames.length) {");
  client.println("        modeName = status.testModeNames[status.currentTestMode];");
  client.println("      }");
  client.println("    } else {");
  client.println("      // Web kontroll - webTestModeType használata");
  client.println("      if (status.webTestModeType >= 0 && status.webTestModeType < status.testModeNames.length) {");
  client.println("        modeName = status.testModeNames[status.webTestModeType];");
  client.println("      }");
  client.println("    }");
  client.println("    ");
  client.println("    var textColor = isButtonControl ? '#ffffff' : '#00aa00';");
  client.println("    var controlType = isButtonControl ? 'Button control' : 'Web control';");
  client.println("    ");
  client.println("    statusDiv.innerHTML = modeName + ' test mode enabled (' + controlType + ')';");
  client.println("    statusDiv.style.color = textColor;");
  client.println("  } else {");
  client.println("    statusDiv.innerHTML = '';");
  client.println("  }");
  client.println("}");
  client.println("");
  client.println("// ENTER billentyű kezelése - megakadályozza a form submit-ot");
  client.println("document.addEventListener('DOMContentLoaded', function() {");
  client.println("  var inputs = document.querySelectorAll('input[type=\"number\"], input[type=\"text\"]');");
  client.println("  inputs.forEach(function(input) {");
  client.println("    input.addEventListener('keypress', function(e) {");
  client.println("      if (e.key === 'Enter' || e.keyCode === 13) {");
  client.println("        e.preventDefault();");
  client.println("        input.blur(); // Fókusz elhagyása");
  client.println("        console.log('ENTER prevented on input:', input.name);");
  client.println("      }");
  client.println("    });");
  client.println("  });");
  client.println("});");
  client.println("");
  client.println("// End Universe mezők inicializálása oldal betöltéskor");
  client.println("for (var i = 0; i < 8; i++) {");
  client.println("  updateEndUniverse(i);");
  client.println("}");
  client.println("");
  client.println("// LED Type / Color Order változás esetén az End Universe mezők frissítése");
  client.println("(function(){");
  client.println("  function updateAllEndFields(){ for (var i = 0; i < 8; i++) updateEndUniverse(i); }");
  client.println("  var ledSel = document.querySelector('select[name=\"ledtype\"]');");
  client.println("  var colSel = document.querySelector('select[name=\"colororder\"]');");
  client.println("  if (ledSel) ledSel.addEventListener('change', updateAllEndFields);");
  client.println("  if (colSel) colSel.addEventListener('change', updateAllEndFields);");
  client.println("})();");
  client.println("");
  client.println("// Státusz ellenőrzés indítása");
  client.println("setInterval(checkButtonStatus, 2000); // 2 másodpercenként");
  client.println("checkButtonStatus(); // Azonnali ellenőrzés");
  client.println("</script>");
  client.flush(); // JavaScript küldése után flush
  
  client.println("</div>");
  client.println("</body></html>");
  client.flush(); // Végső flush
}

// Státusz JSON küldése
void sendStatusJSON(EthernetClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  
  client.println("{");
  client.print("  \"buttonTriggered\": ");
  client.print(buttonTriggeredTestMode ? "true" : "false");
  client.println(",");
  client.print("  \"webTestMode\": ");
  client.print(currentConfig.testMode ? "true" : "false");
  client.println(",");
  client.print("  \"currentTestMode\": ");
  client.print(currentTestMode);
  client.println(",");
  client.print("  \"webTestModeType\": ");
  client.print(currentConfig.testModeType);
  client.println(",");
  client.print("  \"testModeNames\": [\"Rainbow\", \"Red\", \"Green\", \"Blue\"]");
  client.println();
  client.println("}");
}

// 404 hiba küldése
void send404(EthernetClient& client) {
  client.println("HTTP/1.1 404 Not Found");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<h1>404 - Page Not Found</h1>");
}

// Valós idejű teszt mód kezelése
void handleTestModeToggle(EthernetClient& client, String postData) {
  bool testModeEnabled = postData.indexOf("testmode=1") != -1;
  bool isWebSource = postData.indexOf("source=web") != -1;
  
  // Test mode type feldolgozása
  int testType = 0; // alapértelmezett Rainbow
  int testTypeIndex = postData.indexOf("testtype=");
  if (testTypeIndex != -1) {
    int valueStart = testTypeIndex + 9; // "testtype=" hossza
    int valueEnd = postData.indexOf("&", valueStart);
    if (valueEnd == -1) valueEnd = postData.length();
    testType = postData.substring(valueStart, valueEnd).toInt();
  }
  
  String modes[] = {"Rainbow", "Red", "Green", "Blue"};
  
  if (isWebSource) {
    // Web kontroll - felülírja a gomb kontrollt
    buttonTriggeredTestMode = false;
    if (testModeEnabled) {
      currentTestMode = testType; // 0=Rainbow, 1=Red, 2=Green, 3=Blue
      isRainbowMode = (testType == 0);
    } else {
      currentTestMode = TEST_ARTNET;
      isRainbowMode = false;
      clearAllLeds();
    }
  }
  
  // Teszt mód váltása valós időben (EEPROM mentés nélkül)
  currentConfig.testModeType = testType;
  
  // HTTP válasz
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.println("OK");
}

// Konfiguráció mentése POST kérésből
void handleConfigPost(EthernetClient& client, String postData) {
  
  bool configChanged = false;
  int totalLeds = 0;
  
  // Output konfiguráció feldolgozása (Pixels, Start Universe, Brightness)
  for (int i = 0; i < 8; i++) {
    // Pixel számok
    String pixelParam = "pixels" + String(i) + "=";
    int pixelIndex = postData.indexOf(pixelParam);
    if (pixelIndex != -1) {
      int valueStart = pixelIndex + pixelParam.length();
      int valueEnd = postData.indexOf("&", valueStart);
      if (valueEnd == -1) valueEnd = postData.length();
      
      int newValue = postData.substring(valueStart, valueEnd).toInt();
      totalLeds += newValue;
      
      if (newValue != currentConfig.ledsPerPin[i]) {
        currentConfig.ledsPerPin[i] = newValue;
        configChanged = true;
      }
    }
    
    // Start Universe
    String startUniParam = "startUni" + String(i) + "=";
    int startUniIndex = postData.indexOf(startUniParam);
    if (startUniIndex != -1) {
      int valueStart = startUniIndex + startUniParam.length();
      int valueEnd = postData.indexOf("&", valueStart);
      if (valueEnd == -1) valueEnd = postData.length();
      
      int newValue = postData.substring(valueStart, valueEnd).toInt();
      
      if (newValue != currentConfig.startUniverse[i]) {
        currentConfig.startUniverse[i] = newValue;
        configChanged = true;
      }
    }
    
    // Start Address
    String startAddrParam = "startAddr" + String(i) + "=";
    int startAddrIndex = postData.indexOf(startAddrParam);
    if (startAddrIndex != -1) {
      int valueStart = startAddrIndex + startAddrParam.length();
      int valueEnd = postData.indexOf("&", valueStart);
      if (valueEnd == -1) valueEnd = postData.length();
      
      int newValue = postData.substring(valueStart, valueEnd).toInt();
      
      if (newValue != currentConfig.startAddress[i]) {
        currentConfig.startAddress[i] = newValue;
        configChanged = true;
      }
    }
    
    // Brightness
    String brightnessParam = "brightness" + String(i) + "=";
    int brightnessIndex = postData.indexOf(brightnessParam);
    if (brightnessIndex != -1) {
      int valueStart = brightnessIndex + brightnessParam.length();
      int valueEnd = postData.indexOf("&", valueStart);
      if (valueEnd == -1) valueEnd = postData.length();
      
      int newValue = postData.substring(valueStart, valueEnd).toInt();
      
      if (newValue != currentConfig.brightness[i]) {
        currentConfig.brightness[i] = newValue;
        configChanged = true;
      }
    }
  }
  
  // LED típus és színsorrend feldolgozása
  String ledTypeParam = "ledtype=";
  String colorOrderParam = "colororder=";
  int ledTypeValue = 2; // alapértelmezett WS2812
  int colorOrderValue = 18; // alapértelmezett GRB
  
  int ledTypeIndex = postData.indexOf(ledTypeParam);
  if (ledTypeIndex != -1) {
    int valueStart = ledTypeIndex + ledTypeParam.length();
    int valueEnd = postData.indexOf("&", valueStart);
    if (valueEnd == -1) valueEnd = postData.length();
    ledTypeValue = postData.substring(valueStart, valueEnd).toInt();
  }
  
  int colorOrderIndex = postData.indexOf(colorOrderParam);
  if (colorOrderIndex != -1) {
    int valueStart = colorOrderIndex + colorOrderParam.length();
    int valueEnd = postData.indexOf("&", valueStart);
    if (valueEnd == -1) valueEnd = postData.length();
    colorOrderValue = postData.substring(valueStart, valueEnd).toInt();
  }
  
  // LED típus és színsorrend változás ellenőrzése külön-külön
  bool ledConfigChanged = false;
  
  if (ledTypeValue != currentConfig.ledTypeChoice) {
    currentConfig.ledTypeChoice = ledTypeValue;
    ledConfigChanged = true;
  }
  
  if (colorOrderValue != currentConfig.colorOrderChoice) {
    currentConfig.colorOrderChoice = colorOrderValue;
    ledConfigChanged = true;
  }
  
  if (ledConfigChanged) {
    // Csak a színsorrend érték tárolása (LED Type nem használt az OctoWS2811-ben)
    currentConfig.ledType = colorOrderValue;
    configChanged = true;
  } else {
  }
  
  // IP cím feldolgozása
  for (int i = 0; i < 4; i++) {
    String paramName = "ip" + String(i) + "=";
    int paramIndex = postData.indexOf(paramName);
    if (paramIndex != -1) {
      int valueStart = paramIndex + paramName.length();
      int valueEnd = postData.indexOf("&", valueStart);
      if (valueEnd == -1) valueEnd = postData.length();
      
      int newValue = postData.substring(valueStart, valueEnd).toInt();
      if (newValue != currentConfig.ip[i]) {
        currentConfig.ip[i] = newValue;
        configChanged = true;
      }
    }
  }
  
  // Subnet mask feldolgozása
  for (int i = 0; i < 4; i++) {
    String paramName = "subnet" + String(i) + "=";
    int paramIndex = postData.indexOf(paramName);
    if (paramIndex != -1) {
      int valueStart = paramIndex + paramName.length();
      int valueEnd = postData.indexOf("&", valueStart);
      if (valueEnd == -1) valueEnd = postData.length();
      
      int newValue = postData.substring(valueStart, valueEnd).toInt();
      if (newValue != currentConfig.subnet[i]) {
        currentConfig.subnet[i] = newValue;
        configChanged = true;
      }
    }
  }
  
  // MAC address feldolgozása
  for (int i = 0; i < 6; i++) {
    String paramName = "mac" + String(i) + "=";
    int paramIndex = postData.indexOf(paramName);
    if (paramIndex != -1) {
      int valueStart = paramIndex + paramName.length();
      int valueEnd = postData.indexOf("&", valueStart);
      if (valueEnd == -1) valueEnd = postData.length();
      
      String hexValue = postData.substring(valueStart, valueEnd);
      byte newValue = (byte)strtol(hexValue.c_str(), NULL, 16);
      if (newValue != currentConfig.mac[i]) {
        currentConfig.mac[i] = newValue;
        configChanged = true;
      }
    }
  }
  
  // Test mód feldolgozása
  bool testModeChecked = postData.indexOf("testmode=1") != -1;
  if (testModeChecked != currentConfig.testMode) {
    currentConfig.testMode = testModeChecked;
    configChanged = true;
  }
  
  // Memória limit ellenőrzés
  
  if (totalLeds > 8200) {
    // Hiba válasz
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println("{\"status\":\"error\",\"message\":\"Too many LEDs! Maximum total: 8160 (8x1020)\",\"restart\":false}");
    return;
  }
  
  if (configChanged) {
    // Konfiguráció mentése EEPROM-ba
    saveConfigToEEPROM();
    
    // Egyszerű JSON válasz
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println("{\"status\":\"saved\",\"restart\":true}");
    
    // Újraindítás ütemezése
    delay(100); // Kis várakozás a válasz elküldéséhez
    
    // Kapcsolat lezárása
    client.stop();
    
    // Várakozás és újraindítás
    delay(1000);
    SCB_AIRCR = 0x05FA0004; // Teensy software reset
    
  } else {
    // Nincs változás
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println("{\"status\":\"no_changes\",\"restart\":false}");
  }
}

// JSON konfiguráció küldése
void sendConfigJSON(EthernetClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  
  client.println("{");
  client.print("  \"ledsPerPin\": [");
  for (int i = 0; i < 8; i++) {
    client.print(currentConfig.ledsPerPin[i]);
    if (i < 7) client.print(",");
  }
  client.println("],");
  client.print("  \"ledType\": ");
  client.print(currentConfig.ledType);
  client.println(",");
  client.print("  \"ip\": [");
  for (int i = 0; i < 4; i++) {
    client.print(currentConfig.ip[i]);
    if (i < 3) client.print(",");
  }
  client.println("],");
  client.print("  \"subnet\": [");
  for (int i = 0; i < 4; i++) {
    client.print(currentConfig.subnet[i]);
    if (i < 3) client.print(",");
  }
  client.println("],");
  client.print("  \"testMode\": ");
  client.print(currentConfig.testMode ? "true" : "false");
  client.println();
  client.println("}");
}

// Konfiguráció alkalmazása
void applyConfiguration() {
  
  // Globális változók frissítése
  for (int i = 0; i < 4; i++) {
    ip[i] = currentConfig.ip[i];
    broadcast[i] = currentConfig.subnet[i];
  }
  
  for (int i = 0; i < 6; i++) {
    mac[i] = currentConfig.mac[i];
  }
  
  // Test mód alkalmazása
  isRainbowMode = currentConfig.testMode;
  
  // LED konfiguráció már frissítve lett az EEPROM betöltéskor
  
  // Új maximális LED szám kalkulálása a konfiguráció alapján
  int newMaxLedsPerStrip = 0;
  for (int i = 0; i < numPins; i++) {
    if (currentConfig.ledsPerPin[i] > newMaxLedsPerStrip) {
      newMaxLedsPerStrip = currentConfig.ledsPerPin[i];
    }
  }
  
  // OctoWS2811 újrainicializálása az új konfigurációval
  // Megjegyzés: Az OctoWS2811 nem támogatja a dinamikus újrainicializálást
  // Az újraindítás szükséges a változtatások alkalmazásához
  
  for (int i = 0; i < 4; i++) {
  }
  
  for (int i = 0; i < 6; i++) {
  }
  
  for (int i = 0; i < numPins; i++) {
  }
}

// EEPROM konfiguráció mentése
void saveConfigToEEPROM() {
  
  // Magic number írása
  EEPROM.put(EEPROM_CONFIG_ADDR, EEPROM_MAGIC);
  
  // Konfiguráció írása
  EEPROM.put(EEPROM_CONFIG_ADDR + sizeof(int), currentConfig);
  
}

// EEPROM konfiguráció betöltése
bool loadConfigFromEEPROM() {
  
  // Magic number ellenőrzése
  int magic;
  EEPROM.get(EEPROM_CONFIG_ADDR, magic);
  
  if (magic != EEPROM_MAGIC) {
    return false;
  }
  
  // Konfiguráció betöltése
  EEPROM.get(EEPROM_CONFIG_ADDR + sizeof(int), currentConfig);
  
  for (int i = 0; i < 8; i++) {
  }
  
  // LED konfiguráció betöltve - statikus objektum használata
  
  return true;
}

// Memóriahiány ellenőrzés és EEPROM reset
void checkMemoryAndResetIfNeeded() {
  
  // Gyors EEPROM ellenőrzés a memóriahiány detektálásához
  int magic;
  EEPROM.get(EEPROM_CONFIG_ADDR, magic);
  
  if (magic == EEPROM_MAGIC) {
    Config tempConfig;
    EEPROM.get(EEPROM_CONFIG_ADDR + sizeof(int), tempConfig);
    
    // Összes LED szám kalkulálása
    int totalLeds = 0;
    for (int i = 0; i < 8; i++) {
      totalLeds += tempConfig.ledsPerPin[i];
    }
    
    
    // Memória limit ellenőrzés (maximum: 8160 LED = 8×1020)
    if (totalLeds > 8200) {
      
      // EEPROM törlése
      resetEEPROMToSafeValues();
      
      delay(2000);
    } else {
    }
  }
}

// EEPROM reset biztonságos értékekre
void resetEEPROMToSafeValues() {
  
  // Biztonságos konfiguráció
  Config safeConfig;
  for (int i = 0; i < 8; i++) {
    safeConfig.ledsPerPin[i] = 1020; // 1020 pixel/kimenet
  }
  for (int i = 0; i < 4; i++) {
    safeConfig.ip[i] = ip[i];
    safeConfig.subnet[i] = broadcast[i];
  }
  for (int i = 0; i < 6; i++) {
    safeConfig.mac[i] = mac[i];
  }
  safeConfig.ledType = WS2811_GRB;     // GRB színsorrend
  safeConfig.ledTypeChoice = 2;        // WS2812/B
  safeConfig.colorOrderChoice = 18;    // GRB
  safeConfig.testMode = false;
  safeConfig.testModeType = 0;         // Rainbow
  
  // Start Universe, Start Address és Brightness alapértelmezett értékek
  for (int i = 0; i < 8; i++) {
    safeConfig.startUniverse[i] = i * 6;   // 0, 6, 12, 18, 24, 30, 36, 42
    safeConfig.startAddress[i] = 1;    // Minden kimenet 1-es címről indul
    safeConfig.brightness[i] = 255;    // Teljes fényerő
  }
  
  // Biztonságos konfiguráció mentése
  EEPROM.put(EEPROM_CONFIG_ADDR, EEPROM_MAGIC);
  EEPROM.put(EEPROM_CONFIG_ADDR + sizeof(int), safeConfig);
  
}
