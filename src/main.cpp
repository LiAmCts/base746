#include "lvgl.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

static constexpr int SCREEN_WIDTH = 480;
static constexpr int SCREEN_HEIGHT = 272;
static constexpr int CLOCK_SIZE = 86;
static constexpr int CLOCK_CENTER = CLOCK_SIZE / 2;
static constexpr int MAX_ALARMS = 4;
static constexpr float PI = 3.14159265f;
static constexpr uint32_t SECONDS_PER_DAY = 24UL * 60UL * 60UL;

struct AlarmEntry {
  int hour;
  int minute;
  bool active;
};

static lv_obj_t * mainPage = nullptr;
static lv_obj_t * alarmPage = nullptr;
static lv_obj_t * alarmOverlay = nullptr;
static lv_obj_t * dayLayer = nullptr;
static lv_obj_t * nightLayer = nullptr;
static lv_obj_t * sunReflection = nullptr;
static lv_obj_t * timeLabel = nullptr;
static lv_obj_t * statusLabel = nullptr;
static lv_obj_t * alarmBadgeLabel = nullptr;
static lv_obj_t * alarmSummaryLabel = nullptr;
static lv_obj_t * alarmPreviewLabel = nullptr;
static lv_obj_t * alarmCountLabel = nullptr;
static lv_obj_t * hourRoller = nullptr;
static lv_obj_t * minuteRoller = nullptr;
static lv_obj_t * alarmChip[MAX_ALARMS];
static lv_obj_t * ringingTimeLabel = nullptr;
static lv_obj_t * analogClock = nullptr;
static lv_obj_t * hourHand = nullptr;
static lv_obj_t * minuteHand = nullptr;
static lv_obj_t * secondHand = nullptr;

static lv_point_precise_t hourHandPoints[2];
static lv_point_precise_t minuteHandPoints[2];
static lv_point_precise_t secondHandPoints[2];
static lv_point_precise_t alarmIconHourPoints[2] = {{12, 12}, {12, 5}};
static lv_point_precise_t alarmIconMinutePoints[2] = {{12, 12}, {17, 12}};

static AlarmEntry alarms[MAX_ALARMS];
static char hourOptions[80];
static char minuteOptions[192];
static int selectedAlarmHour = 7;
static int selectedAlarmMinute = 0;
static bool alarmPageOpen = false;
static bool alarmRinging = false;
static int ringingHour = 0;
static int ringingMinute = 0;
static uint32_t currentSecondOfDay = 0;
static bool clockReady = false;
static bool dayModeActive = true;

static bool alarmeExiste(int hour, int minute)
{
  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarms[i].active && alarms[i].hour == hour && alarms[i].minute == minute) {
      return true;
    }
  }
  return false;
}

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

  placerAiguille(hourHand, hourHandPoints, ((heure % 12) * 30.0f) + (minute * 0.5f), 22);
  placerAiguille(minuteHand, minuteHandPoints, (minute * 6.0f) + (seconde * 0.1f), 32);
  placerAiguille(secondHand, secondHandPoints, seconde * 6.0f, 36);
}

static lv_obj_t * creerBouton(lv_obj_t * parent, const char * texte, int x, int y,
                              int largeur, int hauteur, lv_event_cb_t callback,
                              void * userData = nullptr)
{
  lv_obj_t * bouton = lv_button_create(parent);
  lv_obj_set_pos(bouton, x, y);
  lv_obj_set_size(bouton, largeur, hauteur);
  lv_obj_set_style_radius(bouton, 8, 0);
  lv_obj_set_style_bg_color(bouton, lv_color_hex(0xEAFBFF), 0);
  lv_obj_set_style_border_color(bouton, lv_color_hex(0x6DE7FF), 0);
  lv_obj_set_style_border_width(bouton, 1, 0);
  lv_obj_set_style_shadow_width(bouton, 0, 0);

  lv_obj_t * label = lv_label_create(bouton);
  lv_label_set_text(label, texte);
  lv_obj_set_style_text_color(label, lv_color_hex(0x06151F), 0);
  lv_obj_center(label);

  if (callback != nullptr) {
    lv_obj_add_event_cb(bouton, callback, LV_EVENT_CLICKED, userData);
  }

  return bouton;
}

static void remplirOptionsNombre(char * buffer, size_t taille, int count)
{
  size_t offset = 0;

  for (int i = 0; i < count && offset < taille; i++) {
    int written = std::snprintf(buffer + offset, taille - offset,
                                (i == count - 1) ? "%02d" : "%02d\n", i);
    if (written <= 0) {
      return;
    }
    offset += static_cast<size_t>(written);
  }
}

static void animerY(lv_obj_t * objet, int32_t depart, int32_t arrivee,
                    uint32_t delai = 0, lv_anim_completed_cb_t fin = nullptr)
{
  lv_anim_delete(objet, reinterpret_cast<lv_anim_exec_xcb_t>(lv_obj_set_y));

  lv_anim_t animation;
  lv_anim_init(&animation);
  lv_anim_set_var(&animation, objet);
  lv_anim_set_exec_cb(&animation, reinterpret_cast<lv_anim_exec_xcb_t>(lv_obj_set_y));
  lv_anim_set_values(&animation, depart, arrivee);
  lv_anim_set_duration(&animation, 420);
  lv_anim_set_delay(&animation, delai);
  lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
  if (fin != nullptr) {
    lv_anim_set_completed_cb(&animation, fin);
  }
  lv_anim_start(&animation);
}

