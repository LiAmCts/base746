#include "lvgl.h"
#include <cmath>

static constexpr int CLOCK_SIZE = 76;
static constexpr int CLOCK_CENTER = CLOCK_SIZE / 2;
static constexpr float PI = 3.14159265f;

static lv_obj_t * timeLabel;
static lv_obj_t * statusLabel;
static lv_obj_t * frameLabel;
static lv_obj_t * analogClock;
static lv_obj_t * hourHand;
static lv_obj_t * minuteHand;
static lv_obj_t * secondHand;
static lv_point_precise_t hourHandPoints[2];
static lv_point_precise_t minuteHandPoints[2];
static lv_point_precise_t secondHandPoints[2];

static void placerAiguille(lv_obj_t * aiguille, lv_point_precise_t * points,
                           float angleDegres, int longueur)
{
  float angleRadians = (angleDegres - 90.0f) * PI / 180.0f;

  points[0].x = CLOCK_CENTER;
  points[0].y = CLOCK_CENTER;
  points[1].x = CLOCK_CENTER + static_cast<int>(std::cos(angleRadians) * longueur);
  points[1].y = CLOCK_CENTER + static_cast<int>(std::sin(angleRadians) * longueur);

  lv_line_set_points(aiguille, points, 2);
}

static void mettreAJourHorlogeAnalogique(int heure, int minute, int seconde)
{
  if (hourHand == nullptr || minuteHand == nullptr || secondHand == nullptr) {
    return;
  }

  float angleHeure = ((heure % 12) * 30.0f) + (minute * 0.5f);
  float angleMinute = (minute * 6.0f) + (seconde * 0.1f);
  float angleSeconde = seconde * 6.0f;

  placerAiguille(hourHand, hourHandPoints, angleHeure, 20);
  placerAiguille(minuteHand, minuteHandPoints, angleMinute, 28);
  placerAiguille(secondHand, secondHandPoints, angleSeconde, 31);
}

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

  analogClock = lv_obj_create(screen);
  lv_obj_remove_flag(analogClock, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(analogClock, CLOCK_SIZE, CLOCK_SIZE);
  lv_obj_set_style_radius(analogClock, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(analogClock, lv_color_white(), 0);
  lv_obj_set_style_border_color(analogClock, lv_color_black(), 0);
  lv_obj_set_style_border_width(analogClock, 2, 0);
  lv_obj_set_style_pad_all(analogClock, 0, 0);
  lv_obj_align(analogClock, LV_ALIGN_RIGHT_MID, -22, -45);

  hourHand = lv_line_create(analogClock);
  minuteHand = lv_line_create(analogClock);
  secondHand = lv_line_create(analogClock);
  lv_obj_set_size(hourHand, CLOCK_SIZE, CLOCK_SIZE);
  lv_obj_set_size(minuteHand, CLOCK_SIZE, CLOCK_SIZE);
  lv_obj_set_size(secondHand, CLOCK_SIZE, CLOCK_SIZE);
  lv_obj_set_style_line_width(hourHand, 5, 0);
  lv_obj_set_style_line_width(minuteHand, 3, 0);
  lv_obj_set_style_line_width(secondHand, 1, 0);
  lv_obj_set_style_line_color(hourHand, lv_color_black(), 0);
  lv_obj_set_style_line_color(minuteHand, lv_color_black(), 0);
  lv_obj_set_style_line_color(secondHand, lv_color_hex(0xCC2020), 0);
  mettreAJourHorlogeAnalogique(0, 0, 0);

  lv_obj_t * centerDot = lv_obj_create(analogClock);
  lv_obj_remove_style_all(centerDot);
  lv_obj_set_size(centerDot, 8, 8);
  lv_obj_set_style_radius(centerDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(centerDot, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(centerDot, LV_OPA_COVER, 0);
  lv_obj_center(centerDot);

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
    mettreAJourHorlogeAnalogique(heure, minute, seconde);
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
