#include "lvgl.h"
#include <cmath>
#include <cstdint>
#include <cstdio>

static constexpr int CLOCK_SIZE = 88;
static constexpr int CLOCK_CENTER = CLOCK_SIZE / 2;
static constexpr int SCREEN_WIDTH = 480;
static constexpr int SCREEN_HEIGHT = 272;
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
static lv_obj_t * mainPage;
static lv_obj_t * alarmPage;
static lv_obj_t * alarmButtonLabel;
static lv_obj_t * alarmSummaryLabel;
static lv_obj_t * alarmTimeLabel;
static lv_obj_t * alarmPanelStatusLabel;
static lv_obj_t * hourRoller;
static lv_obj_t * minuteRoller;
static lv_obj_t * alarmRowLabel[MAX_ALARMS];
static lv_obj_t * alarmDeleteButton[MAX_ALARMS];
static lv_point_precise_t alarmIconHourPoints[2] = {{11, 11}, {11, 5}};
static lv_point_precise_t alarmIconMinutePoints[2] = {{11, 11}, {16, 11}};
static AlarmEntry alarms[MAX_ALARMS];
static int selectedAlarmHour = 7;
static int selectedAlarmMinute = 0;
static bool alarmPageOpen = false;
static char hourOptions[80];
static char minuteOptions[192];

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

static void animerPositionY(lv_obj_t * objet, int32_t depart, int32_t arrivee,
                            uint32_t delai = 0,
                            lv_anim_completed_cb_t callbackFin = nullptr)
{
  lv_anim_delete(objet, reinterpret_cast<lv_anim_exec_xcb_t>(lv_obj_set_y));

  lv_anim_t animation;
  lv_anim_init(&animation);
  lv_anim_set_var(&animation, objet);
  lv_anim_set_exec_cb(&animation, reinterpret_cast<lv_anim_exec_xcb_t>(lv_obj_set_y));
  lv_anim_set_values(&animation, depart, arrivee);
  lv_anim_set_duration(&animation, 360);
  lv_anim_set_delay(&animation, delai);
  lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
  if (callbackFin != nullptr) {
    lv_anim_set_completed_cb(&animation, callbackFin);
  }
  lv_anim_start(&animation);
}