static void animerXInfini(lv_obj_t * objet, int32_t depart, int32_t arrivee,
                          uint32_t duree, uint32_t delai)
{
  lv_anim_t animation;
  lv_anim_init(&animation);
  lv_anim_set_var(&animation, objet);
  lv_anim_set_exec_cb(&animation, reinterpret_cast<lv_anim_exec_xcb_t>(lv_obj_set_x));
  lv_anim_set_values(&animation, depart, arrivee);
  lv_anim_set_duration(&animation, duree);
  lv_anim_set_delay(&animation, delai);
  lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&animation, lv_anim_path_linear);
  lv_anim_start(&animation);
}

static lv_obj_t * creerPastille(lv_obj_t * parent, int x, int y, int taille,
                                uint32_t couleur, lv_opa_t opa = LV_OPA_COVER)
{
  lv_obj_t * objet = lv_obj_create(parent);
  lv_obj_remove_style_all(objet);
  lv_obj_set_size(objet, taille, taille);
  lv_obj_set_pos(objet, x, y);
  lv_obj_set_style_radius(objet, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(objet, lv_color_hex(couleur), 0);
  lv_obj_set_style_bg_opa(objet, opa, 0);
  return objet;
}

static lv_obj_t * creerNuage(lv_obj_t * parent, int x, int y)
{
  lv_obj_t * cloud = lv_obj_create(parent);
  lv_obj_remove_style_all(cloud);
  lv_obj_set_size(cloud, 92, 36);
  lv_obj_set_pos(cloud, x, y);

  lv_obj_t * base = lv_obj_create(cloud);
  lv_obj_remove_style_all(base);
  lv_obj_set_size(base, 76, 18);
  lv_obj_set_pos(base, 8, 15);
  lv_obj_set_style_radius(base, 9, 0);
  lv_obj_set_style_bg_color(base, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(base, LV_OPA_90, 0);

  creerPastille(cloud, 16, 7, 24, 0xFFFFFF, LV_OPA_90);
  creerPastille(cloud, 34, 0, 32, 0xFFFFFF, LV_OPA_90);
  creerPastille(cloud, 58, 9, 22, 0xFFFFFF, LV_OPA_90);

  return cloud;
}

static lv_obj_t * creerPoisson(lv_obj_t * parent, int x, int y, uint32_t couleur)
{
  lv_obj_t * fish = lv_obj_create(parent);
  lv_obj_remove_style_all(fish);
  lv_obj_set_size(fish, 42, 20);
  lv_obj_set_pos(fish, x, y);

  lv_obj_t * body = lv_obj_create(fish);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, 26, 14);
  lv_obj_set_pos(body, 4, 3);
  lv_obj_set_style_radius(body, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(body, lv_color_hex(couleur), 0);
  lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);

  lv_obj_t * tail = lv_obj_create(fish);
  lv_obj_remove_style_all(tail);
  lv_obj_set_size(tail, 12, 12);
  lv_obj_set_pos(tail, 27, 4);
  lv_obj_set_style_radius(tail, 3, 0);
  lv_obj_set_style_bg_color(tail, lv_color_hex(couleur), 0);
  lv_obj_set_style_bg_opa(tail, LV_OPA_COVER, 0);
  lv_obj_set_style_transform_rotation(tail, 450, 0);

  creerPastille(fish, 10, 7, 3, 0x06151F, LV_OPA_COVER);

  return fish;
}

static void configurerCiel(bool dayMode)
{
  dayModeActive = dayMode;

  if (dayLayer == nullptr || nightLayer == nullptr || mainPage == nullptr) {
    return;
  }

  if (dayMode) {
    lv_obj_clear_flag(dayLayer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(nightLayer, LV_OBJ_FLAG_HIDDEN);
    if (sunReflection != nullptr) {
      lv_obj_clear_flag(sunReflection, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x5CB8F6), 0);
    lv_obj_set_style_bg_grad_color(mainPage, lv_color_hex(0xF4C26A), 0);
  }
  else {
    lv_obj_add_flag(dayLayer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(nightLayer, LV_OBJ_FLAG_HIDDEN);
    if (sunReflection != nullptr) {
      lv_obj_add_flag(sunReflection, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x061428), 0);
    lv_obj_set_style_bg_grad_color(mainPage, lv_color_hex(0x122A4A), 0);
  }
}

static void creerDecorPrincipal(lv_obj_t * parent)
{
  lv_obj_set_style_bg_color(parent, lv_color_hex(0x5CB8F6), 0);
  lv_obj_set_style_bg_grad_color(parent, lv_color_hex(0xF4C26A), 0);
  lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);

  dayLayer = lv_obj_create(parent);
  lv_obj_remove_style_all(dayLayer);
  lv_obj_set_size(dayLayer, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(dayLayer, 0, 0);

  nightLayer = lv_obj_create(parent);
  lv_obj_remove_style_all(nightLayer);
  lv_obj_set_size(nightLayer, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(nightLayer, 0, 0);

  creerPastille(dayLayer, 440, 18, 34, 0xFFD66B, LV_OPA_COVER);

  lv_obj_t * cloud1 = creerNuage(dayLayer, 24, 46);
  lv_obj_t * cloud2 = creerNuage(dayLayer, 245, 28);
  animerXInfini(cloud1, -96, SCREEN_WIDTH + 24, 24000, 0);
  animerXInfini(cloud2, -120, SCREEN_WIDTH + 36, 31000, 8000);

  creerPastille(nightLayer, 76, 18, 44, 0xE6EEF7, LV_OPA_COVER);
  creerPastille(nightLayer, 92, 12, 36, 0x061428, LV_OPA_COVER);
  creerPastille(nightLayer, 90, 24, 3, 0xFFFFFF, LV_OPA_80);
  creerPastille(nightLayer, 138, 42, 2, 0xFFFFFF, LV_OPA_70);
  creerPastille(nightLayer, 214, 25, 2, 0xFFFFFF, LV_OPA_70);
  creerPastille(nightLayer, 322, 45, 3, 0xFFFFFF, LV_OPA_70);
  creerPastille(nightLayer, 410, 26, 2, 0xFFFFFF, LV_OPA_70);

  lv_obj_t * sea = lv_obj_create(parent);
  lv_obj_remove_style_all(sea);
  lv_obj_set_size(sea, SCREEN_WIDTH, 116);
  lv_obj_set_pos(sea, 0, 156);
  lv_obj_set_style_bg_color(sea, lv_color_hex(0x095C83), 0);
  lv_obj_set_style_bg_grad_color(sea, lv_color_hex(0x052D46), 0);
  lv_obj_set_style_bg_grad_dir(sea, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(sea, LV_OPA_COVER, 0);

  for (int i = 0; i < 5; i++) {
    lv_obj_t * wave = lv_obj_create(sea);
    lv_obj_remove_style_all(wave);
    lv_obj_set_size(wave, 146, 2);
    lv_obj_set_pos(wave, -30 + (i * 108), 24 + ((i % 2) * 20));
    lv_obj_set_style_bg_color(wave, lv_color_hex(0x9FEAFF), 0);
    lv_obj_set_style_bg_opa(wave, LV_OPA_50, 0);
  }

  lv_obj_t * horizon = lv_obj_create(parent);
  lv_obj_remove_style_all(horizon);
  lv_obj_set_size(horizon, SCREEN_WIDTH, 2);
  lv_obj_set_pos(horizon, 0, 155);
  lv_obj_set_style_bg_color(horizon, lv_color_hex(0xFFF3C5), 0);
  lv_obj_set_style_bg_opa(horizon, LV_OPA_80, 0);

  sunReflection = lv_obj_create(parent);
  lv_obj_remove_style_all(sunReflection);
  lv_obj_set_size(sunReflection, 28, 72);
  lv_obj_set_pos(sunReflection, 444, 166);
  lv_obj_set_style_radius(sunReflection, 14, 0);
  lv_obj_set_style_bg_color(sunReflection, lv_color_hex(0xFFD66B), 0);
  lv_obj_set_style_bg_opa(sunReflection, LV_OPA_30, 0);

  configurerCiel(true);
}

static void creerHorlogeAnalogique(lv_obj_t * parent)
{
  analogClock = lv_obj_create(parent);
  lv_obj_remove_flag(analogClock, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(analogClock, CLOCK_SIZE, CLOCK_SIZE);
  lv_obj_set_style_radius(analogClock, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(analogClock, lv_color_hex(0xF7FCFF), 0);
  lv_obj_set_style_bg_opa(analogClock, LV_OPA_90, 0);
  lv_obj_set_style_border_width(analogClock, 0, 0);
  lv_obj_set_style_outline_color(analogClock, lv_color_hex(0x74DFFF), 0);
  lv_obj_set_style_outline_width(analogClock, 3, 0);
  lv_obj_set_style_outline_pad(analogClock, 0, 0);
  lv_obj_set_style_shadow_width(analogClock, 18, 0);
  lv_obj_set_style_shadow_opa(analogClock, LV_OPA_30, 0);
  lv_obj_set_style_shadow_color(analogClock, lv_color_hex(0x06233A), 0);
  lv_obj_set_style_pad_all(analogClock, 0, 0);
  lv_obj_align(analogClock, LV_ALIGN_CENTER, 150, -34);

  lv_obj_t * innerRing = lv_obj_create(analogClock);
  lv_obj_remove_style_all(innerRing);
  lv_obj_set_size(innerRing, CLOCK_SIZE - 16, CLOCK_SIZE - 16);
  lv_obj_set_pos(innerRing, 8, 8);
  lv_obj_set_style_radius(innerRing, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(innerRing, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(innerRing, lv_color_hex(0xD5EEF8), 0);
  lv_obj_set_style_border_width(innerRing, 1, 0);

  for (int i = 0; i < 12; i++) {
    float angle = ((i * 30.0f) - 90.0f) * PI / 180.0f;
    int taille = (i % 3 == 0) ? 6 : 3;
    int rayon = CLOCK_CENTER - 10;
    lv_obj_t * tick = creerPastille(analogClock,
                                    CLOCK_CENTER + static_cast<int>(std::cos(angle) * rayon) - (taille / 2),
                                    CLOCK_CENTER + static_cast<int>(std::sin(angle) * rayon) - (taille / 2),
                                    taille, (i % 3 == 0) ? 0x103348 : 0x74DFFF);
    lv_obj_move_foreground(tick);
  }

  hourHand = lv_line_create(analogClock);
  minuteHand = lv_line_create(analogClock);
  secondHand = lv_line_create(analogClock);
  lv_obj_set_size(hourHand, CLOCK_SIZE, CLOCK_SIZE);
  lv_obj_set_size(minuteHand, CLOCK_SIZE, CLOCK_SIZE);
  lv_obj_set_size(secondHand, CLOCK_SIZE, CLOCK_SIZE);
  lv_obj_set_pos(hourHand, 0, 0);
  lv_obj_set_pos(minuteHand, 0, 0);
  lv_obj_set_pos(secondHand, 0, 0);
  lv_obj_set_style_line_width(hourHand, 5, 0);
  lv_obj_set_style_line_width(minuteHand, 3, 0);
  lv_obj_set_style_line_width(secondHand, 2, 0);
  lv_obj_set_style_line_rounded(hourHand, true, 0);
  lv_obj_set_style_line_rounded(minuteHand, true, 0);
  lv_obj_set_style_line_rounded(secondHand, true, 0);
  lv_obj_set_style_line_color(hourHand, lv_color_hex(0x08263A), 0);
  lv_obj_set_style_line_color(minuteHand, lv_color_hex(0x0E5A7A), 0);
  lv_obj_set_style_line_color(secondHand, lv_color_hex(0xFF6B4A), 0);
  mettreAJourHorlogeAnalogique(0, 0, 0);

  creerPastille(analogClock, CLOCK_CENTER - 6, CLOCK_CENTER - 6, 12, 0xFFFFFF);
  creerPastille(analogClock, CLOCK_CENTER - 4, CLOCK_CENTER - 4, 8, 0xFF6B4A);
}

static void mettreAJourResumeAlarmes()
{
  int count = 0;
  char resume[96] = "Aucun reveil";

  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarms[i].active) {
      count++;
    }
  }

  if (count > 0) {
    char badge[8];
    std::snprintf(badge, sizeof(badge), "%d", count);
    lv_label_set_text(alarmBadgeLabel, badge);
    lv_obj_remove_flag(alarmBadgeLabel, LV_OBJ_FLAG_HIDDEN);

    int offset = std::snprintf(resume, sizeof(resume), count > 1 ? "Reveils" : "Reveil");
    for (int i = 0; i < MAX_ALARMS && offset < static_cast<int>(sizeof(resume)); i++) {
      if (alarms[i].active) {
        offset += std::snprintf(resume + offset, sizeof(resume) - offset,
                                " %02d:%02d", alarms[i].hour, alarms[i].minute);
      }
    }
  }
  else {
    lv_obj_add_flag(alarmBadgeLabel, LV_OBJ_FLAG_HIDDEN);
  }

  lv_label_set_text(alarmSummaryLabel, resume);

  char countText[20];
  std::snprintf(countText, sizeof(countText), "%d/%d actifs", count, MAX_ALARMS);
  lv_label_set_text(alarmCountLabel, countText);

  if (hourRoller != nullptr) {
    selectedAlarmHour = static_cast<int>(lv_roller_get_selected(hourRoller));
  }
  if (minuteRoller != nullptr) {
    selectedAlarmMinute = static_cast<int>(lv_roller_get_selected(minuteRoller));
  }

  char preview[8];
  std::snprintf(preview, sizeof(preview), "%02d:%02d", selectedAlarmHour, selectedAlarmMinute);
  lv_label_set_text(alarmPreviewLabel, preview);

  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarms[i].active) {
      char texte[12];
      std::snprintf(texte, sizeof(texte), "%02d:%02d", alarms[i].hour, alarms[i].minute);
      lv_label_set_text(lv_obj_get_child(alarmChip[i], 0), texte);
      lv_obj_remove_flag(alarmChip[i], LV_OBJ_FLAG_HIDDEN);
    }
    else {
      lv_obj_add_flag(alarmChip[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void masquerPageAlarmes(lv_anim_t * a)
{
  (void)a;
  lv_obj_add_flag(alarmPage, LV_OBJ_FLAG_HIDDEN);
}

static void ouvrirPageAlarmes(lv_event_t * e)
{
  (void)e;
  if (alarmPageOpen) {
    return;
  }

  alarmPageOpen = true;
  lv_obj_remove_flag(alarmPage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(alarmPage);
  animerY(mainPage, lv_obj_get_y(mainPage), -SCREEN_HEIGHT);
  animerY(alarmPage, lv_obj_get_y(alarmPage), 0, 40);
}

static void fermerPageAlarmes(lv_event_t * e)
{
  (void)e;
  if (!alarmPageOpen) {
    return;
  }

  alarmPageOpen = false;
  animerY(mainPage, lv_obj_get_y(mainPage), 0, 40);
  animerY(alarmPage, lv_obj_get_y(alarmPage), SCREEN_HEIGHT, 0, masquerPageAlarmes);
}

static void selectionnerHeure(lv_event_t * e)
{
  (void)e;
  selectedAlarmHour = static_cast<int>(lv_roller_get_selected(hourRoller));
  mettreAJourResumeAlarmes();
}

static void selectionnerMinute(lv_event_t * e)
{
  (void)e;
  selectedAlarmMinute = static_cast<int>(lv_roller_get_selected(minuteRoller));
  mettreAJourResumeAlarmes();
}

static void ajouterAlarme(lv_event_t * e)
{
  (void)e;
  selectedAlarmHour = static_cast<int>(lv_roller_get_selected(hourRoller));
  selectedAlarmMinute = static_cast<int>(lv_roller_get_selected(minuteRoller));

  if (alarmeExiste(selectedAlarmHour, selectedAlarmMinute)) {
    lv_label_set_text(alarmCountLabel, "Deja actif");
    return;
  }

  for (int i = 0; i < MAX_ALARMS; i++) {
    if (!alarms[i].active) {
      alarms[i].hour = selectedAlarmHour;
      alarms[i].minute = selectedAlarmMinute;
      alarms[i].active = true;
      mettreAJourResumeAlarmes();
      return;
    }
  }
}

static void supprimerAlarme(lv_event_t * e)
{
  int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  if (index >= 0 && index < MAX_ALARMS) {
    alarms[index].active = false;
    mettreAJourResumeAlarmes();
  }
}

static void creerBoutonReveil(lv_obj_t * parent)
{
  lv_obj_t * alarmButton = lv_button_create(parent);
  lv_obj_set_pos(alarmButton, 18, 18);
  lv_obj_set_size(alarmButton, 46, 46);
  lv_obj_set_style_radius(alarmButton, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(alarmButton, lv_color_hex(0xEAFBFF), 0);
  lv_obj_set_style_border_color(alarmButton, lv_color_hex(0x6DE7FF), 0);
  lv_obj_set_style_border_width(alarmButton, 1, 0);
  lv_obj_set_style_shadow_width(alarmButton, 0, 0);
  lv_obj_add_event_cb(alarmButton, ouvrirPageAlarmes, LV_EVENT_CLICKED, nullptr);

  lv_obj_t * icon = lv_obj_create(alarmButton);
  lv_obj_remove_style_all(icon);
  lv_obj_set_size(icon, 24, 24);
  lv_obj_set_style_radius(icon, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_color(icon, lv_color_hex(0x06151F), 0);
  lv_obj_set_style_border_width(icon, 2, 0);
  lv_obj_center(icon);

  lv_obj_t * iconHour = lv_line_create(icon);
  lv_obj_t * iconMinute = lv_line_create(icon);
  lv_line_set_points(iconHour, alarmIconHourPoints, 2);
  lv_line_set_points(iconMinute, alarmIconMinutePoints, 2);
  lv_obj_set_style_line_width(iconHour, 2, 0);
  lv_obj_set_style_line_width(iconMinute, 2, 0);
  lv_obj_set_style_line_color(iconHour, lv_color_hex(0x06151F), 0);
  lv_obj_set_style_line_color(iconMinute, lv_color_hex(0x06151F), 0);

  alarmBadgeLabel = lv_label_create(parent);
  lv_obj_set_size(alarmBadgeLabel, 18, 18);
  lv_obj_set_style_radius(alarmBadgeLabel, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(alarmBadgeLabel, lv_color_hex(0x00E5FF), 0);
  lv_obj_set_style_bg_opa(alarmBadgeLabel, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(alarmBadgeLabel, lv_color_hex(0x06151F), 0);
  lv_obj_set_style_text_align(alarmBadgeLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(alarmBadgeLabel, 58, 14);
  lv_obj_move_foreground(alarmBadgeLabel);

  alarmSummaryLabel = lv_label_create(parent);
  lv_obj_set_width(alarmSummaryLabel, 230);
  lv_label_set_long_mode(alarmSummaryLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(alarmSummaryLabel, lv_color_hex(0xEAFBFF), 0);
  lv_obj_align(alarmSummaryLabel, LV_ALIGN_TOP_LEFT, 76, 32);
  lv_obj_add_flag(alarmSummaryLabel, LV_OBJ_FLAG_HIDDEN);
}

static void creerPageAlarmes(lv_obj_t * screen)
{
  remplirOptionsNombre(hourOptions, sizeof(hourOptions), 24);
  remplirOptionsNombre(minuteOptions, sizeof(minuteOptions), 60);

  alarmPage = lv_obj_create(screen);
  lv_obj_remove_flag(alarmPage, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(alarmPage, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(alarmPage, 0, SCREEN_HEIGHT);
  lv_obj_set_style_radius(alarmPage, 0, 0);
  lv_obj_set_style_bg_color(alarmPage, lv_color_hex(0x0B6F9C), 0);
  lv_obj_set_style_bg_grad_color(alarmPage, lv_color_hex(0x010813), 0);
  lv_obj_set_style_bg_grad_dir(alarmPage, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(alarmPage, 0, 0);
  lv_obj_set_style_pad_all(alarmPage, 0, 0);

  lv_obj_t * fish1 = creerPoisson(alarmPage, -50, 58, 0xFFCA6A);
  lv_obj_t * fish2 = creerPoisson(alarmPage, -90, 176, 0x7EEBFF);
  lv_obj_t * fish3 = creerPoisson(alarmPage, -120, 124, 0xF5F7FA);
  animerXInfini(fish1, -50, SCREEN_WIDTH + 30, 15000, 0);
  animerXInfini(fish2, -90, SCREEN_WIDTH + 40, 21000, 5000);
  animerXInfini(fish3, -120, SCREEN_WIDTH + 50, 26000, 10000);

  for (int i = 0; i < 10; i++) {
    lv_obj_t * bubble = creerPastille(alarmPage, 24 + (i * 46), 32 + ((i % 4) * 36),
                                      5 + (i % 3), 0xA9F5FF, LV_OPA_40);
    animerY(bubble, lv_obj_get_y(bubble) + 90, lv_obj_get_y(bubble) - 12,
            static_cast<uint32_t>(i * 130));
  }

  lv_obj_t * glow = lv_obj_create(alarmPage);
  lv_obj_remove_style_all(glow);
  lv_obj_set_size(glow, 416, 118);
  lv_obj_set_pos(glow, 32, 78);
  lv_obj_set_style_radius(glow, 8, 0);
  lv_obj_set_style_bg_color(glow, lv_color_hex(0x06233A), 0);
  lv_obj_set_style_bg_grad_color(glow, lv_color_hex(0x075477), 0);
  lv_obj_set_style_bg_grad_dir(glow, LV_GRAD_DIR_HOR, 0);
  lv_obj_set_style_bg_opa(glow, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(glow, lv_color_hex(0x4BEAFF), 0);
  lv_obj_set_style_border_width(glow, 1, 0);

  lv_obj_t * title = lv_label_create(alarmPage);
  lv_label_set_text(title, "Reveils");
  lv_obj_set_style_text_color(title, lv_color_hex(0xEAFBFF), 0);
  lv_obj_set_pos(title, 24, 20);

  lv_obj_t * subtitle = lv_label_create(alarmPage);
  lv_label_set_text(subtitle, "Configuration reveils");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0x7EEBFF), 0);
  lv_obj_set_pos(subtitle, 24, 43);

  lv_obj_t * closeButton = creerBouton(alarmPage, LV_SYMBOL_CLOSE, 424, 18, 34, 34, fermerPageAlarmes);
  lv_obj_set_style_bg_color(closeButton, lv_color_hex(0x09243A), 0);
  lv_obj_set_style_text_color(lv_obj_get_child(closeButton, 0), lv_color_hex(0xEAFBFF), 0);

  hourRoller = lv_roller_create(alarmPage);
  lv_roller_set_options(hourRoller, hourOptions, LV_ROLLER_MODE_INFINITE);
  lv_roller_set_visible_row_count(hourRoller, 3);
  lv_roller_set_selected(hourRoller, selectedAlarmHour, LV_ANIM_OFF);
  lv_obj_set_size(hourRoller, 116, 104);
  lv_obj_set_pos(hourRoller, 92, 84);
  lv_obj_set_style_radius(hourRoller, 8, 0);
  lv_obj_set_style_bg_color(hourRoller, lv_color_hex(0x031827), 0);
  lv_obj_set_style_bg_opa(hourRoller, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(hourRoller, 0, 0);
  lv_obj_set_style_text_color(hourRoller, lv_color_hex(0x8FB8C8), 0);
  lv_obj_set_style_text_align(hourRoller, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_color(hourRoller, lv_color_hex(0xEAFBFF), LV_PART_SELECTED);
  lv_obj_set_style_text_color(hourRoller, lv_color_hex(0x031827), LV_PART_SELECTED);
  lv_obj_add_event_cb(hourRoller, selectionnerHeure, LV_EVENT_VALUE_CHANGED, nullptr);

  minuteRoller = lv_roller_create(alarmPage);
  lv_roller_set_options(minuteRoller, minuteOptions, LV_ROLLER_MODE_INFINITE);
  lv_roller_set_visible_row_count(minuteRoller, 3);
  lv_roller_set_selected(minuteRoller, selectedAlarmMinute, LV_ANIM_OFF);
  lv_obj_set_size(minuteRoller, 116, 104);
  lv_obj_set_pos(minuteRoller, 272, 84);
  lv_obj_set_style_radius(minuteRoller, 8, 0);
  lv_obj_set_style_bg_color(minuteRoller, lv_color_hex(0x031827), 0);
  lv_obj_set_style_bg_opa(minuteRoller, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(minuteRoller, 0, 0);
  lv_obj_set_style_text_color(minuteRoller, lv_color_hex(0x8FB8C8), 0);
  lv_obj_set_style_text_align(minuteRoller, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_color(minuteRoller, lv_color_hex(0xEAFBFF), LV_PART_SELECTED);
  lv_obj_set_style_text_color(minuteRoller, lv_color_hex(0x031827), LV_PART_SELECTED);
  lv_obj_add_event_cb(minuteRoller, selectionnerMinute, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t * separator = lv_label_create(alarmPage);
  lv_label_set_text(separator, ":");
  lv_obj_set_style_text_color(separator, lv_color_hex(0x7EEBFF), 0);
#if LV_FONT_MONTSERRAT_48
  lv_obj_set_style_text_font(separator, &lv_font_montserrat_48, 0);
#endif
  lv_obj_set_pos(separator, 229, 103);

  alarmPreviewLabel = lv_label_create(alarmPage);
  lv_obj_set_style_text_color(alarmPreviewLabel, lv_color_hex(0xFFFFFF), 0);
#if LV_FONT_MONTSERRAT_48
  lv_obj_set_style_text_font(alarmPreviewLabel, &lv_font_montserrat_48, 0);
#endif
  lv_obj_set_width(alarmPreviewLabel, 160);
  lv_obj_set_style_text_align(alarmPreviewLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(alarmPreviewLabel, 160, 196);

  creerBouton(alarmPage, "Ajouter", 332, 214, 112, 36, ajouterAlarme);

  alarmCountLabel = lv_label_create(alarmPage);
  lv_obj_set_width(alarmCountLabel, 110);
  lv_obj_set_style_text_color(alarmCountLabel, lv_color_hex(0x9EDBE8), 0);
  lv_obj_set_style_text_align(alarmCountLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(alarmCountLabel, 334, 56);

  for (int i = 0; i < MAX_ALARMS; i++) {
    alarmChip[i] = creerBouton(alarmPage, "00:00", 24 + (i * 72), 214, 58, 26,
                               supprimerAlarme,
                               reinterpret_cast<void *>(static_cast<intptr_t>(i)));
    lv_obj_set_style_bg_color(alarmChip[i], lv_color_hex(0x09243A), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(alarmChip[i], 0), lv_color_hex(0xEAFBFF), 0);
  }

  lv_obj_add_flag(alarmPage, LV_OBJ_FLAG_HIDDEN);
}

#ifdef ARDUINO
static void demarrerBuzzer();
static void arreterBuzzer();
#endif

static void fermerOverlayReveil()
{
  alarmRinging = false;
  lv_obj_add_flag(alarmOverlay, LV_OBJ_FLAG_HIDDEN);
#ifdef ARDUINO
  arreterBuzzer();
#endif
}

static void stopReveil(lv_event_t * e)
{
  (void)e;
  fermerOverlayReveil();
}

static void reporterReveil(lv_event_t * e)
{
  (void)e;
  uint32_t snoozeSecond = (currentSecondOfDay + (5UL * 60UL)) % SECONDS_PER_DAY;
  int hour = static_cast<int>(snoozeSecond / 3600UL);
  int minute = static_cast<int>((snoozeSecond / 60UL) % 60UL);

  if (!alarmeExiste(hour, minute)) {
    for (int i = 0; i < MAX_ALARMS; i++) {
      if (!alarms[i].active) {
        alarms[i].hour = hour;
        alarms[i].minute = minute;
        alarms[i].active = true;
        break;
      }
    }
  }

  fermerOverlayReveil();
  mettreAJourResumeAlarmes();
}

static void creerOverlayReveil(lv_obj_t * screen)
{
  alarmOverlay = lv_obj_create(screen);
  lv_obj_remove_flag(alarmOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(alarmOverlay, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(alarmOverlay, 0, 0);
  lv_obj_set_style_radius(alarmOverlay, 0, 0);
  lv_obj_set_style_bg_color(alarmOverlay, lv_color_hex(0x03101E), 0);
  lv_obj_set_style_bg_opa(alarmOverlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(alarmOverlay, 0, 0);
  lv_obj_set_style_pad_all(alarmOverlay, 0, 0);

  creerPastille(alarmOverlay, 176, 28, 128, 0xFFDA7A, LV_OPA_40);
  creerPastille(alarmOverlay, 200, 52, 80, 0xFFF3C5, LV_OPA_60);

  lv_obj_t * panel = lv_obj_create(alarmOverlay);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(panel, 260, 156);
  lv_obj_align(panel, LV_ALIGN_CENTER, 0, 12);
  lv_obj_set_style_radius(panel, 8, 0);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(0xDDEAF3), 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);

  lv_obj_t * title = lv_label_create(panel);
  lv_label_set_text(title, "REVEIL");
  lv_obj_set_style_text_color(title, lv_color_hex(0x06151F), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

  ringingTimeLabel = lv_label_create(panel);
  lv_label_set_text(ringingTimeLabel, "--:--");
  lv_obj_set_style_text_color(ringingTimeLabel, lv_color_hex(0x06151F), 0);
#if LV_FONT_MONTSERRAT_48
  lv_obj_set_style_text_font(ringingTimeLabel, &lv_font_montserrat_48, 0);
#endif
  lv_obj_align(ringingTimeLabel, LV_ALIGN_TOP_MID, 0, 42);

  creerBouton(panel, "Stop", 32, 108, 84, 34, stopReveil);
  creerBouton(panel, "+5 min", 144, 108, 84, 34, reporterReveil);

  lv_obj_add_flag(alarmOverlay, LV_OBJ_FLAG_HIDDEN);
}

static void declencherReveil(int hour, int minute)
{
  if (alarmRinging) {
    return;
  }

  alarmRinging = true;
  ringingHour = hour;
  ringingMinute = minute;

  char texte[8];
  std::snprintf(texte, sizeof(texte), "%02d:%02d", ringingHour, ringingMinute);
  lv_label_set_text(ringingTimeLabel, texte);
  lv_obj_remove_flag(alarmOverlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(alarmOverlay);
#ifdef ARDUINO
  demarrerBuzzer();
#endif
}

static void verifierAlarmes(uint32_t secondOfDay)
{
  if (alarmRinging || !clockReady || (secondOfDay % 60UL) != 0) {
    return;
  }

  int hour = static_cast<int>(secondOfDay / 3600UL);
  int minute = static_cast<int>((secondOfDay / 60UL) % 60UL);

  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarms[i].active && alarms[i].hour == hour && alarms[i].minute == minute) {
      alarms[i].active = false;
      mettreAJourResumeAlarmes();
      declencherReveil(hour, minute);
      return;
    }
  }
}

static void afficherHeure(uint32_t secondOfDay, const char * statusText)
{
  currentSecondOfDay = secondOfDay % SECONDS_PER_DAY;
  int heure = static_cast<int>(currentSecondOfDay / 3600UL);
  int minute = static_cast<int>((currentSecondOfDay / 60UL) % 60UL);
  int seconde = static_cast<int>(currentSecondOfDay % 60UL);

  char texteHeure[16];
  std::snprintf(texteHeure, sizeof(texteHeure), "%02d:%02d:%02d", heure, minute, seconde);

  configurerCiel(heure >= 9 && heure < 21);
  lv_label_set_text(timeLabel, texteHeure);
  lv_label_set_text(statusLabel, statusText);
  mettreAJourHorlogeAnalogique(heure, minute, seconde);
  verifierAlarmes(currentSecondOfDay);
}

void testLvgl()
{
  lv_obj_t * screen = lv_screen_active();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x071B34), 0);
  lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x052D46), 0);
  lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);

  mainPage = lv_obj_create(screen);
  lv_obj_remove_flag(mainPage, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(mainPage, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(mainPage, 0, 0);
  lv_obj_set_style_radius(mainPage, 0, 0);
  lv_obj_set_style_border_width(mainPage, 0, 0);
  lv_obj_set_style_pad_all(mainPage, 0, 0);

  creerDecorPrincipal(mainPage);
  creerBoutonReveil(mainPage);

  timeLabel = lv_label_create(mainPage);
  lv_label_set_text(timeLabel, "--:--:--");
  lv_obj_set_style_text_color(timeLabel, lv_color_hex(0xFFFFFF), 0);
#if LV_FONT_MONTSERRAT_48
  lv_obj_set_style_text_font(timeLabel, &lv_font_montserrat_48, 0);
#endif
  lv_obj_align(timeLabel, LV_ALIGN_CENTER, -34, -24);

  creerHorlogeAnalogique(mainPage);

  statusLabel = lv_label_create(mainPage);
  lv_label_set_text(statusLabel, "Heure non synchronisee");
  lv_obj_set_width(statusLabel, 420);
  lv_obj_set_style_text_color(statusLabel, lv_color_hex(0xDDF7FF), 0);
  lv_obj_set_style_text_align(statusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(statusLabel, LV_ALIGN_CENTER, 0, 44);

  creerPageAlarmes(screen);
  creerOverlayReveil(screen);
  mettreAJourResumeAlarmes();
}

#ifdef ARDUINO

#include "lvglDrivers.h"

static constexpr uint32_t GPS_BAUDRATE = 9600;
static constexpr int GPS_LOCAL_UTC_OFFSET_HOURS = 2;
static constexpr uint8_t BUZZER_PIN = PC7;
static constexpr uint8_t BUZZER_DUTY = 128;
static constexpr int MELODY_NOTE_COUNT = 15;
static constexpr uint16_t MELODY_FREQUENCY[MELODY_NOTE_COUNT] = {
  330, 330, 349, 392, 392, 349, 330, 294, 262, 262, 294, 330, 330, 294, 294
};
static constexpr uint16_t MELODY_DURATION_MS[MELODY_NOTE_COUNT] = {
  260, 260, 260, 260, 260, 260, 260, 260, 260, 260, 260, 260, 360, 220, 520
};

static HardwareSerial gpsSerial(PF6, PF7);
static char gpsLine[128];
static size_t gpsLineLen = 0;
static bool clockSynchronized = false;
static uint32_t referenceSecondOfDay = 0;
static uint32_t lastDisplayedSecondOfDay = UINT32_MAX;
static TickType_t referenceTick = 0;
static TickType_t noteStartTick = 0;
static int melodyIndex = 0;

static void demarrerBuzzer()
{
  noteStartTick = 0;
  melodyIndex = 0;
}

static void arreterBuzzer()
{
  analogWrite(BUZZER_PIN, 0);
  digitalWrite(BUZZER_PIN, LOW);
}

static void mettreAJourBuzzer()
{
  if (!alarmRinging) {
    return;
  }

  TickType_t now = xTaskGetTickCount();
  if (noteStartTick == 0) {
    noteStartTick = now;
    analogWriteFrequency(MELODY_FREQUENCY[melodyIndex]);
    analogWrite(BUZZER_PIN, BUZZER_DUTY);
  }
  else if ((now - noteStartTick) >= pdMS_TO_TICKS(MELODY_DURATION_MS[melodyIndex])) {
    noteStartTick = now;
    melodyIndex = (melodyIndex + 1) % MELODY_NOTE_COUNT;
    analogWriteFrequency(MELODY_FREQUENCY[melodyIndex]);
    analogWrite(BUZZER_PIN, BUZZER_DUTY);
  }
}

static uint32_t appliquerOffsetLocal(int h, int m, int s)
{
  int total = (h * 3600) + (m * 60) + s + (GPS_LOCAL_UTC_OFFSET_HOURS * 3600);
  total %= static_cast<int>(SECONDS_PER_DAY);
  if (total < 0) {
    total += static_cast<int>(SECONDS_PER_DAY);
  }
  return static_cast<uint32_t>(total);
}

static uint32_t calculerSecondeLocale()
{
  TickType_t elapsedTicks = xTaskGetTickCount() - referenceTick;
  uint32_t elapsedSeconds = static_cast<uint32_t>(
    (static_cast<uint64_t>(elapsedTicks) * portTICK_PERIOD_MS) / 1000ULL
  );

  return (referenceSecondOfDay + elapsedSeconds) % SECONDS_PER_DAY;
}

static void afficherHeureProtegee(uint32_t secondOfDay, const char * statusText)
{
  if (lvglLock(pdMS_TO_TICKS(20))) {
    afficherHeure(secondOfDay, statusText);
    lvglUnlock();
  }
}

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

  const char * virguleApresHeure = std::strchr(heureUtc, ',');
  if (virguleApresHeure == nullptr || virguleApresHeure[1] != 'A') {
    return false;
  }

  int heure = (heureUtc[0] - '0') * 10 + (heureUtc[1] - '0');
  int minute = (heureUtc[2] - '0') * 10 + (heureUtc[3] - '0');
  int seconde = (heureUtc[4] - '0') * 10 + (heureUtc[5] - '0');

  referenceSecondOfDay = appliquerOffsetLocal(heure, minute, seconde);
  referenceTick = xTaskGetTickCount();
  clockSynchronized = true;
  clockReady = true;
  lastDisplayedSecondOfDay = referenceSecondOfDay;

  afficherHeureProtegee(referenceSecondOfDay, "");

  return true;
}

static void mettreAJourHorlogeComptee()
{
  if (!clockSynchronized) {
    return;
  }

  uint32_t secondOfDay = calculerSecondeLocale();
  if (secondOfDay == lastDisplayedSecondOfDay) {
    return;
  }

  lastDisplayedSecondOfDay = secondOfDay;
  afficherHeureProtegee(secondOfDay, "");
}

void mySetup()
{
  testLvgl();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  analogWriteFrequency(2000);

  gpsSerial.begin(GPS_BAUDRATE);
}

void loop()
{
  // Inactif : FreeRTOS execute les taches.
}

void myTask(void *pvParameters)
{
  (void)pvParameters;

  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1)
  {
    while (gpsSerial.available() > 0) {
      char c = static_cast<char>(gpsSerial.read());

      if (c == '\r') {
        continue;
      }

      if (c == '\n') {
        gpsLine[gpsLineLen] = '\0';
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

    mettreAJourHorlogeComptee();
    mettreAJourBuzzer();

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
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
