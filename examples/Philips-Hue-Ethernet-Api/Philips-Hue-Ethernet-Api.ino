#include "PluggableUSB.h"
#include "HID.h"
#include <Ethernet.h>

// This sketch maps HID lights to Phillips Hue color lamps, lightstrips and lights
// You need a USB capable Arduino with an Ethernet shield. Tested on a Leonardo with Ethernet shield.
// - for games with RGB lights (SDVX etc) map each color
// - for single on/off lights, you can map either all RGB channels to the same game light for white
//   or 1 or 2 channels only for a particular color

// follow the following to get a Hub username and your light IDs: https://developers.meethue.com/develop/get-started-2/
char server[] = "192.168.1.11";    // IP address of Hue Hub
String hue_username = "hubusername"; // unique username for your registered "app"

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 1, 177);
IPAddress myDns(192, 168, 1, 1);
EthernetClient client;

#define NUMBER_OF_RGB 2 // set to number of Hue lights, these will appear in order as seperate lights in Btools

int hue_rgb_light_ids[NUMBER_OF_RGB] = {
  6, 5 // get hue light ids from the API in advance. Follow  https://developers.meethue.com/develop/get-started-2/
};

// Blue color can be really dark if you have this set to 1. 5 is a nice number. 
// return to 1 if "color accuracy" is important to you
float brightness_multiplier = 5; 

// no need to edit the rest of this file

typedef struct {
  uint8_t brightness;
} SingleLED;

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} RGBLed;

volatile bool needs_update = false;

volatile RGBLed rgb_desired_values[NUMBER_OF_RGB];

#define NUMBER_OF_SINGLE 0 // not supported in this sample, leave as 0

// not supported
int my_lights[NUMBER_OF_SINGLE] = {

};

void light_update(SingleLED* single_leds, RGBLed* rgb_leds) {
  for (int i = 0; i < NUMBER_OF_SINGLE; i++) {
    // not supported
  }
  for (int i = 0; i < NUMBER_OF_RGB; i++) {
    rgb_desired_values[i].r = rgb_leds[i].r;
    rgb_desired_values[i].g = rgb_leds[i].g;
    rgb_desired_values[i].b = rgb_leds[i].b;
  }

  needs_update = true;
}

void setup() {

  // start the Ethernet connection:
  Serial.println("Initialize Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      while (true) {
        delay(1); // do nothing, no point running without Ethernet hardware
      }
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip, myDns);
    Serial.print("My IP address: ");
    Serial.println(Ethernet.localIP());
  } else {
    Serial.print("  DHCP assigned IP ");
    Serial.println(Ethernet.localIP());
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);

}

void loop() {
  
  double normalizedToOne[3];
  float cred, cgreen, cblue;
  float red, green, blue;

  if (needs_update)
  {
 
    for (int i = 0; i < NUMBER_OF_RGB; i++) {

        // Serial.println(String(i) + ": " + String(rgb_desired_values[i].r) + ", " + String(rgb_desired_values[i].g) + ", " + String(rgb_desired_values[i].b));

        cred = rgb_desired_values[i].r;
        cgreen = rgb_desired_values[i].g;
        cblue = rgb_desired_values[i].b;
        normalizedToOne[0] = (cred / 255);
        normalizedToOne[1] = (cgreen / 255);
        normalizedToOne[2] = (cblue / 255);

        if (cred == 0 && cgreen == 0 && cblue == 0)
        {
          // black means turn the light off  
          
          String command = "{\"on\":false, \"transitiontime\" : 1}";
          // Serial.println(command);
          boolean result = setHue(hue_rgb_light_ids[i], command);

          
        } else {

        // Make red more vivid
        if (normalizedToOne[0] > 0.04045) {
          red = (float) pow(
                  (normalizedToOne[0] + 0.055) / (1.0 + 0.055), 2.4);
        } else {
          red = (float) (normalizedToOne[0] / 12.92);
        }

        // Make green more vivid
        if (normalizedToOne[1] > 0.04045) {
          green = (float) pow((normalizedToOne[1] + 0.055)
                              / (1.0 + 0.055), 2.4);
        } else {
          green = (float) (normalizedToOne[1] / 12.92);
        }

        // Make blue more vivid
        if (normalizedToOne[2] > 0.04045) {
          blue = (float) pow((normalizedToOne[2] + 0.055)
                             / (1.0 + 0.055), 2.4);
        } else {
          blue = (float) (normalizedToOne[2] / 12.92);
        }

        float X = (float) (red * 0.649926 + green * 0.103455 + blue * 0.197109);
        float Y = (float) (red * 0.234327 + green * 0.743075 + blue * 0.022598);
        float Z = (float) (red * 0.0000000 + green * 0.053077 + blue * 1.035763);

        float x = X / (X + Y + Z);
        float y = Y / (X + Y + Z);

        float brightness = min(Y * brightness_multiplier * 255, 255);
      
        String command = "{\"on\":true, \"bri\":" + String(brightness,0) + ", \"xy\" : [" + String(x, 2) + "," + String(y, 2) + "], \"transitiontime\" : 1}";
        // Serial.println(command);
        boolean result = setHue(hue_rgb_light_ids[i], command);

        }
    }

    needs_update = false;
  }
  
}


