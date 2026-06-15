#include "lvgl.h"

static lv_obj_t * timeLabel;
static lv_obj_t * statusLabel;
static lv_obj_t * frameLabel;

void testLvgl()
{
  lv_obj_t * screen = lv_screen_active();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, lv_color_white(), 0);

  timeLabel = lv_label_create(screen);
  lv_label_set_text(timeLabel, "--:--:--");
  lv_obj_set_style_text_color(timeLabel, lv_color_black(), 0);
#if LV_FONT_MONTSERRAT_48
  lv_obj_set_style_text_font(timeLabel, &lv_font_montserrat_48, 0);
#endif
  lv_obj_align(timeLabel, LV_ALIGN_CENTER, 0, -45);

  statusLabel = lv_label_create(screen);
  lv_label_set_text(statusLabel, "Attente des trames GPS...");
  lv_obj_set_width(statusLabel, 440);
  lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x303030), 0);
  lv_obj_set_style_text_align(statusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(statusLabel, LV_ALIGN_CENTER, 0, 20);

  frameLabel = lv_label_create(screen);
  lv_label_set_text(frameLabel, "-");
  lv_label_set_long_mode(frameLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_width(frameLabel, 440);
  lv_obj_set_style_text_color(frameLabel, lv_color_black(), 0);
  lv_obj_set_style_text_align(frameLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(frameLabel, LV_ALIGN_BOTTOM_MID, 0, -28);
}

#ifdef ARDUINO

#include "lvglDrivers.h"
#include <cstring>
#include <cstdio>

static constexpr uint32_t GPS_BAUDRATE = 9600;
static constexpr int GPS_LOCAL_UTC_OFFSET_HOURS = 2;

// UART7 de la DISCO-F746NG : PF6 = RX7, PF7 = TX7.
static HardwareSerial gpsSerial(PF6, PF7);
static char gpsLine[128];
static size_t gpsLineLen = 0;

static bool traiterTrameGps(const char * trame)
{
  if (std::strncmp(trame, "$GPRMC,", 7) != 0 && std::strncmp(trame, "$GNRMC,", 7) != 0) {
    return false;
  }

  const char * heureUtc = trame + 7;
  for (int i = 0; i < 6; i++) {
    if (heureUtc[i] < '0' || heureUtc[i] > '9') {
      return false;
    }
  }

  int heure = (heureUtc[0] - '0') * 10 + (heureUtc[1] - '0');
  int minute = (heureUtc[2] - '0') * 10 + (heureUtc[3] - '0');
  int seconde = (heureUtc[4] - '0') * 10 + (heureUtc[5] - '0');

  heure += GPS_LOCAL_UTC_OFFSET_HOURS;
  if (heure >= 24) {
    heure -= 24;
  }

  const char * virguleApresHeure = std::strchr(heureUtc, ',');
  char statutGps = (virguleApresHeure != nullptr) ? virguleApresHeure[1] : 'V';

  char texteHeure[16];
  std::snprintf(texteHeure, sizeof(texteHeure), "%02d:%02d:%02d", heure, minute, seconde);

  if (lvglLock(pdMS_TO_TICKS(20))) {
    lv_label_set_text(timeLabel, texteHeure);
    lv_label_set_text(statusLabel, (statutGps == 'A') ? "Fix GPS valide" : "Fix GPS non valide");
    lvglUnlock();
  }

  return true;
}

void mySetup()
{
  // Initialisations générales
  testLvgl();

  // Initialisation de l'UART relié au module GPS.
  gpsSerial.begin(GPS_BAUDRATE);
}

void loop()
{
  // Inactif : FreeRTOS exécute les tâches.
}

void myTask(void *pvParameters)
{
  (void)pvParameters;

  // Initialisation de la temporisation de la tâche.
  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();

  while (1)
  {
    // Lecture des caractères reçus depuis le GPS.
    while (gpsSerial.available() > 0) {
      char c = static_cast<char>(gpsSerial.read());

      if (c == '\r') {
        continue;
      }

      // Une trame NMEA complète se termine par un retour ligne.
      if (c == '\n') {
        gpsLine[gpsLineLen] = '\0';
        Serial.println(gpsLine);

        if (lvglLock(pdMS_TO_TICKS(20))) {
          lv_label_set_text(frameLabel, gpsLine);
          lvglUnlock();
        }

        traiterTrameGps(gpsLine);
        gpsLineLen = 0;
      }
      else if (gpsLineLen < sizeof(gpsLine) - 1) {
        gpsLine[gpsLineLen++] = c;
      }
      else {
        gpsLineLen = 0;
      }
    }

    // Endort la tâche pour obtenir une exécution toutes les 200 ms.
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
  }
}

#else

#include "app_hal.h"
#include <cstdio>

int main(void)
{
  printf("LVGL Simulator\n");
  fflush(stdout);

  lv_init();
  hal_setup();

  testLvgl();

  hal_loop();
  return 0;
}

#endif
