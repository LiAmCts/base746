#include "lvgl.h"
#include <cmath>
#include <cstdint>
#include <cstdio>

static constexpr int CLOCK_SIZE = 88;
static constexpr int CLOCK_CENTER = CLOCK_SIZE / 2;
static constexpr float PI = 3.14159265f;
static constexpr int MAX_ALARMS = 4;

struct AlarmEntry {
  int hour;
  int minute;
  bool active;
};

static lv_obj_t * timeLabel;
static lv_obj_t * statusLabel;
static lv_obj_t * analogClock;
static lv_obj_t * hourHand;
static lv_obj_t * minuteHand;
static lv_obj_t * secondHand;
static lv_point_precise_t hourHandPoints[2];
static lv_point_precise_t minuteHandPoints[2];
static lv_point_precise_t secondHandPoints[2];
static lv_obj_t * alarmButtonLabel;
static lv_obj_t * alarmSummaryLabel;
static lv_obj_t * alarmPanel;
static lv_obj_t * alarmTimeLabel;
static lv_obj_t * alarmPanelStatusLabel;
static lv_obj_t * alarmRowLabel[MAX_ALARMS];
static lv_obj_t * alarmDeleteButton[MAX_ALARMS];
static AlarmEntry alarms[MAX_ALARMS];
static int selectedAlarmHour = 7;
static int selectedAlarmMinute = 0;

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

  placerAiguille(hourHand, hourHandPoints, angleHeure, 22);
  placerAiguille(minuteHand, minuteHandPoints, angleMinute, 32);
  placerAiguille(secondHand, secondHandPoints, angleSeconde, 36);
}

static lv_obj_t * creerBoutonTexte(lv_obj_t * parent, const char * texte, int x, int y,
                                   int largeur, int hauteur, lv_event_cb_t callback,
                                   void * userData = nullptr)
{
  lv_obj_t * bouton = lv_button_create(parent);
  lv_obj_set_pos(bouton, x, y);
  lv_obj_set_size(bouton, largeur, hauteur);
  lv_obj_set_style_radius(bouton, 8, 0);
  lv_obj_set_style_bg_color(bouton, lv_color_hex(0x2B2D31), 0);
  lv_obj_set_style_shadow_width(bouton, 0, 0);

  lv_obj_t * label = lv_label_create(bouton);
  lv_label_set_text(label, texte);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_center(label);

  if (callback != nullptr) {
    lv_obj_add_event_cb(bouton, callback, LV_EVENT_CLICKED, userData);
  }

  return bouton;
}