boolean setHue(int lightNum, String command)
{
  if (client.connect(server, 80))
  {

    client.print("PUT /api/");
    client.print(hue_username);
    client.print("/lights/");
    client.print(lightNum);  // hueLight zero based, add 1
    client.println("/state HTTP/1.1");
    client.println("keep-alive");
    client.print("Host: ");
    client.println(server);
    client.print("Content-Length: ");
    client.println(command.length());
    client.println("Content-Type: text/plain;charset=UTF-8");
    client.println();  // blank line before body
    client.println(command);  // Hue command

    //client.stop();
    while (client.connected())
    {
      while (client.available()) {
        char ch = client.read();
      }
    }

    //}
    client.stop();
    return true;  // command executed
  }
  else
    return false;  // command failed
}

// ******************************
// don't need to edit below here

#define NUMBER_OF_LIGHTS (NUMBER_OF_SINGLE + NUMBER_OF_RGB*3)
#if NUMBER_OF_LIGHTS > 63
#error You must have less than 64 lights
#endif

union {
  struct {
    SingleLED singles[NUMBER_OF_SINGLE];
    RGBLed rgb[NUMBER_OF_RGB];
  } leds;
  uint8_t raw[NUMBER_OF_LIGHTS];
} led_data;

static const uint8_t PROGMEM _hidReportLEDs[] = {
  0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
  0x09, 0x00,                    // USAGE (Undefined)
  0xa1, 0x01,                    // COLLECTION (Application)
  // Globals
  0x95, NUMBER_OF_LIGHTS,        //   REPORT_COUNT
  0x75, 0x08,                    //   REPORT_SIZE (8)
  0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
  0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
  0x05, 0x0a,                    //   USAGE_PAGE (Ordinals)
  // Locals
  0x19, 0x01,                    //   USAGE_MINIMUM (Instance 1)
  0x29, NUMBER_OF_LIGHTS,        //   USAGE_MAXIMUM (Instance n)
  0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)
  // BTools needs at least 1 input to work properly
  0x19, 0x01,                    //   USAGE_MINIMUM (Instance 1)
  0x29, 0x01,                    //   USAGE_MAXIMUM (Instance 1)
  0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
  0xc0                           // END_COLLECTION
};

// This is almost entirely copied from NicoHood's wonderful RawHID example
// Trimmed to the bare minimum
// https://github.com/NicoHood/HID/blob/master/src/SingleReport/RawHID.cpp
class HIDLED_ : public PluggableUSBModule {

    uint8_t epType[1];

  public:
    HIDLED_(void) : PluggableUSBModule(1, 1, epType) {
      epType[0] = EP_TYPE_INTERRUPT_IN;
      PluggableUSB().plug(this);
    }

    int getInterface(uint8_t* interfaceCount) {
      *interfaceCount += 1; // uses 1
      HIDDescriptor hidInterface = {
        D_INTERFACE(pluggedInterface, 1, USB_DEVICE_CLASS_HUMAN_INTERFACE, HID_SUBCLASS_NONE, HID_PROTOCOL_NONE),
        D_HIDREPORT(sizeof(_hidReportLEDs)),
        D_ENDPOINT(USB_ENDPOINT_IN(pluggedEndpoint), USB_ENDPOINT_TYPE_INTERRUPT, USB_EP_SIZE, 16)
      };
      return USB_SendControl(0, &hidInterface, sizeof(hidInterface));
    }

    int getDescriptor(USBSetup& setup)
    {
      // Check if this is a HID Class Descriptor request
      if (setup.bmRequestType != REQUEST_DEVICETOHOST_STANDARD_INTERFACE) {
        return 0;
      }
      if (setup.wValueH != HID_REPORT_DESCRIPTOR_TYPE) {
        return 0;
      }

      // In a HID Class Descriptor wIndex contains the interface number
      if (setup.wIndex != pluggedInterface) {
        return 0;
      }

      return USB_SendControl(TRANSFER_PGM, _hidReportLEDs, sizeof(_hidReportLEDs));
    }

    bool setup(USBSetup& setup)
    {
      if (pluggedInterface != setup.wIndex) {
        return false;
      }

      uint8_t request = setup.bRequest;
      uint8_t requestType = setup.bmRequestType;

      if (requestType == REQUEST_DEVICETOHOST_CLASS_INTERFACE)
      {
        return true;
      }

      if (requestType == REQUEST_HOSTTODEVICE_CLASS_INTERFACE) {
        if (request == HID_SET_REPORT) {
          if (setup.wValueH == HID_REPORT_TYPE_OUTPUT && setup.wLength == NUMBER_OF_LIGHTS) {
            USB_RecvControl(led_data.raw, NUMBER_OF_LIGHTS);
            light_update(led_data.leds.singles, led_data.leds.rgb);
            return true;
          }
        }
      }

      return false;
    }
};

HIDLED_ HIDLeds;