static void mettreAJourInterfaceAlarmes()
{
  int count = 0;
  char summaryText[96] = "Aucun reveil";

  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarms[i].active) {
      count++;
    }
  }

  if (count > 0) {
    char badgeText[8];
    std::snprintf(badgeText, sizeof(badgeText), "%d", count);
    lv_label_set_text(alarmButtonLabel, badgeText);
    lv_obj_remove_flag(alarmButtonLabel, LV_OBJ_FLAG_HIDDEN);
  }
  else {
    lv_obj_add_flag(alarmButtonLabel, LV_OBJ_FLAG_HIDDEN);
  }

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

  if (hourRoller != nullptr) {
    selectedAlarmHour = static_cast<int>(lv_roller_get_selected(hourRoller));
  }
  if (minuteRoller != nullptr) {
    selectedAlarmMinute = static_cast<int>(lv_roller_get_selected(minuteRoller));
  }

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
      lv_label_set_text(lv_obj_get_child(alarmDeleteButton[i], 0), alarmText);
      lv_obj_add_flag(alarmRowLabel[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(alarmDeleteButton[i], LV_OBJ_FLAG_HIDDEN);
    }
    else {
      lv_obj_add_flag(alarmRowLabel[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(alarmDeleteButton[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void masquerPageAlarmes(lv_anim_t * a)
{
  (void)a;
  lv_obj_add_flag(alarmPage, LV_OBJ_FLAG_HIDDEN);
}

static void ouvrirPanneauAlarmes(lv_event_t * e)
{
  (void)e;
  if (alarmPageOpen) {
    return;
  }

  alarmPageOpen = true;
  lv_obj_remove_flag(alarmPage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(alarmPage);
  animerPositionY(mainPage, lv_obj_get_y(mainPage), -SCREEN_HEIGHT);
  animerPositionY(alarmPage, lv_obj_get_y(alarmPage), 0, 40);
}

static void fermerPanneauAlarmes(lv_event_t * e)
{
  (void)e;
  if (!alarmPageOpen) {
    return;
  }

  alarmPageOpen = false;
  animerPositionY(mainPage, lv_obj_get_y(mainPage), 0, 40);
  animerPositionY(alarmPage, lv_obj_get_y(alarmPage), SCREEN_HEIGHT, 0, masquerPageAlarmes);
}

static void selectionnerHeureAlarme(lv_event_t * e)
{
  (void)e;
  selectedAlarmHour = static_cast<int>(lv_roller_get_selected(hourRoller));
  mettreAJourInterfaceAlarmes();
}

static void selectionnerMinuteAlarme(lv_event_t * e)
{
  (void)e;
  selectedAlarmMinute = static_cast<int>(lv_roller_get_selected(minuteRoller));
  mettreAJourInterfaceAlarmes();
}

static void ajouterAlarme(lv_event_t * e)
{
  (void)e;

  for (int i = 0; i < MAX_ALARMS; i++) {
    if (!alarms[i].active) {
      selectedAlarmHour = static_cast<int>(lv_roller_get_selected(hourRoller));
      selectedAlarmMinute = static_cast<int>(lv_roller_get_selected(minuteRoller));
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
  remplirOptionsNombre(hourOptions, sizeof(hourOptions), 24);
  remplirOptionsNombre(minuteOptions, sizeof(minuteOptions), 60);

  lv_obj_t * alarmButton = lv_button_create(mainPage);
  lv_obj_set_pos(alarmButton, 18, 18);
  lv_obj_set_size(alarmButton, 46, 46);
  lv_obj_set_style_radius(alarmButton, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(alarmButton, lv_color_hex(0xEEF6FF), 0);
  lv_obj_set_style_border_color(alarmButton, lv_color_hex(0x78DFFF), 0);
  lv_obj_set_style_border_width(alarmButton, 1, 0);
  lv_obj_set_style_shadow_width(alarmButton, 0, 0);
  lv_obj_add_event_cb(alarmButton, ouvrirPanneauAlarmes, LV_EVENT_CLICKED, nullptr);

  lv_obj_t * alarmIcon = lv_obj_create(alarmButton);
  lv_obj_remove_style_all(alarmIcon);
  lv_obj_set_size(alarmIcon, 22, 22);
  lv_obj_set_style_radius(alarmIcon, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_color(alarmIcon, lv_color_hex(0x101820), 0);
  lv_obj_set_style_border_width(alarmIcon, 2, 0);
  lv_obj_center(alarmIcon);

  lv_obj_t * iconHourHand = lv_line_create(alarmIcon);
  lv_obj_t * iconMinuteHand = lv_line_create(alarmIcon);
  lv_line_set_points(iconHourHand, alarmIconHourPoints, 2);
  lv_line_set_points(iconMinuteHand, alarmIconMinutePoints, 2);
  lv_obj_set_style_line_width(iconHourHand, 2, 0);
  lv_obj_set_style_line_width(iconMinuteHand, 2, 0);
  lv_obj_set_style_line_rounded(iconHourHand, true, 0);
  lv_obj_set_style_line_rounded(iconMinuteHand, true, 0);
  lv_obj_set_style_line_color(iconHourHand, lv_color_hex(0x101820), 0);
  lv_obj_set_style_line_color(iconMinuteHand, lv_color_hex(0x101820), 0);

  alarmButtonLabel = lv_label_create(alarmButton);
  lv_obj_set_size(alarmButtonLabel, 18, 18);
  lv_obj_set_style_radius(alarmButtonLabel, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(alarmButtonLabel, lv_color_hex(0x00E5FF), 0);
  lv_obj_set_style_bg_opa(alarmButtonLabel, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(alarmButtonLabel, lv_color_hex(0x061018), 0);
  lv_obj_set_style_text_align(alarmButtonLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(alarmButtonLabel, LV_ALIGN_TOP_RIGHT, 5, -5);

  alarmSummaryLabel = lv_label_create(mainPage);
  lv_obj_set_width(alarmSummaryLabel, 210);
  lv_label_set_long_mode(alarmSummaryLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(alarmSummaryLabel, lv_color_hex(0xB8C0CC), 0);
  lv_obj_align(alarmSummaryLabel, LV_ALIGN_TOP_LEFT, 76, 32);

  alarmPage = lv_obj_create(screen);
  lv_obj_remove_flag(alarmPage, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(alarmPage, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(alarmPage, 0, SCREEN_HEIGHT);
  lv_obj_set_style_radius(alarmPage, 0, 0);
  lv_obj_set_style_bg_color(alarmPage, lv_color_hex(0x050A12), 0);
  lv_obj_set_style_bg_grad_color(alarmPage, lv_color_hex(0x10223A), 0);
  lv_obj_set_style_bg_grad_dir(alarmPage, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(alarmPage, 0, 0);
  lv_obj_set_style_pad_all(alarmPage, 0, 0);

  lv_obj_t * glow = lv_obj_create(alarmPage);
  lv_obj_remove_style_all(glow);
  lv_obj_set_size(glow, 416, 114);
  lv_obj_set_pos(glow, 32, 76);
  lv_obj_set_style_radius(glow, 8, 0);
  lv_obj_set_style_bg_color(glow, lv_color_hex(0x0B1B2A), 0);
  lv_obj_set_style_bg_grad_color(glow, lv_color_hex(0x102F46), 0);
  lv_obj_set_style_bg_grad_dir(glow, LV_GRAD_DIR_HOR, 0);
  lv_obj_set_style_bg_opa(glow, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(glow, lv_color_hex(0x1BE7FF), 0);
  lv_obj_set_style_border_width(glow, 1, 0);

  lv_obj_t * title = lv_label_create(alarmPage);
  lv_label_set_text(title, "Reveils");
  lv_obj_set_style_text_color(title, lv_color_hex(0xF5F7FA), 0);
  lv_obj_set_pos(title, 26, 22);

  lv_obj_t * subtitle = lv_label_create(alarmPage);
  lv_label_set_text(subtitle, "Selection horaire");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0x70DFFF), 0);
  lv_obj_set_pos(subtitle, 26, 44);

  lv_obj_t * closeButton = creerBoutonTexte(alarmPage, LV_SYMBOL_UP, 424, 18, 34, 34, fermerPanneauAlarmes);
  lv_obj_set_style_bg_color(closeButton, lv_color_hex(0x12263A), 0);
  lv_obj_set_style_border_color(closeButton, lv_color_hex(0x295D78), 0);
  lv_obj_set_style_border_width(closeButton, 1, 0);

  hourRoller = lv_roller_create(alarmPage);
  lv_roller_set_options(hourRoller, hourOptions, LV_ROLLER_MODE_INFINITE);
  lv_roller_set_visible_row_count(hourRoller, 3);
  lv_roller_set_selected(hourRoller, selectedAlarmHour, LV_ANIM_OFF);
  lv_obj_set_size(hourRoller, 116, 104);
  lv_obj_set_pos(hourRoller, 92, 82);
  lv_obj_set_style_radius(hourRoller, 8, 0);
  lv_obj_set_style_bg_color(hourRoller, lv_color_hex(0x07131F), 0);
  lv_obj_set_style_bg_opa(hourRoller, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(hourRoller, 0, 0);
  lv_obj_set_style_text_color(hourRoller, lv_color_hex(0x8092A5), 0);
  lv_obj_set_style_text_align(hourRoller, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_color(hourRoller, lv_color_hex(0xEAFBFF), LV_PART_SELECTED);
  lv_obj_set_style_text_color(hourRoller, lv_color_hex(0x061018), LV_PART_SELECTED);
  lv_obj_add_event_cb(hourRoller, selectionnerHeureAlarme, LV_EVENT_VALUE_CHANGED, nullptr);

  minuteRoller = lv_roller_create(alarmPage);
  lv_roller_set_options(minuteRoller, minuteOptions, LV_ROLLER_MODE_INFINITE);
  lv_roller_set_visible_row_count(minuteRoller, 3);
  lv_roller_set_selected(minuteRoller, selectedAlarmMinute, LV_ANIM_OFF);
  lv_obj_set_size(minuteRoller, 116, 104);
  lv_obj_set_pos(minuteRoller, 272, 82);
  lv_obj_set_style_radius(minuteRoller, 8, 0);
  lv_obj_set_style_bg_color(minuteRoller, lv_color_hex(0x07131F), 0);
  lv_obj_set_style_bg_opa(minuteRoller, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(minuteRoller, 0, 0);
  lv_obj_set_style_text_color(minuteRoller, lv_color_hex(0x8092A5), 0);
  lv_obj_set_style_text_align(minuteRoller, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_color(minuteRoller, lv_color_hex(0xEAFBFF), LV_PART_SELECTED);
  lv_obj_set_style_text_color(minuteRoller, lv_color_hex(0x061018), LV_PART_SELECTED);
  lv_obj_add_event_cb(minuteRoller, selectionnerMinuteAlarme, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t * separator = lv_label_create(alarmPage);
  lv_label_set_text(separator, ":");
  lv_obj_set_style_text_color(separator, lv_color_hex(0x70DFFF), 0);
#if LV_FONT_MONTSERRAT_48
  lv_obj_set_style_text_font(separator, &lv_font_montserrat_48, 0);
#endif
  lv_obj_set_pos(separator, 229, 102);

  alarmTimeLabel = lv_label_create(alarmPage);
  lv_obj_set_style_text_color(alarmTimeLabel, lv_color_hex(0xFFFFFF), 0);
#if LV_FONT_MONTSERRAT_48
  lv_obj_set_style_text_font(alarmTimeLabel, &lv_font_montserrat_48, 0);
#endif
  lv_obj_set_width(alarmTimeLabel, 160);
  lv_obj_set_style_text_align(alarmTimeLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(alarmTimeLabel, 160, 196);

  lv_obj_t * addButton = creerBoutonTexte(alarmPage, "Ajouter", 332, 214, 112, 36, ajouterAlarme);
  lv_obj_set_style_bg_color(addButton, lv_color_hex(0xEAFBFF), 0);
  lv_obj_set_style_border_color(addButton, lv_color_hex(0x70DFFF), 0);
  lv_obj_set_style_border_width(addButton, 1, 0);
  lv_obj_set_style_text_color(lv_obj_get_child(addButton, 0), lv_color_hex(0x061018), 0);

  alarmPanelStatusLabel = lv_label_create(alarmPage);
  lv_obj_set_width(alarmPanelStatusLabel, 110);
  lv_obj_set_style_text_color(alarmPanelStatusLabel, lv_color_hex(0x8FA3B8), 0);
  lv_obj_set_style_text_align(alarmPanelStatusLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(alarmPanelStatusLabel, 334, 56);

  for (int i = 0; i < MAX_ALARMS; i++) {
    int x = 26 + (i * 72);
    int y = 214;

    alarmDeleteButton[i] = creerBoutonTexte(alarmPage, LV_SYMBOL_CLOSE, x, y - 4, 58, 26,
                                            supprimerAlarme,
                                            reinterpret_cast<void *>(static_cast<intptr_t>(i)));
    lv_obj_set_style_bg_color(alarmDeleteButton[i], lv_color_hex(0x102033), 0);
    lv_obj_set_style_border_color(alarmDeleteButton[i], lv_color_hex(0x295D78), 0);
    lv_obj_set_style_border_width(alarmDeleteButton[i], 1, 0);
    lv_obj_t * deleteLabel = lv_obj_get_child(alarmDeleteButton[i], 0);
    lv_obj_set_style_text_color(deleteLabel, lv_color_hex(0xF5F7FA), 0);

    alarmRowLabel[i] = lv_label_create(alarmPage);
    lv_obj_set_style_text_color(alarmRowLabel[i], lv_color_hex(0xF5F7FA), 0);
    lv_obj_set_pos(alarmRowLabel[i], x + 6, y + 2);
    lv_obj_move_foreground(alarmRowLabel[i]);
  }

  lv_obj_add_flag(alarmPage, LV_OBJ_FLAG_HIDDEN);
  mettreAJourInterfaceAlarmes();
}

void testLvgl()
{
  lv_obj_t * screen = lv_screen_active();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x07111D), 0);
  lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x111827), 0);
  lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);

  mainPage = lv_obj_create(screen);
  lv_obj_remove_flag(mainPage, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(mainPage, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(mainPage, 0, 0);
  lv_obj_set_style_radius(mainPage, 0, 0);
  lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x07111D), 0);
  lv_obj_set_style_bg_grad_color(mainPage, lv_color_hex(0x111827), 0);
  lv_obj_set_style_bg_grad_dir(mainPage, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(mainPage, 0, 0);
  lv_obj_set_style_pad_all(mainPage, 0, 0);

  timeLabel = lv_label_create(mainPage);
  lv_label_set_text(timeLabel, "--:--:--");
  lv_obj_set_style_text_color(timeLabel, lv_color_hex(0xFFFFFF), 0);
#if LV_FONT_MONTSERRAT_48
  lv_obj_set_style_text_font(timeLabel, &lv_font_montserrat_48, 0);
#endif
  lv_obj_align(timeLabel, LV_ALIGN_CENTER, -34, -22);

  analogClock = lv_obj_create(mainPage);
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

  statusLabel = lv_label_create(mainPage);
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
static constexpr uint32_t SECONDS_PER_DAY = 24UL * 60UL * 60UL;

// UART7 de la DISCO-F746NG : PF6 = RX7, PF7 = TX7.
static HardwareSerial gpsSerial(PF6, PF7);
static char gpsLine[128];
static size_t gpsLineLen = 0;
static bool clockSynchronized = false;
static uint32_t referenceSecondOfDay = 0;
static uint32_t lastDisplayedSecondOfDay = UINT32_MAX;
static TickType_t referenceTick = 0;

static void afficherHeureDepuisSecondes(uint32_t secondOfDay, const char * statusText)
{
  int heure = static_cast<int>(secondOfDay / 3600UL);
  int minute = static_cast<int>((secondOfDay / 60UL) % 60UL);
  int seconde = static_cast<int>(secondOfDay % 60UL);

  char texteHeure[16];
  std::snprintf(texteHeure, sizeof(texteHeure), "%02d:%02d:%02d", heure, minute, seconde);

  if (lvglLock(pdMS_TO_TICKS(20))) {
    lv_label_set_text(timeLabel, texteHeure);
    lv_label_set_text(statusLabel, statusText);
    mettreAJourHorlogeAnalogique(heure, minute, seconde);
    lvglUnlock();
  }
}

static uint32_t calculerSecondeLocale()
{
  TickType_t elapsedTicks = xTaskGetTickCount() - referenceTick;
  uint32_t elapsedSeconds = static_cast<uint32_t>(
    (static_cast<uint64_t>(elapsedTicks) * portTICK_PERIOD_MS) / 1000ULL
  );

  return (referenceSecondOfDay + elapsedSeconds) % SECONDS_PER_DAY;
}

static void mettreAJourHorlogeComptee()
{
  if (!clockSynchronized) {
    return;
  }

  uint32_t currentSecondOfDay = calculerSecondeLocale();
  if (currentSecondOfDay == lastDisplayedSecondOfDay) {
    return;
  }

  lastDisplayedSecondOfDay = currentSecondOfDay;
  afficherHeureDepuisSecondes(currentSecondOfDay, "Horloge synchronisee GPS");
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

  int heure = (heureUtc[0] - '0') * 10 + (heureUtc[1] - '0');
  int minute = (heureUtc[2] - '0') * 10 + (heureUtc[3] - '0');
  int seconde = (heureUtc[4] - '0') * 10 + (heureUtc[5] - '0');

  heure += GPS_LOCAL_UTC_OFFSET_HOURS;
  if (heure >= 24) {
    heure -= 24;
  }

  const char * virguleApresHeure = std::strchr(heureUtc, ',');
  char statutGps = (virguleApresHeure != nullptr) ? virguleApresHeure[1] : 'V';

  referenceSecondOfDay = static_cast<uint32_t>((heure * 3600) + (minute * 60) + seconde);
  referenceTick = xTaskGetTickCount();
  clockSynchronized = true;
  lastDisplayedSecondOfDay = referenceSecondOfDay;

  afficherHeureDepuisSecondes(referenceSecondOfDay,
                              (statutGps == 'A') ? "Synchronisation GPS valide"
                                                 : "Synchronisation GPS sans fix");

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

    // Fait avancer l'horloge entre deux trames GPS.
    mettreAJourHorlogeComptee();

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