static void mettreAJourInterfaceAlarmes()
{
  int count = 0;
  char buttonText[16];
  char summaryText[96] = "Aucun reveil";

  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarms[i].active) {
      count++;
    }
  }

  std::snprintf(buttonText, sizeof(buttonText), "%s %d", LV_SYMBOL_BELL, count);
  lv_label_set_text(alarmButtonLabel, buttonText);

  if (count > 0) {
    int offset = std::snprintf(summaryText, sizeof(summaryText), "Reveil");
    if (count > 1) {
      offset = std::snprintf(summaryText, sizeof(summaryText), "Reveils");
    }

    for (int i = 0; i < MAX_ALARMS && offset < static_cast<int>(sizeof(summaryText)); i++) {
      if (alarms[i].active) {
        offset += std::snprintf(summaryText + offset, sizeof(summaryText) - offset,
                                " %02d:%02d", alarms[i].hour, alarms[i].minute);
      }
    }
  }

  lv_label_set_text(alarmSummaryLabel, summaryText);

  char selectedText[8];
  std::snprintf(selectedText, sizeof(selectedText), "%02d:%02d", selectedAlarmHour, selectedAlarmMinute);
  lv_label_set_text(alarmTimeLabel, selectedText);

  char statusText[32];
  std::snprintf(statusText, sizeof(statusText), "%d/%d reveils", count, MAX_ALARMS);
  lv_label_set_text(alarmPanelStatusLabel, statusText);

  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarms[i].active) {
      char alarmText[16];
      std::snprintf(alarmText, sizeof(alarmText), "%02d:%02d", alarms[i].hour, alarms[i].minute);
      lv_label_set_text(alarmRowLabel[i], alarmText);
      lv_obj_remove_flag(alarmRowLabel[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(alarmDeleteButton[i], LV_OBJ_FLAG_HIDDEN);
    }
    else {
      lv_obj_add_flag(alarmRowLabel[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(alarmDeleteButton[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void ouvrirPanneauAlarmes(lv_event_t * e)
{
  (void)e;
  lv_obj_remove_flag(alarmPanel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(alarmPanel);
}

static void fermerPanneauAlarmes(lv_event_t * e)
{
  (void)e;
  lv_obj_add_flag(alarmPanel, LV_OBJ_FLAG_HIDDEN);
}

static void changerHeureAlarme(lv_event_t * e)
{
  int delta = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  selectedAlarmHour = (selectedAlarmHour + delta + 24) % 24;
  mettreAJourInterfaceAlarmes();
}

static void changerMinuteAlarme(lv_event_t * e)
{
  int delta = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  selectedAlarmMinute = (selectedAlarmMinute + delta + 60) % 60;
  mettreAJourInterfaceAlarmes();
}

static void ajouterAlarme(lv_event_t * e)
{
  (void)e;

  for (int i = 0; i < MAX_ALARMS; i++) {
    if (!alarms[i].active) {
      alarms[i].hour = selectedAlarmHour;
      alarms[i].minute = selectedAlarmMinute;
      alarms[i].active = true;
      mettreAJourInterfaceAlarmes();
      return;
    }
  }

  lv_label_set_text(alarmPanelStatusLabel, "Liste pleine");
}

static void supprimerAlarme(lv_event_t * e)
{
  int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  if (index >= 0 && index < MAX_ALARMS) {
    alarms[index].active = false;
    mettreAJourInterfaceAlarmes();
  }
}

static void creerInterfaceAlarmes(lv_obj_t * screen)
{
  lv_obj_t * alarmButton = lv_button_create(screen);
  lv_obj_set_pos(alarmButton, 18, 18);
  lv_obj_set_size(alarmButton, 68, 32);
  lv_obj_set_style_radius(alarmButton, 8, 0);
  lv_obj_set_style_bg_color(alarmButton, lv_color_hex(0xF5F7FA), 0);
  lv_obj_set_style_shadow_width(alarmButton, 0, 0);
  lv_obj_add_event_cb(alarmButton, ouvrirPanneauAlarmes, LV_EVENT_CLICKED, nullptr);

  alarmButtonLabel = lv_label_create(alarmButton);
  lv_obj_set_style_text_color(alarmButtonLabel, lv_color_hex(0x111111), 0);
  lv_obj_center(alarmButtonLabel);

  alarmSummaryLabel = lv_label_create(screen);
  lv_obj_set_width(alarmSummaryLabel, 210);
  lv_label_set_long_mode(alarmSummaryLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(alarmSummaryLabel, lv_color_hex(0xB8C0CC), 0);
  lv_obj_align(alarmSummaryLabel, LV_ALIGN_TOP_LEFT, 96, 26);

  alarmPanel = lv_obj_create(screen);
  lv_obj_remove_flag(alarmPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(alarmPanel, 318, 210);
  lv_obj_set_style_radius(alarmPanel, 8, 0);
  lv_obj_set_style_bg_color(alarmPanel, lv_color_hex(0x16191F), 0);
  lv_obj_set_style_bg_opa(alarmPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(alarmPanel, lv_color_hex(0x303540), 0);
  lv_obj_set_style_border_width(alarmPanel, 1, 0);
  lv_obj_set_style_shadow_width(alarmPanel, 14, 0);
  lv_obj_set_style_shadow_opa(alarmPanel, LV_OPA_30, 0);
  lv_obj_set_style_pad_all(alarmPanel, 0, 0);
  lv_obj_set_pos(alarmPanel, 18, 58);

  lv_obj_t * title = lv_label_create(alarmPanel);
  lv_label_set_text(title, "Reveils");
  lv_obj_set_style_text_color(title, lv_color_hex(0xF5F7FA), 0);
  lv_obj_set_pos(title, 18, 14);

  creerBoutonTexte(alarmPanel, LV_SYMBOL_CLOSE, 274, 10, 28, 28, fermerPanneauAlarmes);

  creerBoutonTexte(alarmPanel, LV_SYMBOL_PLUS, 48, 52, 36, 28, changerHeureAlarme,
                   reinterpret_cast<void *>(static_cast<intptr_t>(1)));
  creerBoutonTexte(alarmPanel, LV_SYMBOL_MINUS, 48, 96, 36, 28, changerHeureAlarme,
                   reinterpret_cast<void *>(static_cast<intptr_t>(-1)));
  creerBoutonTexte(alarmPanel, LV_SYMBOL_PLUS, 234, 52, 36, 28, changerMinuteAlarme,
                   reinterpret_cast<void *>(static_cast<intptr_t>(1)));
  creerBoutonTexte(alarmPanel, LV_SYMBOL_MINUS, 234, 96, 36, 28, changerMinuteAlarme,
                   reinterpret_cast<void *>(static_cast<intptr_t>(-1)));

  alarmTimeLabel = lv_label_create(alarmPanel);
  lv_obj_set_style_text_color(alarmTimeLabel, lv_color_hex(0xFFFFFF), 0);
#if LV_FONT_MONTSERRAT_48
  lv_obj_set_style_text_font(alarmTimeLabel, &lv_font_montserrat_48, 0);
#endif
  lv_obj_align(alarmTimeLabel, LV_ALIGN_TOP_MID, 0, 50);

  lv_obj_t * addButton = creerBoutonTexte(alarmPanel, "Ajouter", 92, 122, 134, 32, ajouterAlarme);
  lv_obj_set_style_bg_color(addButton, lv_color_hex(0xF5F7FA), 0);
  lv_obj_set_style_text_color(lv_obj_get_child(addButton, 0), lv_color_hex(0x111111), 0);

  alarmPanelStatusLabel = lv_label_create(alarmPanel);
  lv_obj_set_width(alarmPanelStatusLabel, 180);
  lv_obj_set_style_text_color(alarmPanelStatusLabel, lv_color_hex(0x9EA7B3), 0);
  lv_obj_set_style_text_align(alarmPanelStatusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(alarmPanelStatusLabel, LV_ALIGN_TOP_MID, 0, 160);

  for (int i = 0; i < MAX_ALARMS; i++) {
    int x = 18 + (i * 74);
    int y = 184;

    alarmDeleteButton[i] = creerBoutonTexte(alarmPanel, LV_SYMBOL_CLOSE, x, y - 5, 66, 24,
                                            supprimerAlarme,
                                            reinterpret_cast<void *>(static_cast<intptr_t>(i)));
    lv_obj_set_style_bg_color(alarmDeleteButton[i], lv_color_hex(0x252A33), 0);
    lv_obj_set_style_border_color(alarmDeleteButton[i], lv_color_hex(0x3A404D), 0);
    lv_obj_set_style_border_width(alarmDeleteButton[i], 1, 0);
    lv_obj_t * deleteLabel = lv_obj_get_child(alarmDeleteButton[i], 0);
    lv_obj_set_style_text_color(deleteLabel, lv_color_hex(0xF5F7FA), 0);

    alarmRowLabel[i] = lv_label_create(alarmPanel);
    lv_obj_set_style_text_color(alarmRowLabel[i], lv_color_hex(0xF5F7FA), 0);
    lv_obj_set_pos(alarmRowLabel[i], x + 8, y);
    lv_obj_move_foreground(alarmRowLabel[i]);
  }

  lv_obj_add_flag(alarmPanel, LV_OBJ_FLAG_HIDDEN);
  mettreAJourInterfaceAlarmes();
}

void testLvgl()
{
  lv_obj_t * screen = lv_screen_active();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x07111D), 0);
  lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x111827), 0);
  lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);

  timeLabel = lv_label_create(screen);
  lv_label_set_text(timeLabel, "--:--:--");
  lv_obj_set_style_text_color(timeLabel, lv_color_hex(0xFFFFFF), 0);
#if LV_FONT_MONTSERRAT_48
  lv_obj_set_style_text_font(timeLabel, &lv_font_montserrat_48, 0);
#endif
  lv_obj_align(timeLabel, LV_ALIGN_CENTER, -34, -22);

  analogClock = lv_obj_create(screen);
  lv_obj_remove_flag(analogClock, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(analogClock, CLOCK_SIZE, CLOCK_SIZE);
  lv_obj_set_style_radius(analogClock, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(analogClock, lv_color_hex(0xF6F8FB), 0);
  lv_obj_set_style_border_color(analogClock, lv_color_hex(0xDDE5F0), 0);
  lv_obj_set_style_border_width(analogClock, 3, 0);
  lv_obj_set_style_shadow_width(analogClock, 14, 0);
  lv_obj_set_style_shadow_opa(analogClock, LV_OPA_30, 0);
  lv_obj_set_style_shadow_color(analogClock, lv_color_hex(0x000000), 0);
  lv_obj_set_style_pad_all(analogClock, 0, 0);
  lv_obj_align(analogClock, LV_ALIGN_CENTER, 152, -22);

  for (int i = 0; i < 12; i++) {
    float angleRadians = ((i * 30.0f) - 90.0f) * PI / 180.0f;
    int tickSize = (i % 3 == 0) ? 7 : 4;
    int tickRadius = CLOCK_CENTER - 10;
    lv_obj_t * tick = lv_obj_create(analogClock);
    lv_obj_remove_style_all(tick);
    lv_obj_set_size(tick, tickSize, tickSize);
    lv_obj_set_style_radius(tick, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(tick, (i % 3 == 0) ? lv_color_black() : lv_color_hex(0x9A9A9A), 0);
    lv_obj_set_style_bg_opa(tick, LV_OPA_COVER, 0);
    lv_obj_set_pos(tick,
                   CLOCK_CENTER + static_cast<int>(std::cos(angleRadians) * tickRadius) - (tickSize / 2),
                   CLOCK_CENTER + static_cast<int>(std::sin(angleRadians) * tickRadius) - (tickSize / 2));
  }

  hourHand = lv_line_create(analogClock);
  minuteHand = lv_line_create(analogClock);
  secondHand = lv_line_create(analogClock);
  lv_obj_set_size(hourHand, CLOCK_SIZE, CLOCK_SIZE);
  lv_obj_set_size(minuteHand, CLOCK_SIZE, CLOCK_SIZE);
  lv_obj_set_size(secondHand, CLOCK_SIZE, CLOCK_SIZE);
  lv_obj_set_style_line_width(hourHand, 5, 0);
  lv_obj_set_style_line_width(minuteHand, 3, 0);
  lv_obj_set_style_line_width(secondHand, 1, 0);
  lv_obj_set_style_line_rounded(hourHand, true, 0);
  lv_obj_set_style_line_rounded(minuteHand, true, 0);
  lv_obj_set_style_line_rounded(secondHand, true, 0);
  lv_obj_set_style_line_color(hourHand, lv_color_black(), 0);
  lv_obj_set_style_line_color(minuteHand, lv_color_black(), 0);
  lv_obj_set_style_line_color(secondHand, lv_color_hex(0xCC2020), 0);
  mettreAJourHorlogeAnalogique(0, 0, 0);

  lv_obj_t * centerDot = lv_obj_create(analogClock);
  lv_obj_remove_style_all(centerDot);
  lv_obj_set_size(centerDot, 10, 10);
  lv_obj_set_style_radius(centerDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(centerDot, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(centerDot, LV_OPA_COVER, 0);
  lv_obj_center(centerDot);

  statusLabel = lv_label_create(screen);
  lv_label_set_text(statusLabel, "Attente du signal GPS...");
  lv_obj_set_width(statusLabel, 440);
  lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x9FB4D0), 0);
  lv_obj_set_style_text_align(statusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(statusLabel, LV_ALIGN_CENTER, 0, 44);

  creerInterfaceAlarmes(screen);
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
