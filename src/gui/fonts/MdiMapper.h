#pragma once
#include <Arduino.h>
#include "lvgl.h"

LV_FONT_DECLARE(lela_icons);

// --- 1. C++ MACROS (RAW UTF-8 BYTES) ---
#define MY_ICON_AB_TESTING                  "\xf3\xb0\x87\x89"
#define MY_ICON_ABACUS                      "\xf3\xb1\x9b\xa0"
#define MY_ICON_ABJAD_ARABIC                "\xf3\xb1\x8c\xa8"
#define MY_ICON_ABJAD_HEBREW                "\xf3\xb1\x8c\xa9"
#define MY_ICON_ABUGIDA_DEVANAGARI          "\xf3\xb1\x8c\xaa"
#define MY_ICON_ABUGIDA_THAI                "\xf3\xb1\x8c\xab"
#define MY_ICON_ACCESS_POINT_CHECK          "\xf3\xb1\x94\xb8"
#define MY_ICON_ACCESS_POINT_MINUS          "\xf3\xb1\x94\xb9"
#define MY_ICON_ACCESS_POINT_NETWORK_OFF    "\xf3\xb0\xaf\xa1"
#define MY_ICON_ACCESS_POINT_NETWORK        "\xf3\xb0\x80\x82"
#define MY_ICON_ACCESS_POINT_OFF            "\xf3\xb1\x94\x91"
#define MY_ICON_ACCESS_POINT_PLUS           "\xf3\xb1\x94\xba"
#define MY_ICON_ACCESS_POINT_REMOVE         "\xf3\xb1\x94\xbb"
#define MY_ICON_ACCESS_POINT                "\xf3\xb0\x80\x83"
#define MY_ICON_ACCOUNT_ALERT_OUTLINE       "\xf3\xb0\xad\x90"
#define MY_ICON_ACCOUNT_ALERT               "\xf3\xb0\x80\x85"
#define MY_ICON_ACCOUNT_ARROW_DOWN_OUTLINE  "\xf3\xb1\xa1\xa9"
#define MY_ICON_ACCOUNT_ARROW_LEFT_OUTLINE  "\xf3\xb0\xad\x92"
#define MY_ICON_ACCOUNT_ARROW_RIGHT_OUTLINE "\xf3\xb0\xad\x94"
#define MY_ICON_ACCOUNT_ARROW_UP_OUTLINE    "\xf3\xb1\xa1\xaa"
#define MY_ICON_ACCOUNT_ARROW_DOWN          "\xf3\xb1\xa1\xa8"
#define MY_ICON_ACCOUNT_ARROW_LEFT          "\xf3\xb0\xad\x91"
#define MY_ICON_ACCOUNT_ARROW_RIGHT         "\xf3\xb0\xad\x93"
#define MY_ICON_ACCOUNT_ARROW_UP            "\xf3\xb1\xa1\xa7"
#define MY_ICON_ACCOUNT_BADGE_OUTLINE       "\xf3\xb1\xac\x8b"
#define MY_ICON_ACCOUNT_BADGE               "\xf3\xb1\xac\x8a"
#define MY_ICON_ACCOUNT_BOX_EDIT_OUTLINE    "\xf3\xb1\xb3\x88"
#define MY_ICON_ACCOUNT_BOX_MINUS_OUTLINE   "\xf3\xb1\xb3\x89"
#define MY_ICON_ACCOUNT_BOX_MULTIPLE_OUTLINE "\xf3\xb1\x80\x8a"
#define MY_ICON_ACCOUNT_BOX_MULTIPLE        "\xf3\xb0\xa4\xb4"
#define MY_ICON_ACCOUNT_BOX_OUTLINE         "\xf3\xb0\x80\x87"
#define MY_ICON_ACCOUNT_BOX_PLUS_OUTLINE    "\xf3\xb1\xb3\x8a"
#define MY_ICON_ACCOUNT_BOX                 "\xf3\xb0\x80\x86"
#define MY_ICON_ACCOUNT_CANCEL_OUTLINE      "\xf3\xb1\x8b\xa0"
#define MY_ICON_ACCOUNT_CANCEL              "\xf3\xb1\x8b\x9f"
#define MY_ICON_ACCOUNT_CARD_OUTLINE        "\xf3\xb1\xae\xa5"
#define MY_ICON_ACCOUNT_CARD                "\xf3\xb1\xae\xa4"
#define MY_ICON_ACCOUNT_CASH_OUTLINE        "\xf3\xb1\x82\x98"
#define MY_ICON_ACCOUNT_CHECK_OUTLINE       "\xf3\xb0\xaf\xa2"
#define MY_ICON_ACCOUNT_CHECK               "\xf3\xb0\x80\x88"
#define MY_ICON_ACCOUNT_CHILD_CIRCLE        "\xf3\xb0\xaa\x8a"
#define MY_ICON_ACCOUNT_CIRCLE_OUTLINE      "\xf3\xb0\xad\x95"
#define MY_ICON_ACCOUNT_CIRCLE              "\xf3\xb0\x80\x89"
#define MY_ICON_ACCOUNT_COG_OUTLINE         "\xf3\xb1\x8d\xb1"
#define MY_ICON_ACCOUNT_COG                 "\xf3\xb1\x8d\xb0"
#define MY_ICON_ACCOUNT_MINUS_OUTLINE       "\xf3\xb0\xab\xac"
#define MY_ICON_ACCOUNT_MINUS               "\xf3\xb0\x80\x8d"
#define MY_ICON_ACCOUNT_MULTIPLE_CHECK_OUTLINE "\xf3\xb1\x87\xbe"
#define MY_ICON_ACCOUNT_MULTIPLE_CHECK      "\xf3\xb0\xa3\x85"
#define MY_ICON_ACCOUNT_MULTIPLE_MINUS_OUTLINE "\xf3\xb0\xaf\xa5"
#define MY_ICON_ACCOUNT_MUSIC_OUTLINE       "\xf3\xb0\xb3\xa9"
#define MY_ICON_ACCOUNT_MUSIC               "\xf3\xb0\xa0\x83"
#define MY_ICON_ACCOUNT_NETWORK_OFF_OUTLINE "\xf3\xb1\xab\xb2"
#define MY_ICON_ACCOUNT_NETWORK_OFF         "\xf3\xb1\xab\xb1"
#define MY_ICON_ACCOUNT_NETWORK_OUTLINE     "\xf3\xb0\xaf\xa6"
#define MY_ICON_ACCOUNT_NETWORK             "\xf3\xb0\x80\x91"
#define MY_ICON_ACCOUNT_SUPERVISOR_CIRCLE_OUTLINE "\xf3\xb1\x93\xac"
#define MY_ICON_ACCOUNT_SUPERVISOR_CIRCLE   "\xf3\xb0\xaa\x8c"
#define MY_ICON_ADJUST                      "\xf3\xb0\x80\x9a"
#define MY_ICON_ADVERTISEMENTS_OFF          "\xf3\xb1\xa4\xab"
#define MY_ICON_ADVERTISEMENTS              "\xf3\xb1\xa4\xaa"
#define MY_ICON_AIR_CONDITIONER             "\xf3\xb0\x80\x9b"
#define MY_ICON_AIR_FILTER                  "\xf3\xb0\xb5\x83"
#define MY_ICON_AIR_HORN                    "\xf3\xb0\xb6\xac"
#define MY_ICON_AIR_HUMIDIFIER_OFF          "\xf3\xb1\x91\xa6"
#define MY_ICON_AIR_HUMIDIFIER              "\xf3\xb1\x82\x99"
#define MY_ICON_AIR_PURIFIER_OFF            "\xf3\xb1\xad\x97"
#define MY_ICON_AIR_PURIFIER                "\xf3\xb0\xb5\x84"
#define MY_ICON_AIRBAG                      "\xf3\xb0\xaf\xa9"
#define MY_ICON_AIRBALLOON_OUTLINE          "\xf3\xb1\x80\x8b"
#define MY_ICON_AIRBALLOON                  "\xf3\xb0\x80\x9c"
#define MY_ICON_AIRPLANE_CLOCK              "\xf3\xb1\xa1\xbc"
#define MY_ICON_AIRPLANE_EDIT               "\xf3\xb1\xa1\xbe"
#define MY_ICON_AIRPLANE_LANDING            "\xf3\xb0\x97\x94"
#define MY_ICON_AIRPLANE_MARKER             "\xf3\xb1\xa1\xbf"
#define MY_ICON_AIRPLANE_OFF                "\xf3\xb0\x80\x9e"
#define MY_ICON_AIRPLANE_REMOVE             "\xf3\xb1\xa2\x82"
#define MY_ICON_AIRPLANE_SEARCH             "\xf3\xb1\xa2\x83"
#define MY_ICON_AIRPLANE_SETTINGS           "\xf3\xb1\xa2\x84"
#define MY_ICON_AIRPLANE_TAKEOFF            "\xf3\xb0\x97\x95"
#define MY_ICON_AIRPLANE                    "\xf3\xb0\x80\x9d"
#define MY_ICON_ALARM_BELL                  "\xf3\xb0\x9e\x8e"
#define MY_ICON_ALARM_LIGHT_OFF_OUTLINE     "\xf3\xb1\x9c\x9f"
#define MY_ICON_ALARM_LIGHT_OFF             "\xf3\xb1\x9c\x9e"
#define MY_ICON_ALARM_LIGHT_OUTLINE         "\xf3\xb0\xaf\xaa"
#define MY_ICON_ALARM_LIGHT                 "\xf3\xb0\x9e\x8f"
#define MY_ICON_ALARM_PANEL                 "\xf3\xb1\x97\x84"
#define MY_ICON_ALERT                       "\xf3\xb0\x80\xa6"
#define MY_ICON_ALPHA_A_BOX_OUTLINE         "\xf3\xb0\xaf\xab"
#define MY_ICON_ALPHA_B_BOX_OUTLINE         "\xf3\xb0\xaf\xae"
#define MY_ICON_ALPHA_C_BOX_OUTLINE         "\xf3\xb0\xaf\xb1"
#define MY_ICON_ALPHA_D_BOX_OUTLINE         "\xf3\xb0\xaf\xb4"
#define MY_ICON_ALPHA_E_BOX_OUTLINE         "\xf3\xb0\xaf\xb7"
#define MY_ICON_ALPHA_F_BOX_OUTLINE         "\xf3\xb0\xaf\xba"
#define MY_ICON_ALPHA_G_BOX_OUTLINE         "\xf3\xb0\xaf\xbd"
#define MY_ICON_ALPHA_H_BOX_OUTLINE         "\xf3\xb0\xb0\x80"
#define MY_ICON_ALPHA_I_BOX_OUTLINE         "\xf3\xb0\xb0\x83"
#define MY_ICON_ALPHA_J_BOX_OUTLINE         "\xf3\xb0\xb0\x86"
#define MY_ICON_ALPHA_K_BOX_OUTLINE         "\xf3\xb0\xb0\x89"
#define MY_ICON_ALPHA_L_BOX_OUTLINE         "\xf3\xb0\xb0\x8c"
#define MY_ICON_ALPHA_M_BOX_OUTLINE         "\xf3\xb0\xb0\x8f"
#define MY_ICON_ALPHA_N_BOX_OUTLINE         "\xf3\xb0\xb0\x92"
#define MY_ICON_ALPHA_O_BOX_OUTLINE         "\xf3\xb0\xb0\x95"
#define MY_ICON_ALPHA_P_BOX_OUTLINE         "\xf3\xb0\xb0\x98"
#define MY_ICON_ALPHA_Q_BOX_OUTLINE         "\xf3\xb0\xb0\x9b"
#define MY_ICON_ALPHA_R_BOX_OUTLINE         "\xf3\xb0\xb0\x9e"
#define MY_ICON_ALPHA_S_BOX_OUTLINE         "\xf3\xb0\xb0\xa1"
#define MY_ICON_ALPHA_T_BOX_OUTLINE         "\xf3\xb0\xb0\xa4"
#define MY_ICON_ALPHA_U_BOX_OUTLINE         "\xf3\xb0\xb0\xa7"
#define MY_ICON_ALPHA_V_BOX_OUTLINE         "\xf3\xb0\xb0\xaa"
#define MY_ICON_ALPHA_W_BOX_OUTLINE         "\xf3\xb0\xb0\xad"
#define MY_ICON_ALPHA_X_BOX_OUTLINE         "\xf3\xb0\xb0\xb0"
#define MY_ICON_ALPHA_Y_BOX_OUTLINE         "\xf3\xb0\xb0\xb3"
#define MY_ICON_ALPHA_Z_BOX_OUTLINE         "\xf3\xb0\xb0\xb6"
#define MY_ICON_ALPHA_A_BOX                 "\xf3\xb0\xac\x88"
#define MY_ICON_ALPHA_B_BOX                 "\xf3\xb0\xac\x89"
#define MY_ICON_ALPHA_C_BOX                 "\xf3\xb0\xac\x8a"
#define MY_ICON_ALPHA_D_BOX                 "\xf3\xb0\xac\x8b"
#define MY_ICON_ALPHA_E_BOX                 "\xf3\xb0\xac\x8c"
#define MY_ICON_ALPHA_F_BOX                 "\xf3\xb0\xac\x8d"
#define MY_ICON_ALPHA_G_BOX                 "\xf3\xb0\xac\x8e"
#define MY_ICON_ALPHA_H_BOX                 "\xf3\xb0\xac\x8f"
#define MY_ICON_ALPHA_I_BOX                 "\xf3\xb0\xac\x90"
#define MY_ICON_ALPHA_J_BOX                 "\xf3\xb0\xac\x91"
#define MY_ICON_ALPHA_K_BOX                 "\xf3\xb0\xac\x92"
#define MY_ICON_ALPHA_L_BOX                 "\xf3\xb0\xac\x93"
#define MY_ICON_ALPHA_M_BOX                 "\xf3\xb0\xac\x94"
#define MY_ICON_ALPHA_N_BOX                 "\xf3\xb0\xac\x95"
#define MY_ICON_ALPHA_O_BOX                 "\xf3\xb0\xac\x96"
#define MY_ICON_ALPHA_P_BOX                 "\xf3\xb0\xac\x97"
#define MY_ICON_ALPHA_Q_BOX                 "\xf3\xb0\xac\x98"
#define MY_ICON_ALPHA_R_BOX                 "\xf3\xb0\xac\x99"
#define MY_ICON_ALPHA_S_BOX                 "\xf3\xb0\xac\x9a"
#define MY_ICON_ALPHA_T_BOX                 "\xf3\xb0\xac\x9b"
#define MY_ICON_ALPHA_U_BOX                 "\xf3\xb0\xac\x9c"
#define MY_ICON_ALPHA_V_BOX                 "\xf3\xb0\xac\x9d"
#define MY_ICON_ALPHA_W_BOX                 "\xf3\xb0\xac\x9e"
#define MY_ICON_ALPHA_X_BOX                 "\xf3\xb0\xac\x9f"
#define MY_ICON_ALPHA_Y_BOX                 "\xf3\xb0\xac\xa0"
#define MY_ICON_ALPHA_Z_BOX                 "\xf3\xb0\xac\xa1"
#define MY_ICON_ALPHA_A_CIRCLE_OUTLINE      "\xf3\xb0\xaf\xad"
#define MY_ICON_ALPHA_B_CIRCLE_OUTLINE      "\xf3\xb0\xaf\xb0"
#define MY_ICON_ALPHA_C_CIRCLE_OUTLINE      "\xf3\xb0\xaf\xb3"
#define MY_ICON_ALPHA_D_CIRCLE_OUTLINE      "\xf3\xb0\xaf\xb6"
#define MY_ICON_ALPHA_E_CIRCLE_OUTLINE      "\xf3\xb0\xaf\xb9"
#define MY_ICON_ALPHA_F_CIRCLE_OUTLINE      "\xf3\xb0\xaf\xbc"
#define MY_ICON_ALPHA_G_CIRCLE_OUTLINE      "\xf3\xb0\xaf\xbf"
#define MY_ICON_ALPHA_H_CIRCLE_OUTLINE      "\xf3\xb0\xb0\x82"
#define MY_ICON_ALPHA_I_CIRCLE_OUTLINE      "\xf3\xb0\xb0\x85"
#define MY_ICON_ALPHA_J_CIRCLE_OUTLINE      "\xf3\xb0\xb0\x88"
#define MY_ICON_ALPHA_K_CIRCLE_OUTLINE      "\xf3\xb0\xb0\x8b"
#define MY_ICON_ALPHA_L_CIRCLE_OUTLINE      "\xf3\xb0\xb0\x8e"
#define MY_ICON_ALPHA_M_CIRCLE_OUTLINE      "\xf3\xb0\xb0\x91"
#define MY_ICON_ALPHA_N_CIRCLE_OUTLINE      "\xf3\xb0\xb0\x94"
#define MY_ICON_ALPHA_O_CIRCLE_OUTLINE      "\xf3\xb0\xb0\x97"
#define MY_ICON_ALPHA_P_CIRCLE_OUTLINE      "\xf3\xb0\xb0\x9a"
#define MY_ICON_ALPHA_Q_CIRCLE_OUTLINE      "\xf3\xb0\xb0\x9d"
#define MY_ICON_ALPHA_R_CIRCLE_OUTLINE      "\xf3\xb0\xb0\xa0"
#define MY_ICON_ALPHA_S_CIRCLE_OUTLINE      "\xf3\xb0\xb0\xa3"
#define MY_ICON_ALPHA_T_CIRCLE_OUTLINE      "\xf3\xb0\xb0\xa6"
#define MY_ICON_ALPHA_U_CIRCLE_OUTLINE      "\xf3\xb0\xb0\xa9"
#define MY_ICON_ALPHA_V_CIRCLE_OUTLINE      "\xf3\xb0\xb0\xac"
#define MY_ICON_ALPHA_W_CIRCLE_OUTLINE      "\xf3\xb0\xb0\xaf"
#define MY_ICON_ALPHA_X_CIRCLE_OUTLINE      "\xf3\xb0\xb0\xb2"
#define MY_ICON_ALPHA_Y_CIRCLE_OUTLINE      "\xf3\xb0\xb0\xb5"
#define MY_ICON_ALPHA_Z_CIRCLE_OUTLINE      "\xf3\xb0\xb0\xb8"
#define MY_ICON_ALPHA_A_CIRCLE              "\xf3\xb0\xaf\xac"
#define MY_ICON_ALPHA_B_CIRCLE              "\xf3\xb0\xaf\xaf"
#define MY_ICON_ALPHA_C_CIRCLE              "\xf3\xb0\xaf\xb2"
#define MY_ICON_ALPHA_D_CIRCLE              "\xf3\xb0\xaf\xb5"
#define MY_ICON_ALPHA_E_CIRCLE              "\xf3\xb0\xaf\xb8"
#define MY_ICON_ALPHA_F_CIRCLE              "\xf3\xb0\xaf\xbb"
#define MY_ICON_ALPHA_G_CIRCLE              "\xf3\xb0\xaf\xbe"
#define MY_ICON_ALPHA_H_CIRCLE              "\xf3\xb0\xb0\x81"
#define MY_ICON_ALPHA_I_CIRCLE              "\xf3\xb0\xb0\x84"
#define MY_ICON_ALPHA_J_CIRCLE              "\xf3\xb0\xb0\x87"
#define MY_ICON_ALPHA_K_CIRCLE              "\xf3\xb0\xb0\x8a"
#define MY_ICON_ALPHA_L_CIRCLE              "\xf3\xb0\xb0\x8d"
#define MY_ICON_ALPHA_M_CIRCLE              "\xf3\xb0\xb0\x90"
#define MY_ICON_ALPHA_N_CIRCLE              "\xf3\xb0\xb0\x93"
#define MY_ICON_ALPHA_O_CIRCLE              "\xf3\xb0\xb0\x96"
#define MY_ICON_ALPHA_P_CIRCLE              "\xf3\xb0\xb0\x99"
#define MY_ICON_ALPHA_Q_CIRCLE              "\xf3\xb0\xb0\x9c"
#define MY_ICON_ALPHA_R_CIRCLE              "\xf3\xb0\xb0\x9f"
#define MY_ICON_ALPHA_S_CIRCLE              "\xf3\xb0\xb0\xa2"
#define MY_ICON_ALPHA_T_CIRCLE              "\xf3\xb0\xb0\xa5"
#define MY_ICON_ALPHA_U_CIRCLE              "\xf3\xb0\xb0\xa8"
#define MY_ICON_ALPHA_V_CIRCLE              "\xf3\xb0\xb0\xab"
#define MY_ICON_ALPHA_W_CIRCLE              "\xf3\xb0\xb0\xae"
#define MY_ICON_ALPHA_X_CIRCLE              "\xf3\xb0\xb0\xb1"
#define MY_ICON_ALPHA_Y_CIRCLE              "\xf3\xb0\xb0\xb4"
#define MY_ICON_ALPHA_Z_CIRCLE              "\xf3\xb0\xb0\xb7"
#define MY_ICON_ALPHABET_AUREBESH           "\xf3\xb1\x8c\xac"
#define MY_ICON_ALPHABET_CYRILLIC           "\xf3\xb1\x8c\xad"
#define MY_ICON_ALPHABET_GREEK              "\xf3\xb1\x8c\xae"
#define MY_ICON_ALPHABET_LATIN              "\xf3\xb1\x8c\xaf"
#define MY_ICON_ALPHABET_PIQAD              "\xf3\xb1\x8c\xb0"
#define MY_ICON_ALPHABET_TENGWAR            "\xf3\xb1\x8c\xb7"
#define MY_ICON_ALPHABETICAL_OFF            "\xf3\xb1\x80\x8c"
#define MY_ICON_ALPHABETICAL_VARIANT_OFF    "\xf3\xb1\x80\x8e"
#define MY_ICON_ALPHABETICAL_VARIANT        "\xf3\xb1\x80\x8d"
#define MY_ICON_ALPHABETICAL                "\xf3\xb0\x80\xac"
#define MY_ICON_ALPHA                       "\xf3\xb0\x80\xab"
#define MY_ICON_ALPHA_A                     "\xf3\xb0\xab\xae"
#define MY_ICON_ALPHA_B                     "\xf3\xb0\xab\xaf"
#define MY_ICON_ALPHA_C                     "\xf3\xb0\xab\xb0"
#define MY_ICON_ALPHA_D                     "\xf3\xb0\xab\xb1"
#define MY_ICON_ALPHA_E                     "\xf3\xb0\xab\xb2"
#define MY_ICON_ALPHA_F                     "\xf3\xb0\xab\xb3"
#define MY_ICON_ALPHA_G                     "\xf3\xb0\xab\xb4"
#define MY_ICON_ALPHA_H                     "\xf3\xb0\xab\xb5"
#define MY_ICON_ALPHA_I                     "\xf3\xb0\xab\xb6"
#define MY_ICON_ALPHA_J                     "\xf3\xb0\xab\xb7"
#define MY_ICON_ALPHA_K                     "\xf3\xb0\xab\xb8"
#define MY_ICON_ALPHA_L                     "\xf3\xb0\xab\xb9"
#define MY_ICON_ALPHA_M                     "\xf3\xb0\xab\xba"
#define MY_ICON_ALPHA_N                     "\xf3\xb0\xab\xbb"
#define MY_ICON_ALPHA_O                     "\xf3\xb0\xab\xbc"
#define MY_ICON_ALPHA_P                     "\xf3\xb0\xab\xbd"
#define MY_ICON_ALPHA_Q                     "\xf3\xb0\xab\xbe"
#define MY_ICON_ALPHA_R                     "\xf3\xb0\xab\xbf"
#define MY_ICON_ALPHA_S                     "\xf3\xb0\xac\x80"
#define MY_ICON_ALPHA_T                     "\xf3\xb0\xac\x81"
#define MY_ICON_ALPHA_U                     "\xf3\xb0\xac\x82"
#define MY_ICON_ALPHA_V                     "\xf3\xb0\xac\x83"
#define MY_ICON_ALPHA_W                     "\xf3\xb0\xac\x84"
#define MY_ICON_ALPHA_X                     "\xf3\xb0\xac\x85"
#define MY_ICON_ALPHA_Y                     "\xf3\xb0\xac\x86"
#define MY_ICON_ALPHA_Z                     "\xf3\xb0\xac\x87"
#define MY_ICON_AMBULANCE                   "\xf3\xb0\x80\xaf"
#define MY_ICON_ANIMATION_PLAY_OUTLINE      "\xf3\xb0\xaa\x90"
#define MY_ICON_APPLE                       "\xf3\xb0\x80\xb5"
#define MY_ICON_APPLICATION_ARRAY_OUTLINE   "\xf3\xb1\x83\xb6"
#define MY_ICON_APPLICATION_ARRAY           "\xf3\xb1\x83\xb5"
#define MY_ICON_APPLICATION_EDIT_OUTLINE    "\xf3\xb0\x98\x99"
#define MY_ICON_APPLICATION_EDIT            "\xf3\xb0\x82\xae"
#define MY_ICON_APPLICATION_EXPORT          "\xf3\xb0\xb6\xad"
#define MY_ICON_APPLICATION_IMPORT          "\xf3\xb0\xb6\xae"
#define MY_ICON_APPLICATION_OUTLINE         "\xf3\xb0\x98\x94"
#define MY_ICON_APPLICATION_PARENTHESES_OUTLINE "\xf3\xb1\x83\xba"
#define MY_ICON_APPLICATION_PARENTHESES     "\xf3\xb1\x83\xb9"
#define MY_ICON_APPLICATION_SETTINGS_OUTLINE "\xf3\xb1\x95\x95"
#define MY_ICON_APPLICATION_SETTINGS        "\xf3\xb0\xad\xa0"
#define MY_ICON_APPLICATION_VARIABLE_OUTLINE "\xf3\xb1\x83\xbc"
#define MY_ICON_APPLICATION_VARIABLE        "\xf3\xb1\x83\xbb"
#define MY_ICON_APPLICATION                 "\xf3\xb0\xa3\x86"
#define MY_ICON_ARCHIVE_CLOCK_OUTLINE       "\xf3\xb1\x9d\x90"
#define MY_ICON_ARCHIVE_CLOCK               "\xf3\xb1\x9d\x8f"
#define MY_ICON_ARCHIVE_LOCK_OPEN_OUTLINE   "\xf3\xb1\x9d\x99"
#define MY_ICON_ARCHIVE_LOCK_OPEN           "\xf3\xb1\x9d\x98"
#define MY_ICON_ARCHIVE_LOCK_OUTLINE        "\xf3\xb1\x9d\x9a"
#define MY_ICON_ARCHIVE_LOCK                "\xf3\xb1\x9d\x97"
#define MY_ICON_ARROW_BOTTOM_LEFT_BOLD_BOX_OUTLINE "\xf3\xb1\xa5\xa5"
#define MY_ICON_ARROW_BOTTOM_RIGHT_BOLD_BOX_OUTLINE "\xf3\xb1\xa5\xa7"
#define MY_ICON_ARROW_DOWN_BOLD_BOX_OUTLINE "\xf3\xb0\x9c\xb0"
#define MY_ICON_ARROW_LEFT_BOLD_BOX_OUTLINE "\xf3\xb0\x9c\xb3"
#define MY_ICON_ARROW_RIGHT_BOLD_BOX_OUTLINE "\xf3\xb0\x9c\xb6"
#define MY_ICON_ARROW_TOP_LEFT_BOLD_BOX_OUTLINE "\xf3\xb1\xa5\xa9"
#define MY_ICON_ARROW_TOP_RIGHT_BOLD_BOX_OUTLINE "\xf3\xb1\xa5\xab"
#define MY_ICON_ARROW_UP_BOLD_BOX_OUTLINE   "\xf3\xb0\x9c\xb9"
#define MY_ICON_ARROW_BOTTOM_LEFT_BOLD_BOX  "\xf3\xb1\xa5\xa4"
#define MY_ICON_ARROW_BOTTOM_RIGHT_BOLD_BOX "\xf3\xb1\xa5\xa6"
#define MY_ICON_ARROW_DOWN_BOLD_BOX         "\xf3\xb0\x9c\xaf"
#define MY_ICON_ARROW_LEFT_BOLD_BOX         "\xf3\xb0\x9c\xb2"
#define MY_ICON_ARROW_RIGHT_BOLD_BOX        "\xf3\xb0\x9c\xb5"
#define MY_ICON_ARROW_TOP_LEFT_BOLD_BOX     "\xf3\xb1\xa5\xa8"
#define MY_ICON_ARROW_TOP_RIGHT_BOLD_BOX    "\xf3\xb1\xa5\xaa"
#define MY_ICON_ARROW_UP_BOLD_BOX           "\xf3\xb0\x9c\xb8"
#define MY_ICON_ARROW_DOWN_BOLD_CIRCLE_OUTLINE "\xf3\xb0\x81\x88"
#define MY_ICON_ARROW_LEFT_BOLD_CIRCLE_OUTLINE "\xf3\xb0\x81\x90"
#define MY_ICON_ARROW_RIGHT_BOLD_CIRCLE_OUTLINE "\xf3\xb0\x81\x97"
#define MY_ICON_ARROW_UP_BOLD_CIRCLE_OUTLINE "\xf3\xb0\x81\xa0"
#define MY_ICON_ARROW_DOWN_BOLD_CIRCLE      "\xf3\xb0\x81\x87"
#define MY_ICON_ARROW_LEFT_BOLD_CIRCLE      "\xf3\xb0\x81\x8f"
#define MY_ICON_ARROW_RIGHT_BOLD_CIRCLE     "\xf3\xb0\x81\x96"
#define MY_ICON_ARROW_UP_BOLD_CIRCLE        "\xf3\xb0\x81\x9f"
#define MY_ICON_ARROW_DOWN_BOLD_HEXAGON_OUTLINE "\xf3\xb0\x81\x89"
#define MY_ICON_ARROW_LEFT_BOLD_HEXAGON_OUTLINE "\xf3\xb0\x81\x91"
#define MY_ICON_ARROW_RIGHT_BOLD_HEXAGON_OUTLINE "\xf3\xb0\x81\x98"
#define MY_ICON_ARROW_UP_BOLD_HEXAGON_OUTLINE "\xf3\xb0\x81\xa1"
#define MY_ICON_ARROW_BOTTOM_LEFT_BOLD_OUTLINE "\xf3\xb0\xa6\xb7"
#define MY_ICON_ARROW_BOTTOM_RIGHT_BOLD_OUTLINE "\xf3\xb0\xa6\xb9"
#define MY_ICON_ARROW_DOWN_BOLD_OUTLINE     "\xf3\xb0\xa6\xbf"
#define MY_ICON_ARROW_LEFT_BOLD_OUTLINE     "\xf3\xb0\xa7\x80"
#define MY_ICON_ARROW_LEFT_RIGHT_BOLD_OUTLINE "\xf3\xb0\xa7\x81"
#define MY_ICON_ARROW_RIGHT_BOLD_OUTLINE    "\xf3\xb0\xa7\x82"
#define MY_ICON_ARROW_TOP_LEFT_BOLD_OUTLINE "\xf3\xb0\xa7\x83"
#define MY_ICON_ARROW_TOP_RIGHT_BOLD_OUTLINE "\xf3\xb0\xa7\x85"
#define MY_ICON_ARROW_UP_BOLD_OUTLINE       "\xf3\xb0\xa7\x87"
#define MY_ICON_ARROW_UP_DOWN_BOLD_OUTLINE  "\xf3\xb0\xa7\x88"
#define MY_ICON_ARROW_DOWN_BOLD             "\xf3\xb0\x9c\xae"
#define MY_ICON_ARROW_DOWN_LEFT_BOLD        "\xf3\xb1\x9e\xa2"
#define MY_ICON_ARROW_DOWN_RIGHT_BOLD       "\xf3\xb1\x9e\xa4"
#define MY_ICON_ARROW_LEFT_BOLD             "\xf3\xb0\x9c\xb1"
#define MY_ICON_ARROW_LEFT_BOTTOM_BOLD      "\xf3\xb1\x9e\xa6"
#define MY_ICON_ARROW_LEFT_RIGHT_BOLD       "\xf3\xb0\xb9\xb4"
#define MY_ICON_ARROW_LEFT_TOP_BOLD         "\xf3\xb1\x9e\xa8"
#define MY_ICON_ARROW_RIGHT_BOLD            "\xf3\xb0\x9c\xb4"
#define MY_ICON_ARROW_RIGHT_BOTTOM_BOLD     "\xf3\xb1\x9e\xaa"
#define MY_ICON_ARROW_RIGHT_TOP_BOLD        "\xf3\xb1\x9e\xac"
#define MY_ICON_ARROW_TOP_LEFT_BOTTOM_RIGHT_BOLD "\xf3\xb0\xb9\xb6"
#define MY_ICON_ARROW_TOP_RIGHT_BOTTOM_LEFT_BOLD "\xf3\xb0\xb9\xb8"
#define MY_ICON_ARROW_U_DOWN_LEFT_BOLD      "\xf3\xb1\x9e\xae"
#define MY_ICON_ARROW_U_DOWN_RIGHT_BOLD     "\xf3\xb1\x9e\xb0"
#define MY_ICON_ARROW_U_LEFT_BOTTOM_BOLD    "\xf3\xb1\x9e\xb2"
#define MY_ICON_ARROW_U_LEFT_TOP_BOLD       "\xf3\xb1\x9e\xb4"
#define MY_ICON_ARROW_U_RIGHT_BOTTOM_BOLD   "\xf3\xb1\x9e\xb6"
#define MY_ICON_ARROW_U_RIGHT_TOP_BOLD      "\xf3\xb1\x9e\xb8"
#define MY_ICON_ARROW_U_UP_LEFT_BOLD        "\xf3\xb1\x9e\xba"
#define MY_ICON_ARROW_U_UP_RIGHT_BOLD       "\xf3\xb1\x9e\xbc"
#define MY_ICON_ARROW_UP_BOLD               "\xf3\xb0\x9c\xb7"
#define MY_ICON_ARROW_UP_DOWN_BOLD          "\xf3\xb0\xb9\xba"
#define MY_ICON_ARROW_UP_LEFT_BOLD          "\xf3\xb1\x9e\xbe"
#define MY_ICON_ARROW_UP_RIGHT_BOLD         "\xf3\xb1\x9f\x80"
#define MY_ICON_ARROW_DOWN_BOX              "\xf3\xb0\x9b\x80"
#define MY_ICON_ARROW_LEFT_BOX              "\xf3\xb0\x9b\x81"
#define MY_ICON_ARROW_RIGHT_BOX             "\xf3\xb0\x9b\x82"
#define MY_ICON_ARROW_UP_BOX                "\xf3\xb0\x9b\x83"
#define MY_ICON_ARROW_DOWN_CIRCLE_OUTLINE   "\xf3\xb0\xb3\x9c"
#define MY_ICON_ARROW_LEFT_CIRCLE_OUTLINE   "\xf3\xb0\xb3\x9e"
#define MY_ICON_ARROW_RIGHT_CIRCLE_OUTLINE  "\xf3\xb0\xb3\xa0"
#define MY_ICON_ARROW_UP_CIRCLE_OUTLINE     "\xf3\xb0\xb3\xa2"
#define MY_ICON_ARROW_DOWN_CIRCLE           "\xf3\xb0\xb3\x9b"
#define MY_ICON_ARROW_LEFT_CIRCLE           "\xf3\xb0\xb3\x9d"
#define MY_ICON_ARROW_RIGHT_CIRCLE          "\xf3\xb0\xb3\x9f"
#define MY_ICON_ARROW_UP_CIRCLE             "\xf3\xb0\xb3\xa1"
#define MY_ICON_ARROW_DOWN                  "\xf3\xb0\x81\x85"
#define MY_ICON_ARROW_LEFT                  "\xf3\xb0\x81\x8d"
#define MY_ICON_ARROW_RIGHT                 "\xf3\xb0\x81\x94"
#define MY_ICON_ARROW_UP                    "\xf3\xb0\x81\x9d"
#define MY_ICON_AXIS_LOCK                   "\xf3\xb0\xb5\x8b"
#define MY_ICON_AXIS_X_ROTATE_CLOCKWISE     "\xf3\xb0\xb5\x8e"
#define MY_ICON_AXIS_Y_ROTATE_CLOCKWISE     "\xf3\xb0\xb5\x93"
#define MY_ICON_AXIS_Z_ROTATE_CLOCKWISE     "\xf3\xb0\xb5\x97"
#define MY_ICON_AXIS_X_ROTATE_COUNTERCLOCKWISE "\xf3\xb0\xb5\x8f"
#define MY_ICON_AXIS_Y_ROTATE_COUNTERCLOCKWISE "\xf3\xb0\xb5\x94"
#define MY_ICON_AXIS_Z_ROTATE_COUNTERCLOCKWISE "\xf3\xb0\xb5\x98"
#define MY_ICON_BABY_CARRIAGE_OFF           "\xf3\xb0\xbe\xa0"
#define MY_ICON_BABY_CARRIAGE               "\xf3\xb0\x9a\x8f"
#define MY_ICON_BACKSPACE                   "\xf3\xb0\x81\xae"
#define MY_ICON_BAG_CARRY_ON_OFF            "\xf3\xb0\xbc\xbc"
#define MY_ICON_BAG_CARRY_ON                "\xf3\xb0\xbc\xbb"
#define MY_ICON_BALCONY                     "\xf3\xb1\xa0\x97"
#define MY_ICON_BATHTUB_OUTLINE             "\xf3\xb1\xa0\x99"
#define MY_ICON_BATHTUB                     "\xf3\xb1\xa0\x98"
#define MY_ICON_BATTERY_CHARGING            "\xf3\xb0\x82\x84"
#define MY_ICON_BATTERY_CHARGING_10         "\xf3\xb0\xa2\x9c"
#define MY_ICON_BATTERY_CHARGING_20         "\xf3\xb0\x82\x86"
#define MY_ICON_BATTERY_CHARGING_30         "\xf3\xb0\x82\x87"
#define MY_ICON_BATTERY_CHARGING_40         "\xf3\xb0\x82\x88"
#define MY_ICON_BATTERY_CHARGING_50         "\xf3\xb0\xa2\x9d"
#define MY_ICON_BATTERY_CHARGING_60         "\xf3\xb0\x82\x89"
#define MY_ICON_BATTERY_CHARGING_70         "\xf3\xb0\xa2\x9e"
#define MY_ICON_BATTERY_CHARGING_80         "\xf3\xb0\x82\x8a"
#define MY_ICON_BATTERY_CHARGING_90         "\xf3\xb0\x82\x8b"
#define MY_ICON_BATTERY_CHARGING_100        "\xf3\xb0\x82\x85"
#define MY_ICON_BATTERY_OFF                 "\xf3\xb1\x89\x9d"
#define MY_ICON_BATTERY_OUTLINE             "\xf3\xb0\x82\x8e"
#define MY_ICON_BATTERY_REMOVE              "\xf3\xb1\x9f\xa8"
#define MY_ICON_BATTERY                     "\xf3\xb0\x81\xb9"
#define MY_ICON_BATTERY_10                  "\xf3\xb0\x81\xba"
#define MY_ICON_BATTERY_20                  "\xf3\xb0\x81\xbb"
#define MY_ICON_BATTERY_30                  "\xf3\xb0\x81\xbc"
#define MY_ICON_BATTERY_40                  "\xf3\xb0\x81\xbd"
#define MY_ICON_BATTERY_50                  "\xf3\xb0\x81\xbe"
#define MY_ICON_BATTERY_60                  "\xf3\xb0\x81\xbf"
#define MY_ICON_BATTERY_70                  "\xf3\xb0\x82\x80"
#define MY_ICON_BATTERY_80                  "\xf3\xb0\x82\x81"
#define MY_ICON_BATTERY_90                  "\xf3\xb0\x82\x82"
#define MY_ICON_BED_CLOCK                   "\xf3\xb1\xae\x94"
#define MY_ICON_BED_DOUBLE_OUTLINE          "\xf3\xb0\xbf\x93"
#define MY_ICON_BED_DOUBLE                  "\xf3\xb0\xbf\x94"
#define MY_ICON_BED_EMPTY                   "\xf3\xb0\xa2\xa0"
#define MY_ICON_BED_KING_OUTLINE            "\xf3\xb0\xbf\x91"
#define MY_ICON_BED_KING                    "\xf3\xb0\xbf\x92"
#define MY_ICON_BED_OUTLINE                 "\xf3\xb0\x82\x99"
#define MY_ICON_BED_QUEEN_OUTLINE           "\xf3\xb0\xbf\x9b"
#define MY_ICON_BED_QUEEN                   "\xf3\xb0\xbf\x90"
#define MY_ICON_BED_SINGLE_OUTLINE          "\xf3\xb1\x81\xae"
#define MY_ICON_BED_SINGLE                  "\xf3\xb1\x81\xad"
#define MY_ICON_BED                         "\xf3\xb0\x8b\xa3"
#define MY_ICON_BEE_FLOWER                  "\xf3\xb0\xbe\xa2"
#define MY_ICON_BEER                        "\xf3\xb0\x82\x98"
#define MY_ICON_BELL_BADGE_OUTLINE          "\xf3\xb0\x85\xb8"
#define MY_ICON_BELL_BADGE                  "\xf3\xb1\x85\xab"
#define MY_ICON_BELL                        "\xf3\xb0\x82\x9a"
#define MY_ICON_BICYCLE_CARGO               "\xf3\xb1\xa2\x9c"
#define MY_ICON_BICYCLE                     "\xf3\xb1\x82\x9c"
#define MY_ICON_BIKE_FAST                   "\xf3\xb1\x84\x9f"
#define MY_ICON_BIKE_PEDAL_CLIPLESS         "\xf3\xb1\xb0\xa4"
#define MY_ICON_BIKE_PEDAL_MOUNTAIN         "\xf3\xb1\xb0\xa5"
#define MY_ICON_BIKE_PEDAL                  "\xf3\xb1\xb0\xa3"
#define MY_ICON_BIKE                        "\xf3\xb0\x82\xa3"
#define MY_ICON_BIRD                        "\xf3\xb1\x97\x86"
#define MY_ICON_BLENDER_OUTLINE             "\xf3\xb1\xa0\x9a"
#define MY_ICON_BLENDER_SOFTWARE            "\xf3\xb0\x82\xab"
#define MY_ICON_BLENDER                     "\xf3\xb0\xb3\xab"
#define MY_ICON_BLINDS_HORIZONTAL           "\xf3\xb1\xa8\xab"
#define MY_ICON_BLINDS_OPEN                 "\xf3\xb1\x80\x91"
#define MY_ICON_BLINDS_VERTICAL             "\xf3\xb1\xa8\xad"
#define MY_ICON_BLINDS                      "\xf3\xb0\x82\xac"
#define MY_ICON_BLUETOOTH                   "\xf3\xb0\x82\xaf"
#define MY_ICON_BOOK_ALPHABET               "\xf3\xb0\x98\x9d"
#define MY_ICON_BOOK_EDUCATION_OUTLINE      "\xf3\xb1\x9b\x8a"
#define MY_ICON_BOTTLE_WINE                 "\xf3\xb0\xa1\x94"
#define MY_ICON_BULKHEAD_LIGHT              "\xf3\xb1\xa8\xaf"
#define MY_ICON_BUNK_BED_OUTLINE            "\xf3\xb0\x82\x97"
#define MY_ICON_BUNK_BED                    "\xf3\xb1\x8c\x82"
#define MY_ICON_BUS_ARTICULATED_END         "\xf3\xb0\x9e\x9c"
#define MY_ICON_BUS_ARTICULATED_FRONT       "\xf3\xb0\x9e\x9d"
#define MY_ICON_BUS_DOUBLE_DECKER           "\xf3\xb0\x9e\x9e"
#define MY_ICON_BUS_ELECTRIC                "\xf3\xb1\xa4\x9d"
#define MY_ICON_BUS_MARKER                  "\xf3\xb1\x88\x92"
#define MY_ICON_BUS_MULTIPLE                "\xf3\xb0\xbc\xbf"
#define MY_ICON_BUS                         "\xf3\xb0\x83\xa7"
#define MY_ICON_CAMERA                      "\xf3\xb0\x84\x80"
#define MY_ICON_CAR_CONNECTED               "\xf3\xb0\x84\x8d"
#define MY_ICON_CAR_ELECTRIC                "\xf3\xb0\xad\xac"
#define MY_ICON_CAR_LIGHT_DIMMED            "\xf3\xb0\xb1\x8a"
#define MY_ICON_CAR_LIGHT_FOG               "\xf3\xb0\xb1\x8b"
#define MY_ICON_CAR_LIGHT_HIGH              "\xf3\xb0\xb1\x8c"
#define MY_ICON_CAR_PARKING_LIGHTS          "\xf3\xb0\xb5\xa3"
#define MY_ICON_CAR_SPORTS                  "\xf3\xb0\x9e\xac"
#define MY_ICON_CAR                         "\xf3\xb0\x84\x8b"
#define MY_ICON_CAST                        "\xf3\xb0\x84\x98"
#define MY_ICON_CAT                         "\xf3\xb0\x84\x9b"
#define MY_ICON_CCTV                        "\xf3\xb0\x9e\xae"
#define MY_ICON_CEILING_LIGHT_MULTIPLE_OUTLINE "\xf3\xb1\xa3\x9e"
#define MY_ICON_CEILING_LIGHT_MULTIPLE      "\xf3\xb1\xa3\x9d"
#define MY_ICON_CEILING_LIGHT_OUTLINE       "\xf3\xb1\x9f\x87"
#define MY_ICON_CEILING_LIGHT               "\xf3\xb0\x9d\xa9"
#define MY_ICON_CHAIR_ROLLING               "\xf3\xb0\xbd\x88"
#define MY_ICON_CHANDELIER                  "\xf3\xb1\x9e\x93"
#define MY_ICON_CHECK_BOLD                  "\xf3\xb0\xb8\x9e"
#define MY_ICON_CHECK_CIRCLE_OUTLINE        "\xf3\xb0\x97\xa1"
#define MY_ICON_CHECK_CIRCLE                "\xf3\xb0\x97\xa0"
#define MY_ICON_CHECKBOX_BLANK_CIRCLE_OUTLINE "\xf3\xb0\x84\xb0"
#define MY_ICON_CHECKBOX_BLANK_CIRCLE       "\xf3\xb0\x84\xaf"
#define MY_ICON_CHECKBOX_BLANK_OFF_OUTLINE  "\xf3\xb1\x8b\xad"
#define MY_ICON_CHECKBOX_BLANK_OFF          "\xf3\xb1\x8b\xac"
#define MY_ICON_CHECKBOX_BLANK_OUTLINE      "\xf3\xb0\x84\xb1"
#define MY_ICON_CHECKBOX_BLANK              "\xf3\xb0\x84\xae"
#define MY_ICON_CHECKBOX_INTERMEDIATE_VARIANT "\xf3\xb1\xad\x94"
#define MY_ICON_CHECKBOX_INTERMEDIATE       "\xf3\xb0\xa1\x96"
#define MY_ICON_CHECKBOX_MARKED_CIRCLE      "\xf3\xb0\x84\xb3"
#define MY_ICON_CHECKBOX_MARKED_OUTLINE     "\xf3\xb0\x84\xb5"
#define MY_ICON_CHECKBOX_MARKED             "\xf3\xb0\x84\xb2"
#define MY_ICON_CHECK                       "\xf3\xb0\x84\xac"
#define MY_ICON_CHEVRON_DOWN_BOX_OUTLINE    "\xf3\xb0\xa7\x97"
#define MY_ICON_CHEVRON_LEFT_BOX_OUTLINE    "\xf3\xb0\xa7\x99"
#define MY_ICON_CHEVRON_RIGHT_BOX_OUTLINE   "\xf3\xb0\xa7\x9b"
#define MY_ICON_CHEVRON_UP_BOX_OUTLINE      "\xf3\xb0\xa7\x9d"
#define MY_ICON_CHEVRON_DOWN_BOX            "\xf3\xb0\xa7\x96"
#define MY_ICON_CHEVRON_LEFT_BOX            "\xf3\xb0\xa7\x98"
#define MY_ICON_CHEVRON_RIGHT_BOX           "\xf3\xb0\xa7\x9a"
#define MY_ICON_CHEVRON_UP_BOX              "\xf3\xb0\xa7\x9c"
#define MY_ICON_CHEVRON_DOWN_CIRCLE_OUTLINE "\xf3\xb0\xac\xa7"
#define MY_ICON_CHEVRON_LEFT_CIRCLE_OUTLINE "\xf3\xb0\xac\xa9"
#define MY_ICON_CHEVRON_RIGHT_CIRCLE_OUTLINE "\xf3\xb0\xac\xab"
#define MY_ICON_CHEVRON_UP_CIRCLE_OUTLINE   "\xf3\xb0\xac\xad"
#define MY_ICON_CHEVRON_DOWN_CIRCLE         "\xf3\xb0\xac\xa6"
#define MY_ICON_CHEVRON_LEFT_CIRCLE         "\xf3\xb0\xac\xa8"
#define MY_ICON_CHEVRON_RIGHT_CIRCLE        "\xf3\xb0\xac\xaa"
#define MY_ICON_CHEVRON_UP_CIRCLE           "\xf3\xb0\xac\xac"
#define MY_ICON_CHEVRON_DOUBLE_DOWN         "\xf3\xb0\x84\xbc"
#define MY_ICON_CHEVRON_DOUBLE_LEFT         "\xf3\xb0\x84\xbd"
#define MY_ICON_CHEVRON_DOUBLE_RIGHT        "\xf3\xb0\x84\xbe"
#define MY_ICON_CHEVRON_DOUBLE_UP           "\xf3\xb0\x84\xbf"
#define MY_ICON_CHEVRON_TRIPLE_DOWN         "\xf3\xb0\xb6\xb9"
#define MY_ICON_CHEVRON_TRIPLE_LEFT         "\xf3\xb0\xb6\xba"
#define MY_ICON_CHEVRON_TRIPLE_RIGHT        "\xf3\xb0\xb6\xbb"
#define MY_ICON_CHEVRON_TRIPLE_UP           "\xf3\xb0\xb6\xbc"
#define MY_ICON_CHEVRON_DOWN                "\xf3\xb0\x85\x80"
#define MY_ICON_CHEVRON_LEFT                "\xf3\xb0\x85\x81"
#define MY_ICON_CHEVRON_RIGHT               "\xf3\xb0\x85\x82"
#define MY_ICON_CHEVRON_UP                  "\xf3\xb0\x85\x83"
#define MY_ICON_CIRCLE_OUTLINE              "\xf3\xb0\x9d\xa6"
#define MY_ICON_CIRCLE_SLICE_1              "\xf3\xb0\xaa\x9e"
#define MY_ICON_CIRCLE_SLICE_2              "\xf3\xb0\xaa\x9f"
#define MY_ICON_CIRCLE_SLICE_3              "\xf3\xb0\xaa\xa0"
#define MY_ICON_CIRCLE_SLICE_4              "\xf3\xb0\xaa\xa1"
#define MY_ICON_CIRCLE_SLICE_5              "\xf3\xb0\xaa\xa2"
#define MY_ICON_CIRCLE_SLICE_6              "\xf3\xb0\xaa\xa3"
#define MY_ICON_CIRCLE_SLICE_7              "\xf3\xb0\xaa\xa4"
#define MY_ICON_CIRCLE_SLICE_8              "\xf3\xb0\xaa\xa5"
#define MY_ICON_CIRCLE                      "\xf3\xb0\x9d\xa5"
#define MY_ICON_CLOSE_CIRCLE_OUTLINE        "\xf3\xb0\x85\x9a"
#define MY_ICON_CLOSE_CIRCLE                "\xf3\xb0\x85\x99"
#define MY_ICON_CLOSE                       "\xf3\xb0\x85\x96"
#define MY_ICON_COACH_LAMP_VARIANT          "\xf3\xb1\xa8\xb7"
#define MY_ICON_COACH_LAMP                  "\xf3\xb1\x80\xa0"
#define MY_ICON_COFFEE_MAKER_OUTLINE        "\xf3\xb1\xa0\x9b"
#define MY_ICON_COFFEE_MAKER                "\xf3\xb1\x82\x9f"
#define MY_ICON_COFFEE_OFF_OUTLINE          "\xf3\xb0\xbe\xab"
#define MY_ICON_COFFEE_OFF                  "\xf3\xb0\xbe\xaa"
#define MY_ICON_COFFEE_OUTLINE              "\xf3\xb0\x9b\x8a"
#define MY_ICON_COFFEE_TO_GO_OUTLINE        "\xf3\xb1\x8c\x8e"
#define MY_ICON_COFFEE_TO_GO                "\xf3\xb0\x85\xb7"
#define MY_ICON_COFFEE                      "\xf3\xb0\x85\xb6"
#define MY_ICON_COG                         "\xf3\xb0\x92\x93"
#define MY_ICON_COMPASS                     "\xf3\xb0\x86\x8b"
#define MY_ICON_CONTACTLESS_PAYMENT_CIRCLE_OUTLINE "\xf3\xb0\x90\x88"
#define MY_ICON_CONTENT_SAVE                "\xf3\xb0\x86\x93"
#define MY_ICON_COW                         "\xf3\xb0\x86\x9a"
#define MY_ICON_CUP_OFF_OUTLINE             "\xf3\xb1\x8d\xbd"
#define MY_ICON_CUP_OFF                     "\xf3\xb0\x97\xa5"
#define MY_ICON_CUP_OUTLINE                 "\xf3\xb1\x8c\x8f"
#define MY_ICON_CUP_WATER                   "\xf3\xb0\x86\xab"
#define MY_ICON_CUPBOARD_OUTLINE            "\xf3\xb0\xbd\x87"
#define MY_ICON_CUPBOARD                    "\xf3\xb0\xbd\x86"
#define MY_ICON_CUPCAKE                     "\xf3\xb0\xa5\x9a"
#define MY_ICON_CUP                         "\xf3\xb0\x86\xaa"
#define MY_ICON_DESK_LAMP_OFF               "\xf3\xb1\xac\x9f"
#define MY_ICON_DESK_LAMP_ON                "\xf3\xb1\xac\xa0"
#define MY_ICON_DESK_LAMP                   "\xf3\xb0\xa5\x9f"
#define MY_ICON_DISHWASHER_OFF              "\xf3\xb1\x86\xb9"
#define MY_ICON_DISHWASHER                  "\xf3\xb0\xaa\xac"
#define MY_ICON_DOG                         "\xf3\xb0\xa9\x83"
#define MY_ICON_DOME_LIGHT                  "\xf3\xb1\x90\x9e"
#define MY_ICON_DOOR_CLOSED                 "\xf3\xb0\xa0\x9b"
#define MY_ICON_DOOR_OPEN                   "\xf3\xb0\xa0\x9c"
#define MY_ICON_DOORBELL_VIDEO              "\xf3\xb0\xa1\xa9"
#define MY_ICON_DOORBELL                    "\xf3\xb1\x8b\xa6"
#define MY_ICON_DOOR                        "\xf3\xb0\xa0\x9a"
#define MY_ICON_DOTS_HORIZONTAL_CIRCLE_OUTLINE "\xf3\xb0\xae\x8d"
#define MY_ICON_DOTS_HORIZONTAL_CIRCLE      "\xf3\xb0\x9f\x83"
#define MY_ICON_DOWNLOAD_CIRCLE             "\xf3\xb1\x91\xa4"
#define MY_ICON_DOWNLOAD                    "\xf3\xb0\x87\x9a"
#define MY_ICON_EV_STATION                  "\xf3\xb0\x97\xb1"
#define MY_ICON_FACE_MAN_OUTLINE            "\xf3\xb0\xae\x96"
#define MY_ICON_FACE_MAN                    "\xf3\xb0\x99\x83"
#define MY_ICON_FACE_WOMAN_OUTLINE          "\xf3\xb1\x81\xb8"
#define MY_ICON_FACE_WOMAN                  "\xf3\xb1\x81\xb7"
#define MY_ICON_FACEBOOK_MESSENGER          "\xf3\xb0\x88\x8e"
#define MY_ICON_FAN_CHEVRON_DOWN            "\xf3\xb1\x91\xad"
#define MY_ICON_FAN_CHEVRON_UP              "\xf3\xb1\x91\xae"
#define MY_ICON_FAN_SPEED_1                 "\xf3\xb1\x91\xb2"
#define MY_ICON_FAN_SPEED_2                 "\xf3\xb1\x91\xb3"
#define MY_ICON_FAN_SPEED_3                 "\xf3\xb1\x91\xb4"
#define MY_ICON_FAN                         "\xf3\xb0\x88\x90"
#define MY_ICON_FAST_FORWARD                "\xf3\xb0\x88\x91"
#define MY_ICON_FENCE                       "\xf3\xb1\x9e\x9a"
#define MY_ICON_FERRY                       "\xf3\xb0\x88\x93"
#define MY_ICON_FINGERPRINT                 "\xf3\xb0\x88\xb7"
#define MY_ICON_FIRE_ALERT                  "\xf3\xb1\x97\x97"
#define MY_ICON_FIREPLACE                   "\xf3\xb0\xb8\xae"
#define MY_ICON_FIRE                        "\xf3\xb0\x88\xb8"
#define MY_ICON_FISH                        "\xf3\xb0\x88\xba"
#define MY_ICON_FLASH                       "\xf3\xb0\x89\x81"
#define MY_ICON_FLOOR_LAMP_DUAL_OUTLINE     "\xf3\xb1\x9f\x8e"
#define MY_ICON_FLOOR_LAMP_DUAL             "\xf3\xb1\x81\x80"
#define MY_ICON_FLOOR_LAMP_OUTLINE          "\xf3\xb1\x9f\x88"
#define MY_ICON_FLOOR_LAMP_TORCHIERE_OUTLINE "\xf3\xb1\x9f\x96"
#define MY_ICON_FLOOR_LAMP_TORCHIERE_VARIANT_OUTLINE "\xf3\xb1\x9f\x8f"
#define MY_ICON_FLOOR_LAMP_TORCHIERE_VARIANT "\xf3\xb1\x81\x81"
#define MY_ICON_FLOOR_LAMP_TORCHIERE        "\xf3\xb1\x9d\x87"
#define MY_ICON_FLOOR_LAMP                  "\xf3\xb0\xa3\x9d"
#define MY_ICON_FLOWER_TULIP                "\xf3\xb0\xa7\xb1"
#define MY_ICON_FLOWER                      "\xf3\xb0\x89\x8a"
#define MY_ICON_FOOD_APPLE                  "\xf3\xb0\x89\x9b"
#define MY_ICON_FOOD_VARIANT                "\xf3\xb0\x89\x9c"
#define MY_ICON_FRIDGE_INDUSTRIAL_OFF_OUTLINE "\xf3\xb1\x97\xb2"
#define MY_ICON_FRIDGE_INDUSTRIAL_OFF       "\xf3\xb1\x97\xb1"
#define MY_ICON_FRIDGE_INDUSTRIAL_OUTLINE   "\xf3\xb1\x97\xb3"
#define MY_ICON_FRIDGE_OUTLINE              "\xf3\xb0\x8a\x8f"
#define MY_ICON_FRIDGE                      "\xf3\xb0\x8a\x90"
#define MY_ICON_GAMEPAD                     "\xf3\xb0\x8a\x96"
#define MY_ICON_GARAGE                      "\xf3\xb0\x9b\x99"
#define MY_ICON_GAS_STATION                 "\xf3\xb0\x8a\x98"
#define MY_ICON_GAUGE                       "\xf3\xb0\x8a\x9a"
#define MY_ICON_GLASS_WINE                  "\xf3\xb0\xa1\xb6"
#define MY_ICON_GRILL                       "\xf3\xb0\xb9\x85"
#define MY_ICON_HAMMER                      "\xf3\xb0\xa3\xaa"
#define MY_ICON_HEADPHONES                  "\xf3\xb0\x8b\x8b"
#define MY_ICON_HOME_ASSISTANT              "\xf3\xb0\x9f\x90"
#define MY_ICON_HOME                        "\xf3\xb0\x8b\x9c"
#define MY_ICON_HORSE                       "\xf3\xb1\x96\xbf"
#define MY_ICON_HVAC_OFF                    "\xf3\xb1\x96\x9e"
#define MY_ICON_HVAC                        "\xf3\xb1\x8d\x92"
#define MY_ICON_IMAGE                       "\xf3\xb0\x8b\xa9"
#define MY_ICON_INCOGNITO_CIRCLE_OFF        "\xf3\xb1\x90\xa2"
#define MY_ICON_INCOGNITO_CIRCLE            "\xf3\xb1\x90\xa1"
#define MY_ICON_INFORMATION_SLAB_CIRCLE     "\xf3\xb1\xb1\xa9"
#define MY_ICON_INFORMATION_VARIANT_CIRCLE_OUTLINE "\xf3\xb1\xb1\xb0"
#define MY_ICON_INFORMATION                 "\xf3\xb0\x8b\xbc"
#define MY_ICON_IRON                        "\xf3\xb1\xa0\xa4"
#define MY_ICON_KETTLE                      "\xf3\xb0\x97\xba"
#define MY_ICON_KEY_VARIANT                 "\xf3\xb0\x8c\x8b"
#define MY_ICON_KEYBOARD_RETURN             "\xf3\xb0\x8c\x91"
#define MY_ICON_KEY                         "\xf3\xb0\x8c\x86"
#define MY_ICON_LAMP_OUTLINE                "\xf3\xb1\x9f\x90"
#define MY_ICON_LAMPS_OUTLINE               "\xf3\xb1\x9f\x91"
#define MY_ICON_LAMPS                       "\xf3\xb1\x95\xb6"
#define MY_ICON_LAMP                        "\xf3\xb0\x9a\xb5"
#define MY_ICON_LAN                         "\xf3\xb0\x8c\x97"
#define MY_ICON_LAVA_LAMP                   "\xf3\xb0\x9f\x95"
#define MY_ICON_LEAF                        "\xf3\xb0\x8c\xaa"
#define MY_ICON_LEAK                        "\xf3\xb0\xb7\x97"
#define MY_ICON_LED_OFF                     "\xf3\xb0\x8c\xab"
#define MY_ICON_LED_ON                      "\xf3\xb0\x8c\xac"
#define MY_ICON_LED_OUTLINE                 "\xf3\xb0\x8c\xad"
#define MY_ICON_LED_STRIP_VARIANT_OFF       "\xf3\xb1\xa9\x8b"
#define MY_ICON_LED_STRIP_VARIANT           "\xf3\xb1\x81\x91"
#define MY_ICON_LED_STRIP                   "\xf3\xb0\x9f\x96"
#define MY_ICON_LED_VARIANT_OFF             "\xf3\xb0\x8c\xae"
#define MY_ICON_LED_VARIANT_ON              "\xf3\xb0\x8c\xaf"
#define MY_ICON_LED_VARIANT_OUTLINE         "\xf3\xb0\x8c\xb0"
#define MY_ICON_LIGHTBULB_ON                "\xf3\xb0\x9b\xa8"
#define MY_ICON_LIGHTBULB_OUTLINE           "\xf3\xb0\x8c\xb6"
#define MY_ICON_LIGHTBULB_QUESTION_OUTLINE  "\xf3\xb1\xa7\xa4"
#define MY_ICON_LIGHTBULB_QUESTION          "\xf3\xb1\xa7\xa3"
#define MY_ICON_LIGHTBULB                   "\xf3\xb0\x8c\xb5"
#define MY_ICON_LIGHTNING_BOLT              "\xf3\xb1\x90\x8b"
#define MY_ICON_LOCK_OPEN                   "\xf3\xb0\x8c\xbf"
#define MY_ICON_LOCK                        "\xf3\xb0\x8c\xbe"
#define MY_ICON_MAILBOX                     "\xf3\xb0\x9b\xae"
#define MY_ICON_MAP_MARKER                  "\xf3\xb0\x8d\x8e"
#define MY_ICON_MAP                         "\xf3\xb0\x8d\x8d"
#define MY_ICON_MENU_CLOSE                  "\xf3\xb1\xb2\x90"
#define MY_ICON_MENU_OPEN                   "\xf3\xb0\xae\xab"
#define MY_ICON_MENU_DOWN_OUTLINE           "\xf3\xb0\x9a\xb6"
#define MY_ICON_MENU_LEFT_OUTLINE           "\xf3\xb0\xa8\x82"
#define MY_ICON_MENU_RIGHT_OUTLINE          "\xf3\xb0\xa8\x83"
#define MY_ICON_MENU_UP_OUTLINE             "\xf3\xb0\x9a\xb7"
#define MY_ICON_MENU_SWAP_OUTLINE           "\xf3\xb0\xa9\xa5"
#define MY_ICON_MENU_SWAP                   "\xf3\xb0\xa9\xa4"
#define MY_ICON_MENU                        "\xf3\xb0\x8d\x9c"
#define MY_ICON_MENU_DOWN                   "\xf3\xb0\x8d\x9d"
#define MY_ICON_MENU_LEFT                   "\xf3\xb0\x8d\x9e"
#define MY_ICON_MENU_RIGHT                  "\xf3\xb0\x8d\x9f"
#define MY_ICON_MENU_UP                     "\xf3\xb0\x8d\xa0"
#define MY_ICON_MESSAGE_ALERT_OUTLINE       "\xf3\xb0\xa8\x84"
#define MY_ICON_MESSAGE_ALERT               "\xf3\xb0\x8d\xa2"
#define MY_ICON_MESSAGE_CHECK_OUTLINE       "\xf3\xb1\xae\x8b"
#define MY_ICON_MESSAGE_CHECK               "\xf3\xb1\xae\x8a"
#define MY_ICON_MESSAGE_COG_OUTLINE         "\xf3\xb1\x85\xb2"
#define MY_ICON_MESSAGE_COG                 "\xf3\xb0\x9b\xb1"
#define MY_ICON_MICROPHONE                  "\xf3\xb0\x8d\xac"
#define MY_ICON_MICROWAVE                   "\xf3\xb0\xb2\x99"
#define MY_ICON_MINUS_THICK                 "\xf3\xb1\x98\xb9"
#define MY_ICON_MINUS                       "\xf3\xb0\x8d\xb4"
#define MY_ICON_MIRROR                      "\xf3\xb1\x87\xbd"
#define MY_ICON_MOLECULE_CO2                "\xf3\xb0\x9f\xa4"
#define MY_ICON_MOTION_SENSOR               "\xf3\xb0\xb6\x91"
#define MY_ICON_MOTORBIKE                   "\xf3\xb0\x8d\xbc"
#define MY_ICON_MOVIE                       "\xf3\xb0\x8e\x81"
#define MY_ICON_MOWER                       "\xf3\xb1\x99\xaf"
#define MY_ICON_MUSHROOM                    "\xf3\xb0\x9f\x9f"
#define MY_ICON_MUSIC                       "\xf3\xb0\x9d\x9a"
#define MY_ICON_NAS                         "\xf3\xb0\xa3\xb3"
#define MY_ICON_NAVIGATION                  "\xf3\xb0\x8e\x90"
#define MY_ICON_NETFLIX                     "\xf3\xb0\x9d\x86"
#define MY_ICON_NETWORK_STRENGTH_OUTLINE    "\xf3\xb0\xa3\xbe"
#define MY_ICON_NETWORK_STRENGTH_1          "\xf3\xb0\xa3\xb4"
#define MY_ICON_NETWORK_STRENGTH_2          "\xf3\xb0\xa3\xb6"
#define MY_ICON_NETWORK_STRENGTH_3          "\xf3\xb0\xa3\xb8"
#define MY_ICON_NETWORK_STRENGTH_4          "\xf3\xb0\xa3\xba"
#define MY_ICON_NETWORK                     "\xf3\xb0\x9b\xb3"
#define MY_ICON_NUMERIC_0_BOX_MULTIPLE_OUTLINE "\xf3\xb0\x8e\xa2"
#define MY_ICON_NUMERIC_1_BOX_MULTIPLE_OUTLINE "\xf3\xb0\x8e\xa5"
#define MY_ICON_NUMERIC_2_BOX_MULTIPLE_OUTLINE "\xf3\xb0\x8e\xa8"
#define MY_ICON_NUMERIC_3_BOX_MULTIPLE_OUTLINE "\xf3\xb0\x8e\xab"
#define MY_ICON_NUMERIC_4_BOX_MULTIPLE_OUTLINE "\xf3\xb0\x8e\xb2"
#define MY_ICON_NUMERIC_5_BOX_MULTIPLE_OUTLINE "\xf3\xb0\x8e\xaf"
#define MY_ICON_NUMERIC_6_BOX_MULTIPLE_OUTLINE "\xf3\xb0\x8e\xb4"
#define MY_ICON_NUMERIC_7_BOX_MULTIPLE_OUTLINE "\xf3\xb0\x8e\xb7"
#define MY_ICON_NUMERIC_8_BOX_MULTIPLE_OUTLINE "\xf3\xb0\x8e\xba"
#define MY_ICON_NUMERIC_9_BOX_MULTIPLE_OUTLINE "\xf3\xb0\x8e\xbd"
#define MY_ICON_NUMERIC_10_BOX_MULTIPLE_OUTLINE "\xf3\xb0\xbf\xab"
#define MY_ICON_NUMERIC_0_BOX_MULTIPLE      "\xf3\xb0\xbc\x8e"
#define MY_ICON_NUMERIC_1_BOX_MULTIPLE      "\xf3\xb0\xbc\x8f"
#define MY_ICON_NUMERIC_2_BOX_MULTIPLE      "\xf3\xb0\xbc\x90"
#define MY_ICON_NUMERIC_3_BOX_MULTIPLE      "\xf3\xb0\xbc\x91"
#define MY_ICON_NUMERIC_4_BOX_MULTIPLE      "\xf3\xb0\xbc\x92"
#define MY_ICON_NUMERIC_5_BOX_MULTIPLE      "\xf3\xb0\xbc\x93"
#define MY_ICON_NUMERIC_6_BOX_MULTIPLE      "\xf3\xb0\xbc\x94"
#define MY_ICON_NUMERIC_7_BOX_MULTIPLE      "\xf3\xb0\xbc\x95"
#define MY_ICON_NUMERIC_8_BOX_MULTIPLE      "\xf3\xb0\xbc\x96"
#define MY_ICON_NUMERIC_9_BOX_MULTIPLE      "\xf3\xb0\xbc\x97"
#define MY_ICON_NUMERIC_10_BOX_MULTIPLE     "\xf3\xb0\xbf\xaa"
#define MY_ICON_NUMERIC_0_BOX_OUTLINE       "\xf3\xb0\x8e\xa3"
#define MY_ICON_NUMERIC_1_BOX_OUTLINE       "\xf3\xb0\x8e\xa6"
#define MY_ICON_NUMERIC_2_BOX_OUTLINE       "\xf3\xb0\x8e\xa9"
#define MY_ICON_NUMERIC_3_BOX_OUTLINE       "\xf3\xb0\x8e\xac"
#define MY_ICON_NUMERIC_4_BOX_OUTLINE       "\xf3\xb0\x8e\xae"
#define MY_ICON_NUMERIC_5_BOX_OUTLINE       "\xf3\xb0\x8e\xb0"
#define MY_ICON_NUMERIC_6_BOX_OUTLINE       "\xf3\xb0\x8e\xb5"
#define MY_ICON_NUMERIC_7_BOX_OUTLINE       "\xf3\xb0\x8e\xb8"
#define MY_ICON_NUMERIC_8_BOX_OUTLINE       "\xf3\xb0\x8e\xbb"
#define MY_ICON_NUMERIC_9_BOX_OUTLINE       "\xf3\xb0\x8e\xbe"
#define MY_ICON_NUMERIC_10_BOX_OUTLINE      "\xf3\xb0\xbd\xbe"
#define MY_ICON_NUMERIC_0_BOX               "\xf3\xb0\x8e\xa1"
#define MY_ICON_NUMERIC_1_BOX               "\xf3\xb0\x8e\xa4"
#define MY_ICON_NUMERIC_2_BOX               "\xf3\xb0\x8e\xa7"
#define MY_ICON_NUMERIC_3_BOX               "\xf3\xb0\x8e\xaa"
#define MY_ICON_NUMERIC_4_BOX               "\xf3\xb0\x8e\xad"
#define MY_ICON_NUMERIC_5_BOX               "\xf3\xb0\x8e\xb1"
#define MY_ICON_NUMERIC_6_BOX               "\xf3\xb0\x8e\xb3"
#define MY_ICON_NUMERIC_7_BOX               "\xf3\xb0\x8e\xb6"
#define MY_ICON_NUMERIC_8_BOX               "\xf3\xb0\x8e\xb9"
#define MY_ICON_NUMERIC_9_BOX               "\xf3\xb0\x8e\xbc"
#define MY_ICON_NUMERIC_10_BOX              "\xf3\xb0\xbd\xbd"
#define MY_ICON_NUMERIC_0_CIRCLE_OUTLINE    "\xf3\xb0\xb2\x9f"
#define MY_ICON_NUMERIC_1_CIRCLE_OUTLINE    "\xf3\xb0\xb2\xa1"
#define MY_ICON_NUMERIC_2_CIRCLE_OUTLINE    "\xf3\xb0\xb2\xa3"
#define MY_ICON_NUMERIC_3_CIRCLE_OUTLINE    "\xf3\xb0\xb2\xa5"
#define MY_ICON_NUMERIC_4_CIRCLE_OUTLINE    "\xf3\xb0\xb2\xa7"
#define MY_ICON_NUMERIC_5_CIRCLE_OUTLINE    "\xf3\xb0\xb2\xa9"
#define MY_ICON_NUMERIC_6_CIRCLE_OUTLINE    "\xf3\xb0\xb2\xab"
#define MY_ICON_NUMERIC_7_CIRCLE_OUTLINE    "\xf3\xb0\xb2\xad"
#define MY_ICON_NUMERIC_8_CIRCLE_OUTLINE    "\xf3\xb0\xb2\xaf"
#define MY_ICON_NUMERIC_9_CIRCLE_OUTLINE    "\xf3\xb0\xb2\xb1"
#define MY_ICON_NUMERIC_10_CIRCLE_OUTLINE   "\xf3\xb0\xbf\xad"
#define MY_ICON_NUMERIC_0_CIRCLE            "\xf3\xb0\xb2\x9e"
#define MY_ICON_NUMERIC_1_CIRCLE            "\xf3\xb0\xb2\xa0"
#define MY_ICON_NUMERIC_2_CIRCLE            "\xf3\xb0\xb2\xa2"
#define MY_ICON_NUMERIC_3_CIRCLE            "\xf3\xb0\xb2\xa4"
#define MY_ICON_NUMERIC_4_CIRCLE            "\xf3\xb0\xb2\xa6"
#define MY_ICON_NUMERIC_5_CIRCLE            "\xf3\xb0\xb2\xa8"
#define MY_ICON_NUMERIC_6_CIRCLE            "\xf3\xb0\xb2\xaa"
#define MY_ICON_NUMERIC_7_CIRCLE            "\xf3\xb0\xb2\xac"
#define MY_ICON_NUMERIC_8_CIRCLE            "\xf3\xb0\xb2\xae"
#define MY_ICON_NUMERIC_9_CIRCLE            "\xf3\xb0\xb2\xb0"
#define MY_ICON_NUMERIC_10_CIRCLE           "\xf3\xb0\xbf\xac"
#define MY_ICON_NUMERIC_NEGATIVE_1          "\xf3\xb1\x81\x92"
#define MY_ICON_NUMERIC_OFF                 "\xf3\xb1\xa7\x93"
#define MY_ICON_NUMERIC_9_PLUS_BOX_MULTIPLE_OUTLINE "\xf3\xb0\x8f\x80"
#define MY_ICON_NUMERIC_9_PLUS_BOX_MULTIPLE "\xf3\xb0\xbc\x98"
#define MY_ICON_NUMERIC_9_PLUS_BOX_OUTLINE  "\xf3\xb0\x8f\x81"
#define MY_ICON_NUMERIC_9_PLUS_BOX          "\xf3\xb0\x8e\xbf"
#define MY_ICON_NUMERIC_9_PLUS_CIRCLE_OUTLINE "\xf3\xb0\xb2\xb3"
#define MY_ICON_NUMERIC_9_PLUS_CIRCLE       "\xf3\xb0\xb2\xb2"
#define MY_ICON_NUMERIC_9_PLUS              "\xf3\xb0\xbf\xae"
#define MY_ICON_NUMERIC_POSITIVE_1          "\xf3\xb1\x97\x8b"
#define MY_ICON_NUMERIC                     "\xf3\xb0\x8e\xa0"
#define MY_ICON_NUMERIC_0                   "\xf3\xb0\xac\xb9"
#define MY_ICON_NUMERIC_1                   "\xf3\xb0\xac\xba"
#define MY_ICON_NUMERIC_2                   "\xf3\xb0\xac\xbb"
#define MY_ICON_NUMERIC_3                   "\xf3\xb0\xac\xbc"
#define MY_ICON_NUMERIC_4                   "\xf3\xb0\xac\xbd"
#define MY_ICON_NUMERIC_5                   "\xf3\xb0\xac\xbe"
#define MY_ICON_NUMERIC_6                   "\xf3\xb0\xac\xbf"
#define MY_ICON_NUMERIC_7                   "\xf3\xb0\xad\x80"
#define MY_ICON_NUMERIC_8                   "\xf3\xb0\xad\x81"
#define MY_ICON_NUMERIC_9                   "\xf3\xb0\xad\x82"
#define MY_ICON_NUMERIC_10                  "\xf3\xb0\xbf\xa9"
#define MY_ICON_ORDER_ALPHABETICAL_ASCENDING "\xf3\xb0\x88\x8d"
#define MY_ICON_ORDER_ALPHABETICAL_DESCENDING "\xf3\xb0\xb4\x87"
#define MY_ICON_ORDER_NUMERIC_ASCENDING     "\xf3\xb0\x95\x85"
#define MY_ICON_ORDER_NUMERIC_DESCENDING    "\xf3\xb0\x95\x86"
#define MY_ICON_PACKAGE                     "\xf3\xb0\x8f\x93"
#define MY_ICON_PARKING                     "\xf3\xb0\x8f\xa3"
#define MY_ICON_PAUSE_CIRCLE_OUTLINE        "\xf3\xb0\x8f\xa6"
#define MY_ICON_PAUSE_CIRCLE                "\xf3\xb0\x8f\xa5"
#define MY_ICON_PAUSE                       "\xf3\xb0\x8f\xa4"
#define MY_ICON_PENCIL_CIRCLE_OUTLINE       "\xf3\xb0\x9d\xb6"
#define MY_ICON_PENCIL_CIRCLE               "\xf3\xb0\x9b\xbf"
#define MY_ICON_PENCIL                      "\xf3\xb0\x8f\xab"
#define MY_ICON_PIG                         "\xf3\xb0\x90\x81"
#define MY_ICON_PINE_TREE                   "\xf3\xb0\x90\x85"
#define MY_ICON_PLAY                        "\xf3\xb0\x90\x8a"
#define MY_ICON_PLUS_THICK                  "\xf3\xb1\x87\xac"
#define MY_ICON_PLUS                        "\xf3\xb0\x90\x95"
#define MY_ICON_POOL                        "\xf3\xb0\x98\x86"
#define MY_ICON_POST_LAMP                   "\xf3\xb1\xa9\xa0"
#define MY_ICON_POWER                       "\xf3\xb0\x90\xa5"
#define MY_ICON_RABBIT                      "\xf3\xb0\xa4\x87"
#define MY_ICON_RADIATOR                    "\xf3\xb0\x90\xb8"
#define MY_ICON_REFRESH                     "\xf3\xb0\x91\x90"
#define MY_ICON_REMOTE                      "\xf3\xb0\x91\x94"
#define MY_ICON_REPEAT                      "\xf3\xb0\x91\x96"
#define MY_ICON_REWIND                      "\xf3\xb0\x91\x9f"
#define MY_ICON_ROAD                        "\xf3\xb0\x91\xa1"
#define MY_ICON_ROBOT_VACUUM                "\xf3\xb0\x9c\x8d"
#define MY_ICON_ROLLER_SHADE                "\xf3\xb1\xa9\xab"
#define MY_ICON_ROUTER_WIRELESS             "\xf3\xb0\x91\xa9"
#define MY_ICON_SCOOTER                     "\xf3\xb1\x96\xbd"
#define MY_ICON_SCREWDRIVER                 "\xf3\xb0\x91\xb6"
#define MY_ICON_SECURITY                    "\xf3\xb0\x92\x83"
#define MY_ICON_SERVER                      "\xf3\xb0\x92\x8b"
#define MY_ICON_SHIELD_CHECK                "\xf3\xb0\x95\xa5"
#define MY_ICON_SHIELD                      "\xf3\xb0\x92\x98"
#define MY_ICON_SHOWER                      "\xf3\xb0\xa6\xa0"
#define MY_ICON_SHUFFLE                     "\xf3\xb0\x92\x9d"
#define MY_ICON_SIGN_DIRECTION              "\xf3\xb0\x9e\x81"
#define MY_ICON_SILVERWARE                  "\xf3\xb0\x92\xa3"
#define MY_ICON_SMOKE_DETECTOR              "\xf3\xb0\x8e\x92"
#define MY_ICON_SNOWFLAKE                   "\xf3\xb0\x9c\x97"
#define MY_ICON_SOFA_OUTLINE                "\xf3\xb1\x95\xad"
#define MY_ICON_SOFA                        "\xf3\xb0\x92\xb9"
#define MY_ICON_SOLAR_PANEL                 "\xf3\xb0\xb6\x9b"
#define MY_ICON_SOLAR_POWER                 "\xf3\xb0\xa9\xb2"
#define MY_ICON_SORT_ALPHABETICAL_ASCENDING_VARIANT "\xf3\xb1\x85\x88"
#define MY_ICON_SORT_ALPHABETICAL_ASCENDING "\xf3\xb0\x96\xbd"
#define MY_ICON_SORT_ALPHABETICAL_DESCENDING_VARIANT "\xf3\xb1\x85\x89"
#define MY_ICON_SORT_ALPHABETICAL_DESCENDING "\xf3\xb0\x96\xbf"
#define MY_ICON_SORT_ALPHABETICAL_VARIANT   "\xf3\xb0\x92\xbb"
#define MY_ICON_SORT_NUMERIC_ASCENDING_VARIANT "\xf3\xb0\xa4\x8d"
#define MY_ICON_SORT_NUMERIC_ASCENDING      "\xf3\xb1\x8e\x89"
#define MY_ICON_SORT_NUMERIC_DESCENDING_VARIANT "\xf3\xb0\xab\x92"
#define MY_ICON_SORT_NUMERIC_DESCENDING     "\xf3\xb1\x8e\x8a"
#define MY_ICON_SORT_NUMERIC_VARIANT        "\xf3\xb0\x92\xbe"
#define MY_ICON_SPEAKER                     "\xf3\xb0\x93\x83"
#define MY_ICON_SPOTIFY                     "\xf3\xb0\x93\x87"
#define MY_ICON_SPOTLIGHT                   "\xf3\xb0\x93\x88"
#define MY_ICON_STAIRS                      "\xf3\xb0\x93\x8d"
#define MY_ICON_STOP                        "\xf3\xb0\x93\x9b"
#define MY_ICON_STOVE                       "\xf3\xb0\x93\x9e"
#define MY_ICON_TABLE_FURNITURE             "\xf3\xb0\x96\xbc"
#define MY_ICON_TABLE                       "\xf3\xb0\x93\xab"
#define MY_ICON_TELEVISION_CLASSIC          "\xf3\xb0\x9f\xb4"
#define MY_ICON_TELEVISION                  "\xf3\xb0\x94\x82"
#define MY_ICON_THERMOMETER_CHEVRON_DOWN    "\xf3\xb0\xb8\x82"
#define MY_ICON_THERMOMETER_CHEVRON_UP      "\xf3\xb0\xb8\x83"
#define MY_ICON_THERMOMETER_LINES           "\xf3\xb0\x94\x90"
#define MY_ICON_THERMOMETER                 "\xf3\xb0\x94\x8f"
#define MY_ICON_THERMOSTAT                  "\xf3\xb0\x8e\x93"
#define MY_ICON_TOASTER                     "\xf3\xb1\x81\xa3"
#define MY_ICON_TOGGLE_SWITCH_OFF_OUTLINE   "\xf3\xb0\xa8\x99"
#define MY_ICON_TOGGLE_SWITCH_OFF           "\xf3\xb0\x94\xa2"
#define MY_ICON_TOGGLE_SWITCH_OUTLINE       "\xf3\xb0\xa8\x9a"
#define MY_ICON_TOGGLE_SWITCH_VARIANT_OFF   "\xf3\xb1\xa8\xa6"
#define MY_ICON_TOGGLE_SWITCH_VARIANT       "\xf3\xb1\xa8\xa5"
#define MY_ICON_TOGGLE_SWITCH               "\xf3\xb0\x94\xa1"
#define MY_ICON_TOILET                      "\xf3\xb0\xa6\xab"
#define MY_ICON_TOOLBOX                     "\xf3\xb0\xa6\xac"
#define MY_ICON_TRAIN                       "\xf3\xb0\x94\xac"
#define MY_ICON_TRANSMISSION_TOWER          "\xf3\xb0\xb4\xbe"
#define MY_ICON_TRASH_CAN                   "\xf3\xb0\xa9\xb9"
#define MY_ICON_TREE                        "\xf3\xb0\x94\xb1"
#define MY_ICON_TRUCK                       "\xf3\xb0\x94\xbd"
#define MY_ICON_TUMBLE_DRYER                "\xf3\xb0\xa4\x97"
#define MY_ICON_UMBRELLA_OUTLINE            "\xf3\xb0\x95\x8b"
#define MY_ICON_UMBRELLA                    "\xf3\xb0\x95\x8a"
#define MY_ICON_UNDO                        "\xf3\xb0\x95\x8c"
#define MY_ICON_UPLOAD                      "\xf3\xb0\x95\x92"
#define MY_ICON_VACUUM                      "\xf3\xb1\xa6\xa1"
#define MY_ICON_VANITY_LIGHT                "\xf3\xb1\x87\xa1"
#define MY_ICON_VIDEO_BOX                   "\xf3\xb0\x83\xbd"
#define MY_ICON_VIDEO                       "\xf3\xb0\x95\xa7"
#define MY_ICON_VOLUME_HIGH                 "\xf3\xb0\x95\xbe"
#define MY_ICON_VOLUME_MEDIUM               "\xf3\xb0\x96\x80"
#define MY_ICON_VOLUME_MINUS                "\xf3\xb0\x9d\x9e"
#define MY_ICON_VOLUME_OFF                  "\xf3\xb0\x96\x81"
#define MY_ICON_VOLUME_PLUS                 "\xf3\xb0\x9d\x9d"
#define MY_ICON_WALL_SCONCE                 "\xf3\xb0\xa4\x9c"
#define MY_ICON_WARDROBE                    "\xf3\xb0\xbe\x90"
#define MY_ICON_WASHING_MACHINE             "\xf3\xb0\x9c\xaa"
#define MY_ICON_WATER_BOILER                "\xf3\xb0\xbe\x92"
#define MY_ICON_WATER_CIRCLE                "\xf3\xb1\xa0\x86"
#define MY_ICON_WATER_PERCENT               "\xf3\xb0\x96\x8e"
#define MY_ICON_WATERING_CAN                "\xf3\xb1\x92\x81"
#define MY_ICON_WATER                       "\xf3\xb0\x96\x8c"
#define MY_ICON_WEATHER_CLOUDY              "\xf3\xb0\x96\x90"
#define MY_ICON_WEATHER_LIGHTNING           "\xf3\xb0\x96\x93"
#define MY_ICON_WEATHER_NIGHT               "\xf3\xb0\x96\x94"
#define MY_ICON_WEATHER_PARTLY_CLOUDY       "\xf3\xb0\x96\x95"
#define MY_ICON_WEATHER_POURING             "\xf3\xb0\x96\x96"
#define MY_ICON_WEATHER_RAINY               "\xf3\xb0\x96\x97"
#define MY_ICON_WEATHER_SNOWY               "\xf3\xb0\x96\x98"
#define MY_ICON_WEATHER_SUNNY               "\xf3\xb0\x96\x99"
#define MY_ICON_WEATHER_WINDY               "\xf3\xb0\x96\x9d"
#define MY_ICON_WIFI_STRENGTH_2             "\xf3\xb0\xa4\xa2"
#define MY_ICON_WIFI                        "\xf3\xb0\x96\xa9"
#define MY_ICON_WIND_TURBINE                "\xf3\xb0\xb6\xa5"
#define MY_ICON_WINDOW_CLOSED               "\xf3\xb0\x96\xae"
#define MY_ICON_WINDOW_CLOSE                "\xf3\xb0\x96\xad"
#define MY_ICON_WINDOW_OPEN                 "\xf3\xb0\x96\xb1"
#define MY_ICON_WRAP_DISABLED               "\xf3\xb0\xaf\x9f"
#define MY_ICON_WRENCH                      "\xf3\xb0\x96\xb7"
#define MY_ICON_YOUTUBE_STUDIO              "\xf3\xb0\xa1\x87"
#define MY_ICON_YOUTUBE_SUBSCRIPTION        "\xf3\xb0\xb5\x80"
#define MY_ICON_YOUTUBE_TV                  "\xf3\xb0\x91\x88"
#define MY_ICON_YOUTUBE                     "\xf3\xb0\x97\x83"

// --- 2. DROPDOWN KATEGORIEN (FÜR UI EDITOR) ---
struct IconCategory { const char* name; const char* options; };

static const IconCategory icon_categories[] = {
    {"Standard", "--- Unveraendert ---\nStandard"},
    {"Alphabet & Zahlen", "--- Unveraendert ---\nStandard\nmdi:alpha-a-box-outline\nmdi:alpha-b-box-outline\nmdi:alpha-c-box-outline\nmdi:alpha-d-box-outline\nmdi:alpha-e-box-outline\nmdi:alpha-f-box-outline\nmdi:alpha-g-box-outline\nmdi:alpha-h-box-outline\nmdi:alpha-i-box-outline\nmdi:alpha-j-box-outline\nmdi:alpha-k-box-outline\nmdi:alpha-l-box-outline\nmdi:alpha-m-box-outline\nmdi:alpha-n-box-outline\nmdi:alpha-o-box-outline\nmdi:alpha-p-box-outline\nmdi:alpha-q-box-outline\nmdi:alpha-r-box-outline\nmdi:alpha-s-box-outline\nmdi:alpha-t-box-outline\nmdi:alpha-u-box-outline\nmdi:alpha-v-box-outline\nmdi:alpha-w-box-outline\nmdi:alpha-x-box-outline\nmdi:alpha-y-box-outline\nmdi:alpha-z-box-outline\nmdi:alpha-a-box\nmdi:alpha-b-box\nmdi:alpha-c-box\nmdi:alpha-d-box\nmdi:alpha-e-box\nmdi:alpha-f-box\nmdi:alpha-g-box\nmdi:alpha-h-box\nmdi:alpha-i-box\nmdi:alpha-j-box\nmdi:alpha-k-box\nmdi:alpha-l-box\nmdi:alpha-m-box\nmdi:alpha-n-box\nmdi:alpha-o-box\nmdi:alpha-p-box\nmdi:alpha-q-box\nmdi:alpha-r-box\nmdi:alpha-s-box\nmdi:alpha-t-box\nmdi:alpha-u-box\nmdi:alpha-v-box\nmdi:alpha-w-box\nmdi:alpha-x-box\nmdi:alpha-y-box\nmdi:alpha-z-box\nmdi:alpha-a-circle-outline\nmdi:alpha-b-circle-outline\nmdi:alpha-c-circle-outline\nmdi:alpha-d-circle-outline\nmdi:alpha-e-circle-outline\nmdi:alpha-f-circle-outline\nmdi:alpha-g-circle-outline\nmdi:alpha-h-circle-outline\nmdi:alpha-i-circle-outline\nmdi:alpha-j-circle-outline\nmdi:alpha-k-circle-outline\nmdi:alpha-l-circle-outline\nmdi:alpha-m-circle-outline\nmdi:alpha-n-circle-outline\nmdi:alpha-o-circle-outline\nmdi:alpha-p-circle-outline\nmdi:alpha-q-circle-outline\nmdi:alpha-r-circle-outline\nmdi:alpha-s-circle-outline\nmdi:alpha-t-circle-outline\nmdi:alpha-u-circle-outline\nmdi:alpha-v-circle-outline\nmdi:alpha-w-circle-outline\nmdi:alpha-x-circle-outline\nmdi:alpha-y-circle-outline\nmdi:alpha-z-circle-outline\nmdi:alpha-a-circle\nmdi:alpha-b-circle\nmdi:alpha-c-circle\nmdi:alpha-d-circle\nmdi:alpha-e-circle\nmdi:alpha-f-circle\nmdi:alpha-g-circle\nmdi:alpha-h-circle\nmdi:alpha-i-circle\nmdi:alpha-j-circle\nmdi:alpha-k-circle\nmdi:alpha-l-circle\nmdi:alpha-m-circle\nmdi:alpha-n-circle\nmdi:alpha-o-circle\nmdi:alpha-p-circle\nmdi:alpha-q-circle\nmdi:alpha-r-circle\nmdi:alpha-s-circle\nmdi:alpha-t-circle\nmdi:alpha-u-circle\nmdi:alpha-v-circle\nmdi:alpha-w-circle\nmdi:alpha-x-circle\nmdi:alpha-y-circle\nmdi:alpha-z-circle\nmdi:alphabet-aurebesh\nmdi:alphabet-cyrillic\nmdi:alphabet-greek\nmdi:alphabet-latin\nmdi:alphabet-piqad\nmdi:alphabet-tengwar\nmdi:alphabetical-off\nmdi:alphabetical-variant-off\nmdi:alphabetical-variant\nmdi:alphabetical\nmdi:alpha\nmdi:alpha-a\nmdi:alpha-b\nmdi:alpha-c\nmdi:alpha-d\nmdi:alpha-e\nmdi:alpha-f\nmdi:alpha-g\nmdi:alpha-h\nmdi:alpha-i\nmdi:alpha-j\nmdi:alpha-k\nmdi:alpha-l\nmdi:alpha-m\nmdi:alpha-n\nmdi:alpha-o\nmdi:alpha-p\nmdi:alpha-q\nmdi:alpha-r\nmdi:alpha-s\nmdi:alpha-t\nmdi:alpha-u\nmdi:alpha-v\nmdi:alpha-w\nmdi:alpha-x\nmdi:alpha-y\nmdi:alpha-z\nmdi:book-alphabet\nmdi:numeric-0-box-multiple-outline\nmdi:numeric-1-box-multiple-outline\nmdi:numeric-2-box-multiple-outline\nmdi:numeric-3-box-multiple-outline\nmdi:numeric-4-box-multiple-outline\nmdi:numeric-5-box-multiple-outline\nmdi:numeric-6-box-multiple-outline\nmdi:numeric-7-box-multiple-outline\nmdi:numeric-8-box-multiple-outline\nmdi:numeric-9-box-multiple-outline\nmdi:numeric-10-box-multiple-outline\nmdi:numeric-0-box-multiple\nmdi:numeric-1-box-multiple\nmdi:numeric-2-box-multiple\nmdi:numeric-3-box-multiple\nmdi:numeric-4-box-multiple\nmdi:numeric-5-box-multiple\nmdi:numeric-6-box-multiple\nmdi:numeric-7-box-multiple\nmdi:numeric-8-box-multiple\nmdi:numeric-9-box-multiple\nmdi:numeric-10-box-multiple\nmdi:numeric-0-box-outline\nmdi:numeric-1-box-outline\nmdi:numeric-2-box-outline\nmdi:numeric-3-box-outline\nmdi:numeric-4-box-outline\nmdi:numeric-5-box-outline\nmdi:numeric-6-box-outline\nmdi:numeric-7-box-outline\nmdi:numeric-8-box-outline\nmdi:numeric-9-box-outline\nmdi:numeric-10-box-outline\nmdi:numeric-0-box\nmdi:numeric-1-box\nmdi:numeric-2-box\nmdi:numeric-3-box\nmdi:numeric-4-box\nmdi:numeric-5-box\nmdi:numeric-6-box\nmdi:numeric-7-box\nmdi:numeric-8-box\nmdi:numeric-9-box\nmdi:numeric-10-box\nmdi:numeric-0-circle-outline\nmdi:numeric-1-circle-outline\nmdi:numeric-2-circle-outline\nmdi:numeric-3-circle-outline\nmdi:numeric-4-circle-outline\nmdi:numeric-5-circle-outline\nmdi:numeric-6-circle-outline\nmdi:numeric-7-circle-outline\nmdi:numeric-8-circle-outline\nmdi:numeric-9-circle-outline\nmdi:numeric-10-circle-outline\nmdi:numeric-0-circle\nmdi:numeric-1-circle\nmdi:numeric-2-circle\nmdi:numeric-3-circle\nmdi:numeric-4-circle\nmdi:numeric-5-circle\nmdi:numeric-6-circle\nmdi:numeric-7-circle\nmdi:numeric-8-circle\nmdi:numeric-9-circle\nmdi:numeric-10-circle\nmdi:numeric-negative-1\nmdi:numeric-off\nmdi:numeric-9-plus-box-multiple-outline\nmdi:numeric-9-plus-box-multiple\nmdi:numeric-9-plus-box-outline\nmdi:numeric-9-plus-box\nmdi:numeric-9-plus-circle-outline\nmdi:numeric-9-plus-circle\nmdi:numeric-9-plus\nmdi:numeric-positive-1\nmdi:numeric\nmdi:numeric-0\nmdi:numeric-1\nmdi:numeric-2\nmdi:numeric-3\nmdi:numeric-4\nmdi:numeric-5\nmdi:numeric-6\nmdi:numeric-7\nmdi:numeric-8\nmdi:numeric-9\nmdi:numeric-10\nmdi:order-alphabetical-ascending\nmdi:order-alphabetical-descending\nmdi:order-numeric-ascending\nmdi:order-numeric-descending\nmdi:sort-alphabetical-ascending-variant\nmdi:sort-alphabetical-ascending\nmdi:sort-alphabetical-descending-variant\nmdi:sort-alphabetical-descending\nmdi:sort-alphabetical-variant\nmdi:sort-numeric-ascending-variant\nmdi:sort-numeric-ascending\nmdi:sort-numeric-descending-variant\nmdi:sort-numeric-descending\nmdi:sort-numeric-variant"},
    {"Andere", "--- Unveraendert ---\nStandard\nmdi:ab-testing\nmdi:abjad-arabic\nmdi:abjad-hebrew\nmdi:abugida-devanagari\nmdi:abugida-thai\nmdi:adjust\nmdi:advertisements-off\nmdi:advertisements\nmdi:air-conditioner\nmdi:air-filter\nmdi:air-horn\nmdi:air-humidifier-off\nmdi:air-humidifier\nmdi:air-purifier-off\nmdi:air-purifier\nmdi:airbag\nmdi:airballoon-outline\nmdi:airballoon\nmdi:alarm-panel\nmdi:apple\nmdi:beer\nmdi:bicycle\nmdi:bottle-wine\nmdi:compass\nmdi:ev-station\nmdi:fingerprint\nmdi:food-apple\nmdi:food-variant\nmdi:gamepad\nmdi:gas-station\nmdi:glass-wine\nmdi:hammer\nmdi:image\nmdi:key-variant\nmdi:key\nmdi:mailbox\nmdi:map-marker\nmdi:map\nmdi:microphone\nmdi:molecule-co2\nmdi:mower\nmdi:mushroom\nmdi:navigation\nmdi:parking\nmdi:road\nmdi:roller-shade\nmdi:screwdriver\nmdi:security\nmdi:sign-direction\nmdi:silverware\nmdi:toaster\nmdi:toolbox\nmdi:transmission-tower\nmdi:tumble-dryer\nmdi:wall-sconce\nmdi:wind-turbine\nmdi:wrench"},
    {"Beleuchtung", "--- Unveraendert ---\nStandard\nmdi:alarm-light-off-outline\nmdi:alarm-light-off\nmdi:alarm-light-outline\nmdi:alarm-light\nmdi:bulkhead-light\nmdi:car-light-dimmed\nmdi:car-light-fog\nmdi:car-light-high\nmdi:car-parking-lights\nmdi:ceiling-light-multiple-outline\nmdi:ceiling-light-multiple\nmdi:ceiling-light-outline\nmdi:ceiling-light\nmdi:chandelier\nmdi:desk-lamp-off\nmdi:desk-lamp-on\nmdi:desk-lamp\nmdi:dome-light\nmdi:floor-lamp-dual-outline\nmdi:floor-lamp-dual\nmdi:floor-lamp-outline\nmdi:floor-lamp-torchiere-outline\nmdi:floor-lamp-torchiere-variant-outline\nmdi:floor-lamp-torchiere-variant\nmdi:floor-lamp-torchiere\nmdi:floor-lamp\nmdi:lamp-outline\nmdi:lamps-outline\nmdi:lamps\nmdi:lamp\nmdi:lava-lamp\nmdi:led-off\nmdi:led-on\nmdi:led-outline\nmdi:led-strip-variant-off\nmdi:led-strip-variant\nmdi:led-strip\nmdi:led-variant-off\nmdi:led-variant-on\nmdi:led-variant-outline\nmdi:lightbulb-on\nmdi:lightbulb-outline\nmdi:lightbulb-question-outline\nmdi:lightbulb-question\nmdi:lightbulb\nmdi:post-lamp\nmdi:spotlight\nmdi:toggle-switch-off-outline\nmdi:toggle-switch-off\nmdi:toggle-switch-outline\nmdi:toggle-switch-variant-off\nmdi:toggle-switch-variant\nmdi:toggle-switch\nmdi:vanity-light\nmdi:wrap-disabled"},
    {"Energie & Tech", "--- Unveraendert ---\nStandard\nmdi:access-point-network-off\nmdi:access-point-network\nmdi:account-network-off-outline\nmdi:account-network-off\nmdi:account-network-outline\nmdi:account-network\nmdi:airplane-clock\nmdi:airplane-edit\nmdi:airplane-landing\nmdi:airplane-marker\nmdi:airplane-off\nmdi:airplane-remove\nmdi:airplane-search\nmdi:airplane-settings\nmdi:airplane-takeoff\nmdi:airplane\nmdi:ambulance\nmdi:battery-charging\nmdi:battery-charging-10\nmdi:battery-charging-20\nmdi:battery-charging-30\nmdi:battery-charging-40\nmdi:battery-charging-50\nmdi:battery-charging-60\nmdi:battery-charging-70\nmdi:battery-charging-80\nmdi:battery-charging-90\nmdi:battery-charging-100\nmdi:battery-off\nmdi:battery-outline\nmdi:battery-remove\nmdi:battery\nmdi:battery-10\nmdi:battery-20\nmdi:battery-30\nmdi:battery-40\nmdi:battery-50\nmdi:battery-60\nmdi:battery-70\nmdi:battery-80\nmdi:battery-90\nmdi:bluetooth\nmdi:flash\nmdi:lan\nmdi:lightning-bolt\nmdi:nas\nmdi:network-strength-outline\nmdi:network-strength-1\nmdi:network-strength-2\nmdi:network-strength-3\nmdi:network-strength-4\nmdi:network\nmdi:router-wireless\nmdi:server\nmdi:solar-panel\nmdi:weather-lightning\nmdi:wifi-strength-2\nmdi:wifi"},
    {"Fahrzeuge", "--- Unveraendert ---\nStandard\nmdi:baby-carriage-off\nmdi:baby-carriage\nmdi:bag-carry-on-off\nmdi:bag-carry-on\nmdi:bicycle-cargo\nmdi:bike-fast\nmdi:bike-pedal-clipless\nmdi:bike-pedal-mountain\nmdi:bike-pedal\nmdi:bike\nmdi:bus-articulated-end\nmdi:bus-articulated-front\nmdi:bus-double-decker\nmdi:bus-electric\nmdi:bus-marker\nmdi:bus-multiple\nmdi:bus\nmdi:car-connected\nmdi:car-electric\nmdi:car-sports\nmdi:car\nmdi:ferry\nmdi:motorbike\nmdi:scooter\nmdi:train\nmdi:truck"},
    {"Fernbedienung & Media", "--- Unveraendert ---\nStandard\nmdi:account-arrow-down-outline\nmdi:account-arrow-left-outline\nmdi:account-arrow-right-outline\nmdi:account-arrow-up-outline\nmdi:account-arrow-down\nmdi:account-arrow-left\nmdi:account-arrow-right\nmdi:account-arrow-up\nmdi:account-child-circle\nmdi:account-circle-outline\nmdi:account-circle\nmdi:account-music-outline\nmdi:account-music\nmdi:account-supervisor-circle-outline\nmdi:account-supervisor-circle\nmdi:animation-play-outline\nmdi:arrow-bottom-left-bold-box-outline\nmdi:arrow-bottom-right-bold-box-outline\nmdi:arrow-down-bold-box-outline\nmdi:arrow-left-bold-box-outline\nmdi:arrow-right-bold-box-outline\nmdi:arrow-top-left-bold-box-outline\nmdi:arrow-top-right-bold-box-outline\nmdi:arrow-up-bold-box-outline\nmdi:arrow-bottom-left-bold-box\nmdi:arrow-bottom-right-bold-box\nmdi:arrow-down-bold-box\nmdi:arrow-left-bold-box\nmdi:arrow-right-bold-box\nmdi:arrow-top-left-bold-box\nmdi:arrow-top-right-bold-box\nmdi:arrow-up-bold-box\nmdi:arrow-down-bold-circle-outline\nmdi:arrow-left-bold-circle-outline\nmdi:arrow-right-bold-circle-outline\nmdi:arrow-up-bold-circle-outline\nmdi:arrow-down-bold-circle\nmdi:arrow-left-bold-circle\nmdi:arrow-right-bold-circle\nmdi:arrow-up-bold-circle\nmdi:arrow-down-bold-hexagon-outline\nmdi:arrow-left-bold-hexagon-outline\nmdi:arrow-right-bold-hexagon-outline\nmdi:arrow-up-bold-hexagon-outline\nmdi:arrow-bottom-left-bold-outline\nmdi:arrow-bottom-right-bold-outline\nmdi:arrow-down-bold-outline\nmdi:arrow-left-bold-outline\nmdi:arrow-left-right-bold-outline\nmdi:arrow-right-bold-outline\nmdi:arrow-top-left-bold-outline\nmdi:arrow-top-right-bold-outline\nmdi:arrow-up-bold-outline\nmdi:arrow-up-down-bold-outline\nmdi:arrow-down-bold\nmdi:arrow-down-left-bold\nmdi:arrow-down-right-bold\nmdi:arrow-left-bold\nmdi:arrow-left-bottom-bold\nmdi:arrow-left-right-bold\nmdi:arrow-left-top-bold\nmdi:arrow-right-bold\nmdi:arrow-right-bottom-bold\nmdi:arrow-right-top-bold\nmdi:arrow-top-left-bottom-right-bold\nmdi:arrow-top-right-bottom-left-bold\nmdi:arrow-u-down-left-bold\nmdi:arrow-u-down-right-bold\nmdi:arrow-u-left-bottom-bold\nmdi:arrow-u-left-top-bold\nmdi:arrow-u-right-bottom-bold\nmdi:arrow-u-right-top-bold\nmdi:arrow-u-up-left-bold\nmdi:arrow-u-up-right-bold\nmdi:arrow-up-bold\nmdi:arrow-up-down-bold\nmdi:arrow-up-left-bold\nmdi:arrow-up-right-bold\nmdi:arrow-down-box\nmdi:arrow-left-box\nmdi:arrow-right-box\nmdi:arrow-up-box\nmdi:arrow-down-circle-outline\nmdi:arrow-left-circle-outline\nmdi:arrow-right-circle-outline\nmdi:arrow-up-circle-outline\nmdi:arrow-down-circle\nmdi:arrow-left-circle\nmdi:arrow-right-circle\nmdi:arrow-up-circle\nmdi:arrow-down\nmdi:arrow-left\nmdi:arrow-right\nmdi:arrow-up\nmdi:camera\nmdi:cast\nmdi:cctv\nmdi:chevron-down-box-outline\nmdi:chevron-left-box-outline\nmdi:chevron-right-box-outline\nmdi:chevron-up-box-outline\nmdi:chevron-down-box\nmdi:chevron-left-box\nmdi:chevron-right-box\nmdi:chevron-up-box\nmdi:chevron-down-circle-outline\nmdi:chevron-left-circle-outline\nmdi:chevron-right-circle-outline\nmdi:chevron-up-circle-outline\nmdi:chevron-down-circle\nmdi:chevron-left-circle\nmdi:chevron-right-circle\nmdi:chevron-up-circle\nmdi:chevron-double-down\nmdi:chevron-double-left\nmdi:chevron-double-right\nmdi:chevron-double-up\nmdi:chevron-triple-down\nmdi:chevron-triple-left\nmdi:chevron-triple-right\nmdi:chevron-triple-up\nmdi:chevron-down\nmdi:chevron-left\nmdi:chevron-right\nmdi:chevron-up\nmdi:circle-outline\nmdi:circle-slice-1\nmdi:circle-slice-2\nmdi:circle-slice-3\nmdi:circle-slice-4\nmdi:circle-slice-5\nmdi:circle-slice-6\nmdi:circle-slice-7\nmdi:circle-slice-8\nmdi:circle\nmdi:contactless-payment-circle-outline\nmdi:doorbell-video\nmdi:dots-horizontal-circle-outline\nmdi:dots-horizontal-circle\nmdi:fan-chevron-down\nmdi:fan-chevron-up\nmdi:fast-forward\nmdi:headphones\nmdi:movie\nmdi:music\nmdi:netflix\nmdi:pause-circle-outline\nmdi:pause-circle\nmdi:pause\nmdi:play\nmdi:remote\nmdi:repeat\nmdi:rewind\nmdi:shuffle\nmdi:speaker\nmdi:spotify\nmdi:stop\nmdi:television-classic\nmdi:television\nmdi:thermometer-chevron-down\nmdi:thermometer-chevron-up\nmdi:video-box\nmdi:video\nmdi:volume-high\nmdi:volume-medium\nmdi:volume-off\nmdi:water-circle\nmdi:youtube-studio\nmdi:youtube-subscription\nmdi:youtube-tv\nmdi:youtube"},
    {"Klima & Wetter", "--- Unveraendert ---\nStandard\nmdi:abacus\nmdi:access-point-off\nmdi:access-point-remove\nmdi:access-point\nmdi:account-badge-outline\nmdi:account-badge\nmdi:account-box-edit-outline\nmdi:account-box-multiple-outline\nmdi:account-box-multiple\nmdi:account-box-outline\nmdi:account-box\nmdi:account-cancel-outline\nmdi:account-cancel\nmdi:account-card-outline\nmdi:account-card\nmdi:account-cash-outline\nmdi:backspace\nmdi:coach-lamp-variant\nmdi:coach-lamp\nmdi:face-man-outline\nmdi:face-man\nmdi:face-woman-outline\nmdi:face-woman\nmdi:facebook-messenger\nmdi:fan-speed-1\nmdi:fan-speed-2\nmdi:fan-speed-3\nmdi:fan\nmdi:fireplace\nmdi:fire\nmdi:gauge\nmdi:hvac-off\nmdi:hvac\nmdi:package\nmdi:radiator\nmdi:snowflake\nmdi:thermometer-lines\nmdi:thermometer\nmdi:thermostat\nmdi:umbrella-outline\nmdi:umbrella\nmdi:water-percent\nmdi:watering-can\nmdi:water\nmdi:weather-cloudy\nmdi:weather-night\nmdi:weather-partly-cloudy\nmdi:weather-pouring\nmdi:weather-rainy\nmdi:weather-snowy\nmdi:weather-sunny\nmdi:weather-windy"},
    {"Küche & Haushalt", "--- Unveraendert ---\nStandard\nmdi:blender-outline\nmdi:blender-software\nmdi:blender\nmdi:coffee-maker-outline\nmdi:coffee-maker\nmdi:coffee-off-outline\nmdi:coffee-off\nmdi:coffee-outline\nmdi:coffee-to-go-outline\nmdi:coffee-to-go\nmdi:coffee\nmdi:cup-off-outline\nmdi:cup-off\nmdi:cup-outline\nmdi:cup-water\nmdi:cupboard-outline\nmdi:cupboard\nmdi:cupcake\nmdi:cup\nmdi:dishwasher-off\nmdi:dishwasher\nmdi:fridge-industrial-off-outline\nmdi:fridge-industrial-off\nmdi:fridge-industrial-outline\nmdi:fridge-outline\nmdi:fridge\nmdi:grill\nmdi:iron\nmdi:kettle\nmdi:microwave\nmdi:robot-vacuum\nmdi:stove\nmdi:vacuum\nmdi:washing-machine\nmdi:water-boiler"},
    {"Natur & Tiere", "--- Unveraendert ---\nStandard\nmdi:application-array-outline\nmdi:application-array\nmdi:application-edit-outline\nmdi:application-edit\nmdi:application-export\nmdi:application-import\nmdi:application-outline\nmdi:application-parentheses-outline\nmdi:application-parentheses\nmdi:application-settings-outline\nmdi:application-settings\nmdi:application-variable-outline\nmdi:application-variable\nmdi:application\nmdi:bee-flower\nmdi:bird\nmdi:book-education-outline\nmdi:cat\nmdi:cow\nmdi:dog\nmdi:fish\nmdi:flower-tulip\nmdi:flower\nmdi:horse\nmdi:leaf\nmdi:pig\nmdi:pine-tree\nmdi:rabbit\nmdi:tree"},
    {"Räume & Möbel", "--- Unveraendert ---\nStandard\nmdi:balcony\nmdi:bathtub-outline\nmdi:bathtub\nmdi:bed-clock\nmdi:bed-double-outline\nmdi:bed-double\nmdi:bed-empty\nmdi:bed-king-outline\nmdi:bed-king\nmdi:bed-outline\nmdi:bed-queen-outline\nmdi:bed-queen\nmdi:bed-single-outline\nmdi:bed-single\nmdi:bed\nmdi:blinds-horizontal\nmdi:blinds-open\nmdi:blinds-vertical\nmdi:blinds\nmdi:bunk-bed-outline\nmdi:bunk-bed\nmdi:chair-rolling\nmdi:door-open\nmdi:doorbell\nmdi:door\nmdi:fence\nmdi:garage\nmdi:home-assistant\nmdi:home\nmdi:mirror\nmdi:pool\nmdi:shower\nmdi:sofa-outline\nmdi:sofa\nmdi:stairs\nmdi:table-furniture\nmdi:table\nmdi:toilet\nmdi:wardrobe\nmdi:window-open"},
    {"Sicherheit", "--- Unveraendert ---\nStandard\nmdi:alarm-bell\nmdi:archive-clock-outline\nmdi:archive-clock\nmdi:archive-lock-open-outline\nmdi:archive-lock-open\nmdi:archive-lock-outline\nmdi:archive-lock\nmdi:axis-lock\nmdi:axis-x-rotate-clockwise\nmdi:axis-y-rotate-clockwise\nmdi:axis-z-rotate-clockwise\nmdi:axis-x-rotate-counterclockwise\nmdi:axis-y-rotate-counterclockwise\nmdi:axis-z-rotate-counterclockwise\nmdi:bell-badge-outline\nmdi:bell-badge\nmdi:bell\nmdi:leak\nmdi:lock-open\nmdi:lock\nmdi:motion-sensor\nmdi:shield\nmdi:smoke-detector"},
    {"UI & Navigation", "--- Unveraendert ---\nStandard\nmdi:access-point-check\nmdi:access-point-minus\nmdi:access-point-plus\nmdi:account-alert-outline\nmdi:account-alert\nmdi:account-box-minus-outline\nmdi:account-box-plus-outline\nmdi:account-check-outline\nmdi:account-check\nmdi:account-cog-outline\nmdi:account-cog\nmdi:account-minus-outline\nmdi:account-minus\nmdi:account-multiple-check-outline\nmdi:account-multiple-check\nmdi:account-multiple-minus-outline\nmdi:alert\nmdi:check-bold\nmdi:check-circle-outline\nmdi:check-circle\nmdi:checkbox-blank-circle-outline\nmdi:checkbox-blank-circle\nmdi:checkbox-blank-off-outline\nmdi:checkbox-blank-off\nmdi:checkbox-blank-outline\nmdi:checkbox-blank\nmdi:checkbox-intermediate-variant\nmdi:checkbox-intermediate\nmdi:checkbox-marked-circle\nmdi:checkbox-marked-outline\nmdi:checkbox-marked\nmdi:check\nmdi:close-circle-outline\nmdi:close-circle\nmdi:close\nmdi:cog\nmdi:content-save\nmdi:door-closed\nmdi:download-circle\nmdi:download\nmdi:fire-alert\nmdi:incognito-circle-off\nmdi:incognito-circle\nmdi:information-slab-circle\nmdi:information-variant-circle-outline\nmdi:information\nmdi:keyboard-return\nmdi:menu-close\nmdi:menu-open\nmdi:menu-down-outline\nmdi:menu-left-outline\nmdi:menu-right-outline\nmdi:menu-up-outline\nmdi:menu-swap-outline\nmdi:menu-swap\nmdi:menu\nmdi:menu-down\nmdi:menu-left\nmdi:menu-right\nmdi:menu-up\nmdi:message-alert-outline\nmdi:message-alert\nmdi:message-check-outline\nmdi:message-check\nmdi:message-cog-outline\nmdi:message-cog\nmdi:minus-thick\nmdi:minus\nmdi:pencil-circle-outline\nmdi:pencil-circle\nmdi:pencil\nmdi:plus-thick\nmdi:plus\nmdi:power\nmdi:refresh\nmdi:shield-check\nmdi:solar-power\nmdi:trash-can\nmdi:undo\nmdi:upload\nmdi:volume-minus\nmdi:volume-plus\nmdi:window-closed\nmdi:window-close"},
};
static const int num_icon_categories = 13;

// --- 3. AUTO-MAPPER FÜR HOME ASSISTANT ---
inline String getAutoIcon(String mdi_icon) {
    if (mdi_icon == "mdi:ab-testing") return MY_ICON_AB_TESTING;
    if (mdi_icon == "mdi:abacus") return MY_ICON_ABACUS;
    if (mdi_icon == "mdi:abjad-arabic") return MY_ICON_ABJAD_ARABIC;
    if (mdi_icon == "mdi:abjad-hebrew") return MY_ICON_ABJAD_HEBREW;
    if (mdi_icon == "mdi:abugida-devanagari") return MY_ICON_ABUGIDA_DEVANAGARI;
    if (mdi_icon == "mdi:abugida-thai") return MY_ICON_ABUGIDA_THAI;
    if (mdi_icon == "mdi:access-point-check") return MY_ICON_ACCESS_POINT_CHECK;
    if (mdi_icon == "mdi:access-point-minus") return MY_ICON_ACCESS_POINT_MINUS;
    if (mdi_icon == "mdi:access-point-network-off") return MY_ICON_ACCESS_POINT_NETWORK_OFF;
    if (mdi_icon == "mdi:access-point-network") return MY_ICON_ACCESS_POINT_NETWORK;
    if (mdi_icon == "mdi:access-point-off") return MY_ICON_ACCESS_POINT_OFF;
    if (mdi_icon == "mdi:access-point-plus") return MY_ICON_ACCESS_POINT_PLUS;
    if (mdi_icon == "mdi:access-point-remove") return MY_ICON_ACCESS_POINT_REMOVE;
    if (mdi_icon == "mdi:access-point") return MY_ICON_ACCESS_POINT;
    if (mdi_icon == "mdi:account-alert-outline") return MY_ICON_ACCOUNT_ALERT_OUTLINE;
    if (mdi_icon == "mdi:account-alert") return MY_ICON_ACCOUNT_ALERT;
    if (mdi_icon == "mdi:account-arrow-down-outline") return MY_ICON_ACCOUNT_ARROW_DOWN_OUTLINE;
    if (mdi_icon == "mdi:account-arrow-left-outline") return MY_ICON_ACCOUNT_ARROW_LEFT_OUTLINE;
    if (mdi_icon == "mdi:account-arrow-right-outline") return MY_ICON_ACCOUNT_ARROW_RIGHT_OUTLINE;
    if (mdi_icon == "mdi:account-arrow-up-outline") return MY_ICON_ACCOUNT_ARROW_UP_OUTLINE;
    if (mdi_icon == "mdi:account-arrow-down") return MY_ICON_ACCOUNT_ARROW_DOWN;
    if (mdi_icon == "mdi:account-arrow-left") return MY_ICON_ACCOUNT_ARROW_LEFT;
    if (mdi_icon == "mdi:account-arrow-right") return MY_ICON_ACCOUNT_ARROW_RIGHT;
    if (mdi_icon == "mdi:account-arrow-up") return MY_ICON_ACCOUNT_ARROW_UP;
    if (mdi_icon == "mdi:account-badge-outline") return MY_ICON_ACCOUNT_BADGE_OUTLINE;
    if (mdi_icon == "mdi:account-badge") return MY_ICON_ACCOUNT_BADGE;
    if (mdi_icon == "mdi:account-box-edit-outline") return MY_ICON_ACCOUNT_BOX_EDIT_OUTLINE;
    if (mdi_icon == "mdi:account-box-minus-outline") return MY_ICON_ACCOUNT_BOX_MINUS_OUTLINE;
    if (mdi_icon == "mdi:account-box-multiple-outline") return MY_ICON_ACCOUNT_BOX_MULTIPLE_OUTLINE;
    if (mdi_icon == "mdi:account-box-multiple") return MY_ICON_ACCOUNT_BOX_MULTIPLE;
    if (mdi_icon == "mdi:account-box-outline") return MY_ICON_ACCOUNT_BOX_OUTLINE;
    if (mdi_icon == "mdi:account-box-plus-outline") return MY_ICON_ACCOUNT_BOX_PLUS_OUTLINE;
    if (mdi_icon == "mdi:account-box") return MY_ICON_ACCOUNT_BOX;
    if (mdi_icon == "mdi:account-cancel-outline") return MY_ICON_ACCOUNT_CANCEL_OUTLINE;
    if (mdi_icon == "mdi:account-cancel") return MY_ICON_ACCOUNT_CANCEL;
    if (mdi_icon == "mdi:account-card-outline") return MY_ICON_ACCOUNT_CARD_OUTLINE;
    if (mdi_icon == "mdi:account-card") return MY_ICON_ACCOUNT_CARD;
    if (mdi_icon == "mdi:account-cash-outline") return MY_ICON_ACCOUNT_CASH_OUTLINE;
    if (mdi_icon == "mdi:account-check-outline") return MY_ICON_ACCOUNT_CHECK_OUTLINE;
    if (mdi_icon == "mdi:account-check") return MY_ICON_ACCOUNT_CHECK;
    if (mdi_icon == "mdi:account-child-circle") return MY_ICON_ACCOUNT_CHILD_CIRCLE;
    if (mdi_icon == "mdi:account-circle-outline") return MY_ICON_ACCOUNT_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:account-circle") return MY_ICON_ACCOUNT_CIRCLE;
    if (mdi_icon == "mdi:account-cog-outline") return MY_ICON_ACCOUNT_COG_OUTLINE;
    if (mdi_icon == "mdi:account-cog") return MY_ICON_ACCOUNT_COG;
    if (mdi_icon == "mdi:account-minus-outline") return MY_ICON_ACCOUNT_MINUS_OUTLINE;
    if (mdi_icon == "mdi:account-minus") return MY_ICON_ACCOUNT_MINUS;
    if (mdi_icon == "mdi:account-multiple-check-outline") return MY_ICON_ACCOUNT_MULTIPLE_CHECK_OUTLINE;
    if (mdi_icon == "mdi:account-multiple-check") return MY_ICON_ACCOUNT_MULTIPLE_CHECK;
    if (mdi_icon == "mdi:account-multiple-minus-outline") return MY_ICON_ACCOUNT_MULTIPLE_MINUS_OUTLINE;
    if (mdi_icon == "mdi:account-music-outline") return MY_ICON_ACCOUNT_MUSIC_OUTLINE;
    if (mdi_icon == "mdi:account-music") return MY_ICON_ACCOUNT_MUSIC;
    if (mdi_icon == "mdi:account-network-off-outline") return MY_ICON_ACCOUNT_NETWORK_OFF_OUTLINE;
    if (mdi_icon == "mdi:account-network-off") return MY_ICON_ACCOUNT_NETWORK_OFF;
    if (mdi_icon == "mdi:account-network-outline") return MY_ICON_ACCOUNT_NETWORK_OUTLINE;
    if (mdi_icon == "mdi:account-network") return MY_ICON_ACCOUNT_NETWORK;
    if (mdi_icon == "mdi:account-supervisor-circle-outline") return MY_ICON_ACCOUNT_SUPERVISOR_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:account-supervisor-circle") return MY_ICON_ACCOUNT_SUPERVISOR_CIRCLE;
    if (mdi_icon == "mdi:adjust") return MY_ICON_ADJUST;
    if (mdi_icon == "mdi:advertisements-off") return MY_ICON_ADVERTISEMENTS_OFF;
    if (mdi_icon == "mdi:advertisements") return MY_ICON_ADVERTISEMENTS;
    if (mdi_icon == "mdi:air-conditioner") return MY_ICON_AIR_CONDITIONER;
    if (mdi_icon == "mdi:air-filter") return MY_ICON_AIR_FILTER;
    if (mdi_icon == "mdi:air-horn") return MY_ICON_AIR_HORN;
    if (mdi_icon == "mdi:air-humidifier-off") return MY_ICON_AIR_HUMIDIFIER_OFF;
    if (mdi_icon == "mdi:air-humidifier") return MY_ICON_AIR_HUMIDIFIER;
    if (mdi_icon == "mdi:air-purifier-off") return MY_ICON_AIR_PURIFIER_OFF;
    if (mdi_icon == "mdi:air-purifier") return MY_ICON_AIR_PURIFIER;
    if (mdi_icon == "mdi:airbag") return MY_ICON_AIRBAG;
    if (mdi_icon == "mdi:airballoon-outline") return MY_ICON_AIRBALLOON_OUTLINE;
    if (mdi_icon == "mdi:airballoon") return MY_ICON_AIRBALLOON;
    if (mdi_icon == "mdi:airplane-clock") return MY_ICON_AIRPLANE_CLOCK;
    if (mdi_icon == "mdi:airplane-edit") return MY_ICON_AIRPLANE_EDIT;
    if (mdi_icon == "mdi:airplane-landing") return MY_ICON_AIRPLANE_LANDING;
    if (mdi_icon == "mdi:airplane-marker") return MY_ICON_AIRPLANE_MARKER;
    if (mdi_icon == "mdi:airplane-off") return MY_ICON_AIRPLANE_OFF;
    if (mdi_icon == "mdi:airplane-remove") return MY_ICON_AIRPLANE_REMOVE;
    if (mdi_icon == "mdi:airplane-search") return MY_ICON_AIRPLANE_SEARCH;
    if (mdi_icon == "mdi:airplane-settings") return MY_ICON_AIRPLANE_SETTINGS;
    if (mdi_icon == "mdi:airplane-takeoff") return MY_ICON_AIRPLANE_TAKEOFF;
    if (mdi_icon == "mdi:airplane") return MY_ICON_AIRPLANE;
    if (mdi_icon == "mdi:alarm-bell") return MY_ICON_ALARM_BELL;
    if (mdi_icon == "mdi:alarm-light-off-outline") return MY_ICON_ALARM_LIGHT_OFF_OUTLINE;
    if (mdi_icon == "mdi:alarm-light-off") return MY_ICON_ALARM_LIGHT_OFF;
    if (mdi_icon == "mdi:alarm-light-outline") return MY_ICON_ALARM_LIGHT_OUTLINE;
    if (mdi_icon == "mdi:alarm-light") return MY_ICON_ALARM_LIGHT;
    if (mdi_icon == "mdi:alarm-panel") return MY_ICON_ALARM_PANEL;
    if (mdi_icon == "mdi:alert") return MY_ICON_ALERT;
    if (mdi_icon == "mdi:alpha-a-box-outline") return MY_ICON_ALPHA_A_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-b-box-outline") return MY_ICON_ALPHA_B_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-c-box-outline") return MY_ICON_ALPHA_C_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-d-box-outline") return MY_ICON_ALPHA_D_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-e-box-outline") return MY_ICON_ALPHA_E_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-f-box-outline") return MY_ICON_ALPHA_F_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-g-box-outline") return MY_ICON_ALPHA_G_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-h-box-outline") return MY_ICON_ALPHA_H_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-i-box-outline") return MY_ICON_ALPHA_I_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-j-box-outline") return MY_ICON_ALPHA_J_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-k-box-outline") return MY_ICON_ALPHA_K_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-l-box-outline") return MY_ICON_ALPHA_L_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-m-box-outline") return MY_ICON_ALPHA_M_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-n-box-outline") return MY_ICON_ALPHA_N_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-o-box-outline") return MY_ICON_ALPHA_O_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-p-box-outline") return MY_ICON_ALPHA_P_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-q-box-outline") return MY_ICON_ALPHA_Q_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-r-box-outline") return MY_ICON_ALPHA_R_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-s-box-outline") return MY_ICON_ALPHA_S_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-t-box-outline") return MY_ICON_ALPHA_T_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-u-box-outline") return MY_ICON_ALPHA_U_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-v-box-outline") return MY_ICON_ALPHA_V_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-w-box-outline") return MY_ICON_ALPHA_W_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-x-box-outline") return MY_ICON_ALPHA_X_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-y-box-outline") return MY_ICON_ALPHA_Y_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-z-box-outline") return MY_ICON_ALPHA_Z_BOX_OUTLINE;
    if (mdi_icon == "mdi:alpha-a-box") return MY_ICON_ALPHA_A_BOX;
    if (mdi_icon == "mdi:alpha-b-box") return MY_ICON_ALPHA_B_BOX;
    if (mdi_icon == "mdi:alpha-c-box") return MY_ICON_ALPHA_C_BOX;
    if (mdi_icon == "mdi:alpha-d-box") return MY_ICON_ALPHA_D_BOX;
    if (mdi_icon == "mdi:alpha-e-box") return MY_ICON_ALPHA_E_BOX;
    if (mdi_icon == "mdi:alpha-f-box") return MY_ICON_ALPHA_F_BOX;
    if (mdi_icon == "mdi:alpha-g-box") return MY_ICON_ALPHA_G_BOX;
    if (mdi_icon == "mdi:alpha-h-box") return MY_ICON_ALPHA_H_BOX;
    if (mdi_icon == "mdi:alpha-i-box") return MY_ICON_ALPHA_I_BOX;
    if (mdi_icon == "mdi:alpha-j-box") return MY_ICON_ALPHA_J_BOX;
    if (mdi_icon == "mdi:alpha-k-box") return MY_ICON_ALPHA_K_BOX;
    if (mdi_icon == "mdi:alpha-l-box") return MY_ICON_ALPHA_L_BOX;
    if (mdi_icon == "mdi:alpha-m-box") return MY_ICON_ALPHA_M_BOX;
    if (mdi_icon == "mdi:alpha-n-box") return MY_ICON_ALPHA_N_BOX;
    if (mdi_icon == "mdi:alpha-o-box") return MY_ICON_ALPHA_O_BOX;
    if (mdi_icon == "mdi:alpha-p-box") return MY_ICON_ALPHA_P_BOX;
    if (mdi_icon == "mdi:alpha-q-box") return MY_ICON_ALPHA_Q_BOX;
    if (mdi_icon == "mdi:alpha-r-box") return MY_ICON_ALPHA_R_BOX;
    if (mdi_icon == "mdi:alpha-s-box") return MY_ICON_ALPHA_S_BOX;
    if (mdi_icon == "mdi:alpha-t-box") return MY_ICON_ALPHA_T_BOX;
    if (mdi_icon == "mdi:alpha-u-box") return MY_ICON_ALPHA_U_BOX;
    if (mdi_icon == "mdi:alpha-v-box") return MY_ICON_ALPHA_V_BOX;
    if (mdi_icon == "mdi:alpha-w-box") return MY_ICON_ALPHA_W_BOX;
    if (mdi_icon == "mdi:alpha-x-box") return MY_ICON_ALPHA_X_BOX;
    if (mdi_icon == "mdi:alpha-y-box") return MY_ICON_ALPHA_Y_BOX;
    if (mdi_icon == "mdi:alpha-z-box") return MY_ICON_ALPHA_Z_BOX;
    if (mdi_icon == "mdi:alpha-a-circle-outline") return MY_ICON_ALPHA_A_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-b-circle-outline") return MY_ICON_ALPHA_B_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-c-circle-outline") return MY_ICON_ALPHA_C_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-d-circle-outline") return MY_ICON_ALPHA_D_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-e-circle-outline") return MY_ICON_ALPHA_E_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-f-circle-outline") return MY_ICON_ALPHA_F_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-g-circle-outline") return MY_ICON_ALPHA_G_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-h-circle-outline") return MY_ICON_ALPHA_H_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-i-circle-outline") return MY_ICON_ALPHA_I_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-j-circle-outline") return MY_ICON_ALPHA_J_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-k-circle-outline") return MY_ICON_ALPHA_K_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-l-circle-outline") return MY_ICON_ALPHA_L_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-m-circle-outline") return MY_ICON_ALPHA_M_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-n-circle-outline") return MY_ICON_ALPHA_N_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-o-circle-outline") return MY_ICON_ALPHA_O_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-p-circle-outline") return MY_ICON_ALPHA_P_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-q-circle-outline") return MY_ICON_ALPHA_Q_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-r-circle-outline") return MY_ICON_ALPHA_R_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-s-circle-outline") return MY_ICON_ALPHA_S_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-t-circle-outline") return MY_ICON_ALPHA_T_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-u-circle-outline") return MY_ICON_ALPHA_U_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-v-circle-outline") return MY_ICON_ALPHA_V_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-w-circle-outline") return MY_ICON_ALPHA_W_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-x-circle-outline") return MY_ICON_ALPHA_X_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-y-circle-outline") return MY_ICON_ALPHA_Y_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-z-circle-outline") return MY_ICON_ALPHA_Z_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:alpha-a-circle") return MY_ICON_ALPHA_A_CIRCLE;
    if (mdi_icon == "mdi:alpha-b-circle") return MY_ICON_ALPHA_B_CIRCLE;
    if (mdi_icon == "mdi:alpha-c-circle") return MY_ICON_ALPHA_C_CIRCLE;
    if (mdi_icon == "mdi:alpha-d-circle") return MY_ICON_ALPHA_D_CIRCLE;
    if (mdi_icon == "mdi:alpha-e-circle") return MY_ICON_ALPHA_E_CIRCLE;
    if (mdi_icon == "mdi:alpha-f-circle") return MY_ICON_ALPHA_F_CIRCLE;
    if (mdi_icon == "mdi:alpha-g-circle") return MY_ICON_ALPHA_G_CIRCLE;
    if (mdi_icon == "mdi:alpha-h-circle") return MY_ICON_ALPHA_H_CIRCLE;
    if (mdi_icon == "mdi:alpha-i-circle") return MY_ICON_ALPHA_I_CIRCLE;
    if (mdi_icon == "mdi:alpha-j-circle") return MY_ICON_ALPHA_J_CIRCLE;
    if (mdi_icon == "mdi:alpha-k-circle") return MY_ICON_ALPHA_K_CIRCLE;
    if (mdi_icon == "mdi:alpha-l-circle") return MY_ICON_ALPHA_L_CIRCLE;
    if (mdi_icon == "mdi:alpha-m-circle") return MY_ICON_ALPHA_M_CIRCLE;
    if (mdi_icon == "mdi:alpha-n-circle") return MY_ICON_ALPHA_N_CIRCLE;
    if (mdi_icon == "mdi:alpha-o-circle") return MY_ICON_ALPHA_O_CIRCLE;
    if (mdi_icon == "mdi:alpha-p-circle") return MY_ICON_ALPHA_P_CIRCLE;
    if (mdi_icon == "mdi:alpha-q-circle") return MY_ICON_ALPHA_Q_CIRCLE;
    if (mdi_icon == "mdi:alpha-r-circle") return MY_ICON_ALPHA_R_CIRCLE;
    if (mdi_icon == "mdi:alpha-s-circle") return MY_ICON_ALPHA_S_CIRCLE;
    if (mdi_icon == "mdi:alpha-t-circle") return MY_ICON_ALPHA_T_CIRCLE;
    if (mdi_icon == "mdi:alpha-u-circle") return MY_ICON_ALPHA_U_CIRCLE;
    if (mdi_icon == "mdi:alpha-v-circle") return MY_ICON_ALPHA_V_CIRCLE;
    if (mdi_icon == "mdi:alpha-w-circle") return MY_ICON_ALPHA_W_CIRCLE;
    if (mdi_icon == "mdi:alpha-x-circle") return MY_ICON_ALPHA_X_CIRCLE;
    if (mdi_icon == "mdi:alpha-y-circle") return MY_ICON_ALPHA_Y_CIRCLE;
    if (mdi_icon == "mdi:alpha-z-circle") return MY_ICON_ALPHA_Z_CIRCLE;
    if (mdi_icon == "mdi:alphabet-aurebesh") return MY_ICON_ALPHABET_AUREBESH;
    if (mdi_icon == "mdi:alphabet-cyrillic") return MY_ICON_ALPHABET_CYRILLIC;
    if (mdi_icon == "mdi:alphabet-greek") return MY_ICON_ALPHABET_GREEK;
    if (mdi_icon == "mdi:alphabet-latin") return MY_ICON_ALPHABET_LATIN;
    if (mdi_icon == "mdi:alphabet-piqad") return MY_ICON_ALPHABET_PIQAD;
    if (mdi_icon == "mdi:alphabet-tengwar") return MY_ICON_ALPHABET_TENGWAR;
    if (mdi_icon == "mdi:alphabetical-off") return MY_ICON_ALPHABETICAL_OFF;
    if (mdi_icon == "mdi:alphabetical-variant-off") return MY_ICON_ALPHABETICAL_VARIANT_OFF;
    if (mdi_icon == "mdi:alphabetical-variant") return MY_ICON_ALPHABETICAL_VARIANT;
    if (mdi_icon == "mdi:alphabetical") return MY_ICON_ALPHABETICAL;
    if (mdi_icon == "mdi:alpha") return MY_ICON_ALPHA;
    if (mdi_icon == "mdi:alpha-a") return MY_ICON_ALPHA_A;
    if (mdi_icon == "mdi:alpha-b") return MY_ICON_ALPHA_B;
    if (mdi_icon == "mdi:alpha-c") return MY_ICON_ALPHA_C;
    if (mdi_icon == "mdi:alpha-d") return MY_ICON_ALPHA_D;
    if (mdi_icon == "mdi:alpha-e") return MY_ICON_ALPHA_E;
    if (mdi_icon == "mdi:alpha-f") return MY_ICON_ALPHA_F;
    if (mdi_icon == "mdi:alpha-g") return MY_ICON_ALPHA_G;
    if (mdi_icon == "mdi:alpha-h") return MY_ICON_ALPHA_H;
    if (mdi_icon == "mdi:alpha-i") return MY_ICON_ALPHA_I;
    if (mdi_icon == "mdi:alpha-j") return MY_ICON_ALPHA_J;
    if (mdi_icon == "mdi:alpha-k") return MY_ICON_ALPHA_K;
    if (mdi_icon == "mdi:alpha-l") return MY_ICON_ALPHA_L;
    if (mdi_icon == "mdi:alpha-m") return MY_ICON_ALPHA_M;
    if (mdi_icon == "mdi:alpha-n") return MY_ICON_ALPHA_N;
    if (mdi_icon == "mdi:alpha-o") return MY_ICON_ALPHA_O;
    if (mdi_icon == "mdi:alpha-p") return MY_ICON_ALPHA_P;
    if (mdi_icon == "mdi:alpha-q") return MY_ICON_ALPHA_Q;
    if (mdi_icon == "mdi:alpha-r") return MY_ICON_ALPHA_R;
    if (mdi_icon == "mdi:alpha-s") return MY_ICON_ALPHA_S;
    if (mdi_icon == "mdi:alpha-t") return MY_ICON_ALPHA_T;
    if (mdi_icon == "mdi:alpha-u") return MY_ICON_ALPHA_U;
    if (mdi_icon == "mdi:alpha-v") return MY_ICON_ALPHA_V;
    if (mdi_icon == "mdi:alpha-w") return MY_ICON_ALPHA_W;
    if (mdi_icon == "mdi:alpha-x") return MY_ICON_ALPHA_X;
    if (mdi_icon == "mdi:alpha-y") return MY_ICON_ALPHA_Y;
    if (mdi_icon == "mdi:alpha-z") return MY_ICON_ALPHA_Z;
    if (mdi_icon == "mdi:ambulance") return MY_ICON_AMBULANCE;
    if (mdi_icon == "mdi:animation-play-outline") return MY_ICON_ANIMATION_PLAY_OUTLINE;
    if (mdi_icon == "mdi:apple") return MY_ICON_APPLE;
    if (mdi_icon == "mdi:application-array-outline") return MY_ICON_APPLICATION_ARRAY_OUTLINE;
    if (mdi_icon == "mdi:application-array") return MY_ICON_APPLICATION_ARRAY;
    if (mdi_icon == "mdi:application-edit-outline") return MY_ICON_APPLICATION_EDIT_OUTLINE;
    if (mdi_icon == "mdi:application-edit") return MY_ICON_APPLICATION_EDIT;
    if (mdi_icon == "mdi:application-export") return MY_ICON_APPLICATION_EXPORT;
    if (mdi_icon == "mdi:application-import") return MY_ICON_APPLICATION_IMPORT;
    if (mdi_icon == "mdi:application-outline") return MY_ICON_APPLICATION_OUTLINE;
    if (mdi_icon == "mdi:application-parentheses-outline") return MY_ICON_APPLICATION_PARENTHESES_OUTLINE;
    if (mdi_icon == "mdi:application-parentheses") return MY_ICON_APPLICATION_PARENTHESES;
    if (mdi_icon == "mdi:application-settings-outline") return MY_ICON_APPLICATION_SETTINGS_OUTLINE;
    if (mdi_icon == "mdi:application-settings") return MY_ICON_APPLICATION_SETTINGS;
    if (mdi_icon == "mdi:application-variable-outline") return MY_ICON_APPLICATION_VARIABLE_OUTLINE;
    if (mdi_icon == "mdi:application-variable") return MY_ICON_APPLICATION_VARIABLE;
    if (mdi_icon == "mdi:application") return MY_ICON_APPLICATION;
    if (mdi_icon == "mdi:archive-clock-outline") return MY_ICON_ARCHIVE_CLOCK_OUTLINE;
    if (mdi_icon == "mdi:archive-clock") return MY_ICON_ARCHIVE_CLOCK;
    if (mdi_icon == "mdi:archive-lock-open-outline") return MY_ICON_ARCHIVE_LOCK_OPEN_OUTLINE;
    if (mdi_icon == "mdi:archive-lock-open") return MY_ICON_ARCHIVE_LOCK_OPEN;
    if (mdi_icon == "mdi:archive-lock-outline") return MY_ICON_ARCHIVE_LOCK_OUTLINE;
    if (mdi_icon == "mdi:archive-lock") return MY_ICON_ARCHIVE_LOCK;
    if (mdi_icon == "mdi:arrow-bottom-left-bold-box-outline") return MY_ICON_ARROW_BOTTOM_LEFT_BOLD_BOX_OUTLINE;
    if (mdi_icon == "mdi:arrow-bottom-right-bold-box-outline") return MY_ICON_ARROW_BOTTOM_RIGHT_BOLD_BOX_OUTLINE;
    if (mdi_icon == "mdi:arrow-down-bold-box-outline") return MY_ICON_ARROW_DOWN_BOLD_BOX_OUTLINE;
    if (mdi_icon == "mdi:arrow-left-bold-box-outline") return MY_ICON_ARROW_LEFT_BOLD_BOX_OUTLINE;
    if (mdi_icon == "mdi:arrow-right-bold-box-outline") return MY_ICON_ARROW_RIGHT_BOLD_BOX_OUTLINE;
    if (mdi_icon == "mdi:arrow-top-left-bold-box-outline") return MY_ICON_ARROW_TOP_LEFT_BOLD_BOX_OUTLINE;
    if (mdi_icon == "mdi:arrow-top-right-bold-box-outline") return MY_ICON_ARROW_TOP_RIGHT_BOLD_BOX_OUTLINE;
    if (mdi_icon == "mdi:arrow-up-bold-box-outline") return MY_ICON_ARROW_UP_BOLD_BOX_OUTLINE;
    if (mdi_icon == "mdi:arrow-bottom-left-bold-box") return MY_ICON_ARROW_BOTTOM_LEFT_BOLD_BOX;
    if (mdi_icon == "mdi:arrow-bottom-right-bold-box") return MY_ICON_ARROW_BOTTOM_RIGHT_BOLD_BOX;
    if (mdi_icon == "mdi:arrow-down-bold-box") return MY_ICON_ARROW_DOWN_BOLD_BOX;
    if (mdi_icon == "mdi:arrow-left-bold-box") return MY_ICON_ARROW_LEFT_BOLD_BOX;
    if (mdi_icon == "mdi:arrow-right-bold-box") return MY_ICON_ARROW_RIGHT_BOLD_BOX;
    if (mdi_icon == "mdi:arrow-top-left-bold-box") return MY_ICON_ARROW_TOP_LEFT_BOLD_BOX;
    if (mdi_icon == "mdi:arrow-top-right-bold-box") return MY_ICON_ARROW_TOP_RIGHT_BOLD_BOX;
    if (mdi_icon == "mdi:arrow-up-bold-box") return MY_ICON_ARROW_UP_BOLD_BOX;
    if (mdi_icon == "mdi:arrow-down-bold-circle-outline") return MY_ICON_ARROW_DOWN_BOLD_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:arrow-left-bold-circle-outline") return MY_ICON_ARROW_LEFT_BOLD_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:arrow-right-bold-circle-outline") return MY_ICON_ARROW_RIGHT_BOLD_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:arrow-up-bold-circle-outline") return MY_ICON_ARROW_UP_BOLD_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:arrow-down-bold-circle") return MY_ICON_ARROW_DOWN_BOLD_CIRCLE;
    if (mdi_icon == "mdi:arrow-left-bold-circle") return MY_ICON_ARROW_LEFT_BOLD_CIRCLE;
    if (mdi_icon == "mdi:arrow-right-bold-circle") return MY_ICON_ARROW_RIGHT_BOLD_CIRCLE;
    if (mdi_icon == "mdi:arrow-up-bold-circle") return MY_ICON_ARROW_UP_BOLD_CIRCLE;
    if (mdi_icon == "mdi:arrow-down-bold-hexagon-outline") return MY_ICON_ARROW_DOWN_BOLD_HEXAGON_OUTLINE;
    if (mdi_icon == "mdi:arrow-left-bold-hexagon-outline") return MY_ICON_ARROW_LEFT_BOLD_HEXAGON_OUTLINE;
    if (mdi_icon == "mdi:arrow-right-bold-hexagon-outline") return MY_ICON_ARROW_RIGHT_BOLD_HEXAGON_OUTLINE;
    if (mdi_icon == "mdi:arrow-up-bold-hexagon-outline") return MY_ICON_ARROW_UP_BOLD_HEXAGON_OUTLINE;
    if (mdi_icon == "mdi:arrow-bottom-left-bold-outline") return MY_ICON_ARROW_BOTTOM_LEFT_BOLD_OUTLINE;
    if (mdi_icon == "mdi:arrow-bottom-right-bold-outline") return MY_ICON_ARROW_BOTTOM_RIGHT_BOLD_OUTLINE;
    if (mdi_icon == "mdi:arrow-down-bold-outline") return MY_ICON_ARROW_DOWN_BOLD_OUTLINE;
    if (mdi_icon == "mdi:arrow-left-bold-outline") return MY_ICON_ARROW_LEFT_BOLD_OUTLINE;
    if (mdi_icon == "mdi:arrow-left-right-bold-outline") return MY_ICON_ARROW_LEFT_RIGHT_BOLD_OUTLINE;
    if (mdi_icon == "mdi:arrow-right-bold-outline") return MY_ICON_ARROW_RIGHT_BOLD_OUTLINE;
    if (mdi_icon == "mdi:arrow-top-left-bold-outline") return MY_ICON_ARROW_TOP_LEFT_BOLD_OUTLINE;
    if (mdi_icon == "mdi:arrow-top-right-bold-outline") return MY_ICON_ARROW_TOP_RIGHT_BOLD_OUTLINE;
    if (mdi_icon == "mdi:arrow-up-bold-outline") return MY_ICON_ARROW_UP_BOLD_OUTLINE;
    if (mdi_icon == "mdi:arrow-up-down-bold-outline") return MY_ICON_ARROW_UP_DOWN_BOLD_OUTLINE;
    if (mdi_icon == "mdi:arrow-down-bold") return MY_ICON_ARROW_DOWN_BOLD;
    if (mdi_icon == "mdi:arrow-down-left-bold") return MY_ICON_ARROW_DOWN_LEFT_BOLD;
    if (mdi_icon == "mdi:arrow-down-right-bold") return MY_ICON_ARROW_DOWN_RIGHT_BOLD;
    if (mdi_icon == "mdi:arrow-left-bold") return MY_ICON_ARROW_LEFT_BOLD;
    if (mdi_icon == "mdi:arrow-left-bottom-bold") return MY_ICON_ARROW_LEFT_BOTTOM_BOLD;
    if (mdi_icon == "mdi:arrow-left-right-bold") return MY_ICON_ARROW_LEFT_RIGHT_BOLD;
    if (mdi_icon == "mdi:arrow-left-top-bold") return MY_ICON_ARROW_LEFT_TOP_BOLD;
    if (mdi_icon == "mdi:arrow-right-bold") return MY_ICON_ARROW_RIGHT_BOLD;
    if (mdi_icon == "mdi:arrow-right-bottom-bold") return MY_ICON_ARROW_RIGHT_BOTTOM_BOLD;
    if (mdi_icon == "mdi:arrow-right-top-bold") return MY_ICON_ARROW_RIGHT_TOP_BOLD;
    if (mdi_icon == "mdi:arrow-top-left-bottom-right-bold") return MY_ICON_ARROW_TOP_LEFT_BOTTOM_RIGHT_BOLD;
    if (mdi_icon == "mdi:arrow-top-right-bottom-left-bold") return MY_ICON_ARROW_TOP_RIGHT_BOTTOM_LEFT_BOLD;
    if (mdi_icon == "mdi:arrow-u-down-left-bold") return MY_ICON_ARROW_U_DOWN_LEFT_BOLD;
    if (mdi_icon == "mdi:arrow-u-down-right-bold") return MY_ICON_ARROW_U_DOWN_RIGHT_BOLD;
    if (mdi_icon == "mdi:arrow-u-left-bottom-bold") return MY_ICON_ARROW_U_LEFT_BOTTOM_BOLD;
    if (mdi_icon == "mdi:arrow-u-left-top-bold") return MY_ICON_ARROW_U_LEFT_TOP_BOLD;
    if (mdi_icon == "mdi:arrow-u-right-bottom-bold") return MY_ICON_ARROW_U_RIGHT_BOTTOM_BOLD;
    if (mdi_icon == "mdi:arrow-u-right-top-bold") return MY_ICON_ARROW_U_RIGHT_TOP_BOLD;
    if (mdi_icon == "mdi:arrow-u-up-left-bold") return MY_ICON_ARROW_U_UP_LEFT_BOLD;
    if (mdi_icon == "mdi:arrow-u-up-right-bold") return MY_ICON_ARROW_U_UP_RIGHT_BOLD;
    if (mdi_icon == "mdi:arrow-up-bold") return MY_ICON_ARROW_UP_BOLD;
    if (mdi_icon == "mdi:arrow-up-down-bold") return MY_ICON_ARROW_UP_DOWN_BOLD;
    if (mdi_icon == "mdi:arrow-up-left-bold") return MY_ICON_ARROW_UP_LEFT_BOLD;
    if (mdi_icon == "mdi:arrow-up-right-bold") return MY_ICON_ARROW_UP_RIGHT_BOLD;
    if (mdi_icon == "mdi:arrow-down-box") return MY_ICON_ARROW_DOWN_BOX;
    if (mdi_icon == "mdi:arrow-left-box") return MY_ICON_ARROW_LEFT_BOX;
    if (mdi_icon == "mdi:arrow-right-box") return MY_ICON_ARROW_RIGHT_BOX;
    if (mdi_icon == "mdi:arrow-up-box") return MY_ICON_ARROW_UP_BOX;
    if (mdi_icon == "mdi:arrow-down-circle-outline") return MY_ICON_ARROW_DOWN_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:arrow-left-circle-outline") return MY_ICON_ARROW_LEFT_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:arrow-right-circle-outline") return MY_ICON_ARROW_RIGHT_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:arrow-up-circle-outline") return MY_ICON_ARROW_UP_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:arrow-down-circle") return MY_ICON_ARROW_DOWN_CIRCLE;
    if (mdi_icon == "mdi:arrow-left-circle") return MY_ICON_ARROW_LEFT_CIRCLE;
    if (mdi_icon == "mdi:arrow-right-circle") return MY_ICON_ARROW_RIGHT_CIRCLE;
    if (mdi_icon == "mdi:arrow-up-circle") return MY_ICON_ARROW_UP_CIRCLE;
    if (mdi_icon == "mdi:arrow-down") return MY_ICON_ARROW_DOWN;
    if (mdi_icon == "mdi:arrow-left") return MY_ICON_ARROW_LEFT;
    if (mdi_icon == "mdi:arrow-right") return MY_ICON_ARROW_RIGHT;
    if (mdi_icon == "mdi:arrow-up") return MY_ICON_ARROW_UP;
    if (mdi_icon == "mdi:axis-lock") return MY_ICON_AXIS_LOCK;
    if (mdi_icon == "mdi:axis-x-rotate-clockwise") return MY_ICON_AXIS_X_ROTATE_CLOCKWISE;
    if (mdi_icon == "mdi:axis-y-rotate-clockwise") return MY_ICON_AXIS_Y_ROTATE_CLOCKWISE;
    if (mdi_icon == "mdi:axis-z-rotate-clockwise") return MY_ICON_AXIS_Z_ROTATE_CLOCKWISE;
    if (mdi_icon == "mdi:axis-x-rotate-counterclockwise") return MY_ICON_AXIS_X_ROTATE_COUNTERCLOCKWISE;
    if (mdi_icon == "mdi:axis-y-rotate-counterclockwise") return MY_ICON_AXIS_Y_ROTATE_COUNTERCLOCKWISE;
    if (mdi_icon == "mdi:axis-z-rotate-counterclockwise") return MY_ICON_AXIS_Z_ROTATE_COUNTERCLOCKWISE;
    if (mdi_icon == "mdi:baby-carriage-off") return MY_ICON_BABY_CARRIAGE_OFF;
    if (mdi_icon == "mdi:baby-carriage") return MY_ICON_BABY_CARRIAGE;
    if (mdi_icon == "mdi:backspace") return MY_ICON_BACKSPACE;
    if (mdi_icon == "mdi:bag-carry-on-off") return MY_ICON_BAG_CARRY_ON_OFF;
    if (mdi_icon == "mdi:bag-carry-on") return MY_ICON_BAG_CARRY_ON;
    if (mdi_icon == "mdi:balcony") return MY_ICON_BALCONY;
    if (mdi_icon == "mdi:bathtub-outline") return MY_ICON_BATHTUB_OUTLINE;
    if (mdi_icon == "mdi:bathtub") return MY_ICON_BATHTUB;
    if (mdi_icon == "mdi:battery-charging") return MY_ICON_BATTERY_CHARGING;
    if (mdi_icon == "mdi:battery-charging-10") return MY_ICON_BATTERY_CHARGING_10;
    if (mdi_icon == "mdi:battery-charging-20") return MY_ICON_BATTERY_CHARGING_20;
    if (mdi_icon == "mdi:battery-charging-30") return MY_ICON_BATTERY_CHARGING_30;
    if (mdi_icon == "mdi:battery-charging-40") return MY_ICON_BATTERY_CHARGING_40;
    if (mdi_icon == "mdi:battery-charging-50") return MY_ICON_BATTERY_CHARGING_50;
    if (mdi_icon == "mdi:battery-charging-60") return MY_ICON_BATTERY_CHARGING_60;
    if (mdi_icon == "mdi:battery-charging-70") return MY_ICON_BATTERY_CHARGING_70;
    if (mdi_icon == "mdi:battery-charging-80") return MY_ICON_BATTERY_CHARGING_80;
    if (mdi_icon == "mdi:battery-charging-90") return MY_ICON_BATTERY_CHARGING_90;
    if (mdi_icon == "mdi:battery-charging-100") return MY_ICON_BATTERY_CHARGING_100;
    if (mdi_icon == "mdi:battery-off") return MY_ICON_BATTERY_OFF;
    if (mdi_icon == "mdi:battery-outline") return MY_ICON_BATTERY_OUTLINE;
    if (mdi_icon == "mdi:battery-remove") return MY_ICON_BATTERY_REMOVE;
    if (mdi_icon == "mdi:battery") return MY_ICON_BATTERY;
    if (mdi_icon == "mdi:battery-10") return MY_ICON_BATTERY_10;
    if (mdi_icon == "mdi:battery-20") return MY_ICON_BATTERY_20;
    if (mdi_icon == "mdi:battery-30") return MY_ICON_BATTERY_30;
    if (mdi_icon == "mdi:battery-40") return MY_ICON_BATTERY_40;
    if (mdi_icon == "mdi:battery-50") return MY_ICON_BATTERY_50;
    if (mdi_icon == "mdi:battery-60") return MY_ICON_BATTERY_60;
    if (mdi_icon == "mdi:battery-70") return MY_ICON_BATTERY_70;
    if (mdi_icon == "mdi:battery-80") return MY_ICON_BATTERY_80;
    if (mdi_icon == "mdi:battery-90") return MY_ICON_BATTERY_90;
    if (mdi_icon == "mdi:bed-clock") return MY_ICON_BED_CLOCK;
    if (mdi_icon == "mdi:bed-double-outline") return MY_ICON_BED_DOUBLE_OUTLINE;
    if (mdi_icon == "mdi:bed-double") return MY_ICON_BED_DOUBLE;
    if (mdi_icon == "mdi:bed-empty") return MY_ICON_BED_EMPTY;
    if (mdi_icon == "mdi:bed-king-outline") return MY_ICON_BED_KING_OUTLINE;
    if (mdi_icon == "mdi:bed-king") return MY_ICON_BED_KING;
    if (mdi_icon == "mdi:bed-outline") return MY_ICON_BED_OUTLINE;
    if (mdi_icon == "mdi:bed-queen-outline") return MY_ICON_BED_QUEEN_OUTLINE;
    if (mdi_icon == "mdi:bed-queen") return MY_ICON_BED_QUEEN;
    if (mdi_icon == "mdi:bed-single-outline") return MY_ICON_BED_SINGLE_OUTLINE;
    if (mdi_icon == "mdi:bed-single") return MY_ICON_BED_SINGLE;
    if (mdi_icon == "mdi:bed") return MY_ICON_BED;
    if (mdi_icon == "mdi:bee-flower") return MY_ICON_BEE_FLOWER;
    if (mdi_icon == "mdi:beer") return MY_ICON_BEER;
    if (mdi_icon == "mdi:bell-badge-outline") return MY_ICON_BELL_BADGE_OUTLINE;
    if (mdi_icon == "mdi:bell-badge") return MY_ICON_BELL_BADGE;
    if (mdi_icon == "mdi:bell") return MY_ICON_BELL;
    if (mdi_icon == "mdi:bicycle-cargo") return MY_ICON_BICYCLE_CARGO;
    if (mdi_icon == "mdi:bicycle") return MY_ICON_BICYCLE;
    if (mdi_icon == "mdi:bike-fast") return MY_ICON_BIKE_FAST;
    if (mdi_icon == "mdi:bike-pedal-clipless") return MY_ICON_BIKE_PEDAL_CLIPLESS;
    if (mdi_icon == "mdi:bike-pedal-mountain") return MY_ICON_BIKE_PEDAL_MOUNTAIN;
    if (mdi_icon == "mdi:bike-pedal") return MY_ICON_BIKE_PEDAL;
    if (mdi_icon == "mdi:bike") return MY_ICON_BIKE;
    if (mdi_icon == "mdi:bird") return MY_ICON_BIRD;
    if (mdi_icon == "mdi:blender-outline") return MY_ICON_BLENDER_OUTLINE;
    if (mdi_icon == "mdi:blender-software") return MY_ICON_BLENDER_SOFTWARE;
    if (mdi_icon == "mdi:blender") return MY_ICON_BLENDER;
    if (mdi_icon == "mdi:blinds-horizontal") return MY_ICON_BLINDS_HORIZONTAL;
    if (mdi_icon == "mdi:blinds-open") return MY_ICON_BLINDS_OPEN;
    if (mdi_icon == "mdi:blinds-vertical") return MY_ICON_BLINDS_VERTICAL;
    if (mdi_icon == "mdi:blinds") return MY_ICON_BLINDS;
    if (mdi_icon == "mdi:bluetooth") return MY_ICON_BLUETOOTH;
    if (mdi_icon == "mdi:book-alphabet") return MY_ICON_BOOK_ALPHABET;
    if (mdi_icon == "mdi:book-education-outline") return MY_ICON_BOOK_EDUCATION_OUTLINE;
    if (mdi_icon == "mdi:bottle-wine") return MY_ICON_BOTTLE_WINE;
    if (mdi_icon == "mdi:bulkhead-light") return MY_ICON_BULKHEAD_LIGHT;
    if (mdi_icon == "mdi:bunk-bed-outline") return MY_ICON_BUNK_BED_OUTLINE;
    if (mdi_icon == "mdi:bunk-bed") return MY_ICON_BUNK_BED;
    if (mdi_icon == "mdi:bus-articulated-end") return MY_ICON_BUS_ARTICULATED_END;
    if (mdi_icon == "mdi:bus-articulated-front") return MY_ICON_BUS_ARTICULATED_FRONT;
    if (mdi_icon == "mdi:bus-double-decker") return MY_ICON_BUS_DOUBLE_DECKER;
    if (mdi_icon == "mdi:bus-electric") return MY_ICON_BUS_ELECTRIC;
    if (mdi_icon == "mdi:bus-marker") return MY_ICON_BUS_MARKER;
    if (mdi_icon == "mdi:bus-multiple") return MY_ICON_BUS_MULTIPLE;
    if (mdi_icon == "mdi:bus") return MY_ICON_BUS;
    if (mdi_icon == "mdi:camera") return MY_ICON_CAMERA;
    if (mdi_icon == "mdi:car-connected") return MY_ICON_CAR_CONNECTED;
    if (mdi_icon == "mdi:car-electric") return MY_ICON_CAR_ELECTRIC;
    if (mdi_icon == "mdi:car-light-dimmed") return MY_ICON_CAR_LIGHT_DIMMED;
    if (mdi_icon == "mdi:car-light-fog") return MY_ICON_CAR_LIGHT_FOG;
    if (mdi_icon == "mdi:car-light-high") return MY_ICON_CAR_LIGHT_HIGH;
    if (mdi_icon == "mdi:car-parking-lights") return MY_ICON_CAR_PARKING_LIGHTS;
    if (mdi_icon == "mdi:car-sports") return MY_ICON_CAR_SPORTS;
    if (mdi_icon == "mdi:car") return MY_ICON_CAR;
    if (mdi_icon == "mdi:cast") return MY_ICON_CAST;
    if (mdi_icon == "mdi:cat") return MY_ICON_CAT;
    if (mdi_icon == "mdi:cctv") return MY_ICON_CCTV;
    if (mdi_icon == "mdi:ceiling-light-multiple-outline") return MY_ICON_CEILING_LIGHT_MULTIPLE_OUTLINE;
    if (mdi_icon == "mdi:ceiling-light-multiple") return MY_ICON_CEILING_LIGHT_MULTIPLE;
    if (mdi_icon == "mdi:ceiling-light-outline") return MY_ICON_CEILING_LIGHT_OUTLINE;
    if (mdi_icon == "mdi:ceiling-light") return MY_ICON_CEILING_LIGHT;
    if (mdi_icon == "mdi:chair-rolling") return MY_ICON_CHAIR_ROLLING;
    if (mdi_icon == "mdi:chandelier") return MY_ICON_CHANDELIER;
    if (mdi_icon == "mdi:check-bold") return MY_ICON_CHECK_BOLD;
    if (mdi_icon == "mdi:check-circle-outline") return MY_ICON_CHECK_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:check-circle") return MY_ICON_CHECK_CIRCLE;
    if (mdi_icon == "mdi:checkbox-blank-circle-outline") return MY_ICON_CHECKBOX_BLANK_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:checkbox-blank-circle") return MY_ICON_CHECKBOX_BLANK_CIRCLE;
    if (mdi_icon == "mdi:checkbox-blank-off-outline") return MY_ICON_CHECKBOX_BLANK_OFF_OUTLINE;
    if (mdi_icon == "mdi:checkbox-blank-off") return MY_ICON_CHECKBOX_BLANK_OFF;
    if (mdi_icon == "mdi:checkbox-blank-outline") return MY_ICON_CHECKBOX_BLANK_OUTLINE;
    if (mdi_icon == "mdi:checkbox-blank") return MY_ICON_CHECKBOX_BLANK;
    if (mdi_icon == "mdi:checkbox-intermediate-variant") return MY_ICON_CHECKBOX_INTERMEDIATE_VARIANT;
    if (mdi_icon == "mdi:checkbox-intermediate") return MY_ICON_CHECKBOX_INTERMEDIATE;
    if (mdi_icon == "mdi:checkbox-marked-circle") return MY_ICON_CHECKBOX_MARKED_CIRCLE;
    if (mdi_icon == "mdi:checkbox-marked-outline") return MY_ICON_CHECKBOX_MARKED_OUTLINE;
    if (mdi_icon == "mdi:checkbox-marked") return MY_ICON_CHECKBOX_MARKED;
    if (mdi_icon == "mdi:check") return MY_ICON_CHECK;
    if (mdi_icon == "mdi:chevron-down-box-outline") return MY_ICON_CHEVRON_DOWN_BOX_OUTLINE;
    if (mdi_icon == "mdi:chevron-left-box-outline") return MY_ICON_CHEVRON_LEFT_BOX_OUTLINE;
    if (mdi_icon == "mdi:chevron-right-box-outline") return MY_ICON_CHEVRON_RIGHT_BOX_OUTLINE;
    if (mdi_icon == "mdi:chevron-up-box-outline") return MY_ICON_CHEVRON_UP_BOX_OUTLINE;
    if (mdi_icon == "mdi:chevron-down-box") return MY_ICON_CHEVRON_DOWN_BOX;
    if (mdi_icon == "mdi:chevron-left-box") return MY_ICON_CHEVRON_LEFT_BOX;
    if (mdi_icon == "mdi:chevron-right-box") return MY_ICON_CHEVRON_RIGHT_BOX;
    if (mdi_icon == "mdi:chevron-up-box") return MY_ICON_CHEVRON_UP_BOX;
    if (mdi_icon == "mdi:chevron-down-circle-outline") return MY_ICON_CHEVRON_DOWN_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:chevron-left-circle-outline") return MY_ICON_CHEVRON_LEFT_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:chevron-right-circle-outline") return MY_ICON_CHEVRON_RIGHT_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:chevron-up-circle-outline") return MY_ICON_CHEVRON_UP_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:chevron-down-circle") return MY_ICON_CHEVRON_DOWN_CIRCLE;
    if (mdi_icon == "mdi:chevron-left-circle") return MY_ICON_CHEVRON_LEFT_CIRCLE;
    if (mdi_icon == "mdi:chevron-right-circle") return MY_ICON_CHEVRON_RIGHT_CIRCLE;
    if (mdi_icon == "mdi:chevron-up-circle") return MY_ICON_CHEVRON_UP_CIRCLE;
    if (mdi_icon == "mdi:chevron-double-down") return MY_ICON_CHEVRON_DOUBLE_DOWN;
    if (mdi_icon == "mdi:chevron-double-left") return MY_ICON_CHEVRON_DOUBLE_LEFT;
    if (mdi_icon == "mdi:chevron-double-right") return MY_ICON_CHEVRON_DOUBLE_RIGHT;
    if (mdi_icon == "mdi:chevron-double-up") return MY_ICON_CHEVRON_DOUBLE_UP;
    if (mdi_icon == "mdi:chevron-triple-down") return MY_ICON_CHEVRON_TRIPLE_DOWN;
    if (mdi_icon == "mdi:chevron-triple-left") return MY_ICON_CHEVRON_TRIPLE_LEFT;
    if (mdi_icon == "mdi:chevron-triple-right") return MY_ICON_CHEVRON_TRIPLE_RIGHT;
    if (mdi_icon == "mdi:chevron-triple-up") return MY_ICON_CHEVRON_TRIPLE_UP;
    if (mdi_icon == "mdi:chevron-down") return MY_ICON_CHEVRON_DOWN;
    if (mdi_icon == "mdi:chevron-left") return MY_ICON_CHEVRON_LEFT;
    if (mdi_icon == "mdi:chevron-right") return MY_ICON_CHEVRON_RIGHT;
    if (mdi_icon == "mdi:chevron-up") return MY_ICON_CHEVRON_UP;
    if (mdi_icon == "mdi:circle-outline") return MY_ICON_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:circle-slice-1") return MY_ICON_CIRCLE_SLICE_1;
    if (mdi_icon == "mdi:circle-slice-2") return MY_ICON_CIRCLE_SLICE_2;
    if (mdi_icon == "mdi:circle-slice-3") return MY_ICON_CIRCLE_SLICE_3;
    if (mdi_icon == "mdi:circle-slice-4") return MY_ICON_CIRCLE_SLICE_4;
    if (mdi_icon == "mdi:circle-slice-5") return MY_ICON_CIRCLE_SLICE_5;
    if (mdi_icon == "mdi:circle-slice-6") return MY_ICON_CIRCLE_SLICE_6;
    if (mdi_icon == "mdi:circle-slice-7") return MY_ICON_CIRCLE_SLICE_7;
    if (mdi_icon == "mdi:circle-slice-8") return MY_ICON_CIRCLE_SLICE_8;
    if (mdi_icon == "mdi:circle") return MY_ICON_CIRCLE;
    if (mdi_icon == "mdi:close-circle-outline") return MY_ICON_CLOSE_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:close-circle") return MY_ICON_CLOSE_CIRCLE;
    if (mdi_icon == "mdi:close") return MY_ICON_CLOSE;
    if (mdi_icon == "mdi:coach-lamp-variant") return MY_ICON_COACH_LAMP_VARIANT;
    if (mdi_icon == "mdi:coach-lamp") return MY_ICON_COACH_LAMP;
    if (mdi_icon == "mdi:coffee-maker-outline") return MY_ICON_COFFEE_MAKER_OUTLINE;
    if (mdi_icon == "mdi:coffee-maker") return MY_ICON_COFFEE_MAKER;
    if (mdi_icon == "mdi:coffee-off-outline") return MY_ICON_COFFEE_OFF_OUTLINE;
    if (mdi_icon == "mdi:coffee-off") return MY_ICON_COFFEE_OFF;
    if (mdi_icon == "mdi:coffee-outline") return MY_ICON_COFFEE_OUTLINE;
    if (mdi_icon == "mdi:coffee-to-go-outline") return MY_ICON_COFFEE_TO_GO_OUTLINE;
    if (mdi_icon == "mdi:coffee-to-go") return MY_ICON_COFFEE_TO_GO;
    if (mdi_icon == "mdi:coffee") return MY_ICON_COFFEE;
    if (mdi_icon == "mdi:cog") return MY_ICON_COG;
    if (mdi_icon == "mdi:compass") return MY_ICON_COMPASS;
    if (mdi_icon == "mdi:contactless-payment-circle-outline") return MY_ICON_CONTACTLESS_PAYMENT_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:content-save") return MY_ICON_CONTENT_SAVE;
    if (mdi_icon == "mdi:cow") return MY_ICON_COW;
    if (mdi_icon == "mdi:cup-off-outline") return MY_ICON_CUP_OFF_OUTLINE;
    if (mdi_icon == "mdi:cup-off") return MY_ICON_CUP_OFF;
    if (mdi_icon == "mdi:cup-outline") return MY_ICON_CUP_OUTLINE;
    if (mdi_icon == "mdi:cup-water") return MY_ICON_CUP_WATER;
    if (mdi_icon == "mdi:cupboard-outline") return MY_ICON_CUPBOARD_OUTLINE;
    if (mdi_icon == "mdi:cupboard") return MY_ICON_CUPBOARD;
    if (mdi_icon == "mdi:cupcake") return MY_ICON_CUPCAKE;
    if (mdi_icon == "mdi:cup") return MY_ICON_CUP;
    if (mdi_icon == "mdi:desk-lamp-off") return MY_ICON_DESK_LAMP_OFF;
    if (mdi_icon == "mdi:desk-lamp-on") return MY_ICON_DESK_LAMP_ON;
    if (mdi_icon == "mdi:desk-lamp") return MY_ICON_DESK_LAMP;
    if (mdi_icon == "mdi:dishwasher-off") return MY_ICON_DISHWASHER_OFF;
    if (mdi_icon == "mdi:dishwasher") return MY_ICON_DISHWASHER;
    if (mdi_icon == "mdi:dog") return MY_ICON_DOG;
    if (mdi_icon == "mdi:dome-light") return MY_ICON_DOME_LIGHT;
    if (mdi_icon == "mdi:door-closed") return MY_ICON_DOOR_CLOSED;
    if (mdi_icon == "mdi:door-open") return MY_ICON_DOOR_OPEN;
    if (mdi_icon == "mdi:doorbell-video") return MY_ICON_DOORBELL_VIDEO;
    if (mdi_icon == "mdi:doorbell") return MY_ICON_DOORBELL;
    if (mdi_icon == "mdi:door") return MY_ICON_DOOR;
    if (mdi_icon == "mdi:dots-horizontal-circle-outline") return MY_ICON_DOTS_HORIZONTAL_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:dots-horizontal-circle") return MY_ICON_DOTS_HORIZONTAL_CIRCLE;
    if (mdi_icon == "mdi:download-circle") return MY_ICON_DOWNLOAD_CIRCLE;
    if (mdi_icon == "mdi:download") return MY_ICON_DOWNLOAD;
    if (mdi_icon == "mdi:ev-station") return MY_ICON_EV_STATION;
    if (mdi_icon == "mdi:face-man-outline") return MY_ICON_FACE_MAN_OUTLINE;
    if (mdi_icon == "mdi:face-man") return MY_ICON_FACE_MAN;
    if (mdi_icon == "mdi:face-woman-outline") return MY_ICON_FACE_WOMAN_OUTLINE;
    if (mdi_icon == "mdi:face-woman") return MY_ICON_FACE_WOMAN;
    if (mdi_icon == "mdi:facebook-messenger") return MY_ICON_FACEBOOK_MESSENGER;
    if (mdi_icon == "mdi:fan-chevron-down") return MY_ICON_FAN_CHEVRON_DOWN;
    if (mdi_icon == "mdi:fan-chevron-up") return MY_ICON_FAN_CHEVRON_UP;
    if (mdi_icon == "mdi:fan-speed-1") return MY_ICON_FAN_SPEED_1;
    if (mdi_icon == "mdi:fan-speed-2") return MY_ICON_FAN_SPEED_2;
    if (mdi_icon == "mdi:fan-speed-3") return MY_ICON_FAN_SPEED_3;
    if (mdi_icon == "mdi:fan") return MY_ICON_FAN;
    if (mdi_icon == "mdi:fast-forward") return MY_ICON_FAST_FORWARD;
    if (mdi_icon == "mdi:fence") return MY_ICON_FENCE;
    if (mdi_icon == "mdi:ferry") return MY_ICON_FERRY;
    if (mdi_icon == "mdi:fingerprint") return MY_ICON_FINGERPRINT;
    if (mdi_icon == "mdi:fire-alert") return MY_ICON_FIRE_ALERT;
    if (mdi_icon == "mdi:fireplace") return MY_ICON_FIREPLACE;
    if (mdi_icon == "mdi:fire") return MY_ICON_FIRE;
    if (mdi_icon == "mdi:fish") return MY_ICON_FISH;
    if (mdi_icon == "mdi:flash") return MY_ICON_FLASH;
    if (mdi_icon == "mdi:floor-lamp-dual-outline") return MY_ICON_FLOOR_LAMP_DUAL_OUTLINE;
    if (mdi_icon == "mdi:floor-lamp-dual") return MY_ICON_FLOOR_LAMP_DUAL;
    if (mdi_icon == "mdi:floor-lamp-outline") return MY_ICON_FLOOR_LAMP_OUTLINE;
    if (mdi_icon == "mdi:floor-lamp-torchiere-outline") return MY_ICON_FLOOR_LAMP_TORCHIERE_OUTLINE;
    if (mdi_icon == "mdi:floor-lamp-torchiere-variant-outline") return MY_ICON_FLOOR_LAMP_TORCHIERE_VARIANT_OUTLINE;
    if (mdi_icon == "mdi:floor-lamp-torchiere-variant") return MY_ICON_FLOOR_LAMP_TORCHIERE_VARIANT;
    if (mdi_icon == "mdi:floor-lamp-torchiere") return MY_ICON_FLOOR_LAMP_TORCHIERE;
    if (mdi_icon == "mdi:floor-lamp") return MY_ICON_FLOOR_LAMP;
    if (mdi_icon == "mdi:flower-tulip") return MY_ICON_FLOWER_TULIP;
    if (mdi_icon == "mdi:flower") return MY_ICON_FLOWER;
    if (mdi_icon == "mdi:food-apple") return MY_ICON_FOOD_APPLE;
    if (mdi_icon == "mdi:food-variant") return MY_ICON_FOOD_VARIANT;
    if (mdi_icon == "mdi:fridge-industrial-off-outline") return MY_ICON_FRIDGE_INDUSTRIAL_OFF_OUTLINE;
    if (mdi_icon == "mdi:fridge-industrial-off") return MY_ICON_FRIDGE_INDUSTRIAL_OFF;
    if (mdi_icon == "mdi:fridge-industrial-outline") return MY_ICON_FRIDGE_INDUSTRIAL_OUTLINE;
    if (mdi_icon == "mdi:fridge-outline") return MY_ICON_FRIDGE_OUTLINE;
    if (mdi_icon == "mdi:fridge") return MY_ICON_FRIDGE;
    if (mdi_icon == "mdi:gamepad") return MY_ICON_GAMEPAD;
    if (mdi_icon == "mdi:garage") return MY_ICON_GARAGE;
    if (mdi_icon == "mdi:gas-station") return MY_ICON_GAS_STATION;
    if (mdi_icon == "mdi:gauge") return MY_ICON_GAUGE;
    if (mdi_icon == "mdi:glass-wine") return MY_ICON_GLASS_WINE;
    if (mdi_icon == "mdi:grill") return MY_ICON_GRILL;
    if (mdi_icon == "mdi:hammer") return MY_ICON_HAMMER;
    if (mdi_icon == "mdi:headphones") return MY_ICON_HEADPHONES;
    if (mdi_icon == "mdi:home-assistant") return MY_ICON_HOME_ASSISTANT;
    if (mdi_icon == "mdi:home") return MY_ICON_HOME;
    if (mdi_icon == "mdi:horse") return MY_ICON_HORSE;
    if (mdi_icon == "mdi:hvac-off") return MY_ICON_HVAC_OFF;
    if (mdi_icon == "mdi:hvac") return MY_ICON_HVAC;
    if (mdi_icon == "mdi:image") return MY_ICON_IMAGE;
    if (mdi_icon == "mdi:incognito-circle-off") return MY_ICON_INCOGNITO_CIRCLE_OFF;
    if (mdi_icon == "mdi:incognito-circle") return MY_ICON_INCOGNITO_CIRCLE;
    if (mdi_icon == "mdi:information-slab-circle") return MY_ICON_INFORMATION_SLAB_CIRCLE;
    if (mdi_icon == "mdi:information-variant-circle-outline") return MY_ICON_INFORMATION_VARIANT_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:information") return MY_ICON_INFORMATION;
    if (mdi_icon == "mdi:iron") return MY_ICON_IRON;
    if (mdi_icon == "mdi:kettle") return MY_ICON_KETTLE;
    if (mdi_icon == "mdi:key-variant") return MY_ICON_KEY_VARIANT;
    if (mdi_icon == "mdi:keyboard-return") return MY_ICON_KEYBOARD_RETURN;
    if (mdi_icon == "mdi:key") return MY_ICON_KEY;
    if (mdi_icon == "mdi:lamp-outline") return MY_ICON_LAMP_OUTLINE;
    if (mdi_icon == "mdi:lamps-outline") return MY_ICON_LAMPS_OUTLINE;
    if (mdi_icon == "mdi:lamps") return MY_ICON_LAMPS;
    if (mdi_icon == "mdi:lamp") return MY_ICON_LAMP;
    if (mdi_icon == "mdi:lan") return MY_ICON_LAN;
    if (mdi_icon == "mdi:lava-lamp") return MY_ICON_LAVA_LAMP;
    if (mdi_icon == "mdi:leaf") return MY_ICON_LEAF;
    if (mdi_icon == "mdi:leak") return MY_ICON_LEAK;
    if (mdi_icon == "mdi:led-off") return MY_ICON_LED_OFF;
    if (mdi_icon == "mdi:led-on") return MY_ICON_LED_ON;
    if (mdi_icon == "mdi:led-outline") return MY_ICON_LED_OUTLINE;
    if (mdi_icon == "mdi:led-strip-variant-off") return MY_ICON_LED_STRIP_VARIANT_OFF;
    if (mdi_icon == "mdi:led-strip-variant") return MY_ICON_LED_STRIP_VARIANT;
    if (mdi_icon == "mdi:led-strip") return MY_ICON_LED_STRIP;
    if (mdi_icon == "mdi:led-variant-off") return MY_ICON_LED_VARIANT_OFF;
    if (mdi_icon == "mdi:led-variant-on") return MY_ICON_LED_VARIANT_ON;
    if (mdi_icon == "mdi:led-variant-outline") return MY_ICON_LED_VARIANT_OUTLINE;
    if (mdi_icon == "mdi:lightbulb-on") return MY_ICON_LIGHTBULB_ON;
    if (mdi_icon == "mdi:lightbulb-outline") return MY_ICON_LIGHTBULB_OUTLINE;
    if (mdi_icon == "mdi:lightbulb-question-outline") return MY_ICON_LIGHTBULB_QUESTION_OUTLINE;
    if (mdi_icon == "mdi:lightbulb-question") return MY_ICON_LIGHTBULB_QUESTION;
    if (mdi_icon == "mdi:lightbulb") return MY_ICON_LIGHTBULB;
    if (mdi_icon == "mdi:lightning-bolt") return MY_ICON_LIGHTNING_BOLT;
    if (mdi_icon == "mdi:lock-open") return MY_ICON_LOCK_OPEN;
    if (mdi_icon == "mdi:lock") return MY_ICON_LOCK;
    if (mdi_icon == "mdi:mailbox") return MY_ICON_MAILBOX;
    if (mdi_icon == "mdi:map-marker") return MY_ICON_MAP_MARKER;
    if (mdi_icon == "mdi:map") return MY_ICON_MAP;
    if (mdi_icon == "mdi:menu-close") return MY_ICON_MENU_CLOSE;
    if (mdi_icon == "mdi:menu-open") return MY_ICON_MENU_OPEN;
    if (mdi_icon == "mdi:menu-down-outline") return MY_ICON_MENU_DOWN_OUTLINE;
    if (mdi_icon == "mdi:menu-left-outline") return MY_ICON_MENU_LEFT_OUTLINE;
    if (mdi_icon == "mdi:menu-right-outline") return MY_ICON_MENU_RIGHT_OUTLINE;
    if (mdi_icon == "mdi:menu-up-outline") return MY_ICON_MENU_UP_OUTLINE;
    if (mdi_icon == "mdi:menu-swap-outline") return MY_ICON_MENU_SWAP_OUTLINE;
    if (mdi_icon == "mdi:menu-swap") return MY_ICON_MENU_SWAP;
    if (mdi_icon == "mdi:menu") return MY_ICON_MENU;
    if (mdi_icon == "mdi:menu-down") return MY_ICON_MENU_DOWN;
    if (mdi_icon == "mdi:menu-left") return MY_ICON_MENU_LEFT;
    if (mdi_icon == "mdi:menu-right") return MY_ICON_MENU_RIGHT;
    if (mdi_icon == "mdi:menu-up") return MY_ICON_MENU_UP;
    if (mdi_icon == "mdi:message-alert-outline") return MY_ICON_MESSAGE_ALERT_OUTLINE;
    if (mdi_icon == "mdi:message-alert") return MY_ICON_MESSAGE_ALERT;
    if (mdi_icon == "mdi:message-check-outline") return MY_ICON_MESSAGE_CHECK_OUTLINE;
    if (mdi_icon == "mdi:message-check") return MY_ICON_MESSAGE_CHECK;
    if (mdi_icon == "mdi:message-cog-outline") return MY_ICON_MESSAGE_COG_OUTLINE;
    if (mdi_icon == "mdi:message-cog") return MY_ICON_MESSAGE_COG;
    if (mdi_icon == "mdi:microphone") return MY_ICON_MICROPHONE;
    if (mdi_icon == "mdi:microwave") return MY_ICON_MICROWAVE;
    if (mdi_icon == "mdi:minus-thick") return MY_ICON_MINUS_THICK;
    if (mdi_icon == "mdi:minus") return MY_ICON_MINUS;
    if (mdi_icon == "mdi:mirror") return MY_ICON_MIRROR;
    if (mdi_icon == "mdi:molecule-co2") return MY_ICON_MOLECULE_CO2;
    if (mdi_icon == "mdi:motion-sensor") return MY_ICON_MOTION_SENSOR;
    if (mdi_icon == "mdi:motorbike") return MY_ICON_MOTORBIKE;
    if (mdi_icon == "mdi:movie") return MY_ICON_MOVIE;
    if (mdi_icon == "mdi:mower") return MY_ICON_MOWER;
    if (mdi_icon == "mdi:mushroom") return MY_ICON_MUSHROOM;
    if (mdi_icon == "mdi:music") return MY_ICON_MUSIC;
    if (mdi_icon == "mdi:nas") return MY_ICON_NAS;
    if (mdi_icon == "mdi:navigation") return MY_ICON_NAVIGATION;
    if (mdi_icon == "mdi:netflix") return MY_ICON_NETFLIX;
    if (mdi_icon == "mdi:network-strength-outline") return MY_ICON_NETWORK_STRENGTH_OUTLINE;
    if (mdi_icon == "mdi:network-strength-1") return MY_ICON_NETWORK_STRENGTH_1;
    if (mdi_icon == "mdi:network-strength-2") return MY_ICON_NETWORK_STRENGTH_2;
    if (mdi_icon == "mdi:network-strength-3") return MY_ICON_NETWORK_STRENGTH_3;
    if (mdi_icon == "mdi:network-strength-4") return MY_ICON_NETWORK_STRENGTH_4;
    if (mdi_icon == "mdi:network") return MY_ICON_NETWORK;
    if (mdi_icon == "mdi:numeric-0-box-multiple-outline") return MY_ICON_NUMERIC_0_BOX_MULTIPLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-1-box-multiple-outline") return MY_ICON_NUMERIC_1_BOX_MULTIPLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-2-box-multiple-outline") return MY_ICON_NUMERIC_2_BOX_MULTIPLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-3-box-multiple-outline") return MY_ICON_NUMERIC_3_BOX_MULTIPLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-4-box-multiple-outline") return MY_ICON_NUMERIC_4_BOX_MULTIPLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-5-box-multiple-outline") return MY_ICON_NUMERIC_5_BOX_MULTIPLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-6-box-multiple-outline") return MY_ICON_NUMERIC_6_BOX_MULTIPLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-7-box-multiple-outline") return MY_ICON_NUMERIC_7_BOX_MULTIPLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-8-box-multiple-outline") return MY_ICON_NUMERIC_8_BOX_MULTIPLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-9-box-multiple-outline") return MY_ICON_NUMERIC_9_BOX_MULTIPLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-10-box-multiple-outline") return MY_ICON_NUMERIC_10_BOX_MULTIPLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-0-box-multiple") return MY_ICON_NUMERIC_0_BOX_MULTIPLE;
    if (mdi_icon == "mdi:numeric-1-box-multiple") return MY_ICON_NUMERIC_1_BOX_MULTIPLE;
    if (mdi_icon == "mdi:numeric-2-box-multiple") return MY_ICON_NUMERIC_2_BOX_MULTIPLE;
    if (mdi_icon == "mdi:numeric-3-box-multiple") return MY_ICON_NUMERIC_3_BOX_MULTIPLE;
    if (mdi_icon == "mdi:numeric-4-box-multiple") return MY_ICON_NUMERIC_4_BOX_MULTIPLE;
    if (mdi_icon == "mdi:numeric-5-box-multiple") return MY_ICON_NUMERIC_5_BOX_MULTIPLE;
    if (mdi_icon == "mdi:numeric-6-box-multiple") return MY_ICON_NUMERIC_6_BOX_MULTIPLE;
    if (mdi_icon == "mdi:numeric-7-box-multiple") return MY_ICON_NUMERIC_7_BOX_MULTIPLE;
    if (mdi_icon == "mdi:numeric-8-box-multiple") return MY_ICON_NUMERIC_8_BOX_MULTIPLE;
    if (mdi_icon == "mdi:numeric-9-box-multiple") return MY_ICON_NUMERIC_9_BOX_MULTIPLE;
    if (mdi_icon == "mdi:numeric-10-box-multiple") return MY_ICON_NUMERIC_10_BOX_MULTIPLE;
    if (mdi_icon == "mdi:numeric-0-box-outline") return MY_ICON_NUMERIC_0_BOX_OUTLINE;
    if (mdi_icon == "mdi:numeric-1-box-outline") return MY_ICON_NUMERIC_1_BOX_OUTLINE;
    if (mdi_icon == "mdi:numeric-2-box-outline") return MY_ICON_NUMERIC_2_BOX_OUTLINE;
    if (mdi_icon == "mdi:numeric-3-box-outline") return MY_ICON_NUMERIC_3_BOX_OUTLINE;
    if (mdi_icon == "mdi:numeric-4-box-outline") return MY_ICON_NUMERIC_4_BOX_OUTLINE;
    if (mdi_icon == "mdi:numeric-5-box-outline") return MY_ICON_NUMERIC_5_BOX_OUTLINE;
    if (mdi_icon == "mdi:numeric-6-box-outline") return MY_ICON_NUMERIC_6_BOX_OUTLINE;
    if (mdi_icon == "mdi:numeric-7-box-outline") return MY_ICON_NUMERIC_7_BOX_OUTLINE;
    if (mdi_icon == "mdi:numeric-8-box-outline") return MY_ICON_NUMERIC_8_BOX_OUTLINE;
    if (mdi_icon == "mdi:numeric-9-box-outline") return MY_ICON_NUMERIC_9_BOX_OUTLINE;
    if (mdi_icon == "mdi:numeric-10-box-outline") return MY_ICON_NUMERIC_10_BOX_OUTLINE;
    if (mdi_icon == "mdi:numeric-0-box") return MY_ICON_NUMERIC_0_BOX;
    if (mdi_icon == "mdi:numeric-1-box") return MY_ICON_NUMERIC_1_BOX;
    if (mdi_icon == "mdi:numeric-2-box") return MY_ICON_NUMERIC_2_BOX;
    if (mdi_icon == "mdi:numeric-3-box") return MY_ICON_NUMERIC_3_BOX;
    if (mdi_icon == "mdi:numeric-4-box") return MY_ICON_NUMERIC_4_BOX;
    if (mdi_icon == "mdi:numeric-5-box") return MY_ICON_NUMERIC_5_BOX;
    if (mdi_icon == "mdi:numeric-6-box") return MY_ICON_NUMERIC_6_BOX;
    if (mdi_icon == "mdi:numeric-7-box") return MY_ICON_NUMERIC_7_BOX;
    if (mdi_icon == "mdi:numeric-8-box") return MY_ICON_NUMERIC_8_BOX;
    if (mdi_icon == "mdi:numeric-9-box") return MY_ICON_NUMERIC_9_BOX;
    if (mdi_icon == "mdi:numeric-10-box") return MY_ICON_NUMERIC_10_BOX;
    if (mdi_icon == "mdi:numeric-0-circle-outline") return MY_ICON_NUMERIC_0_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-1-circle-outline") return MY_ICON_NUMERIC_1_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-2-circle-outline") return MY_ICON_NUMERIC_2_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-3-circle-outline") return MY_ICON_NUMERIC_3_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-4-circle-outline") return MY_ICON_NUMERIC_4_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-5-circle-outline") return MY_ICON_NUMERIC_5_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-6-circle-outline") return MY_ICON_NUMERIC_6_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-7-circle-outline") return MY_ICON_NUMERIC_7_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-8-circle-outline") return MY_ICON_NUMERIC_8_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-9-circle-outline") return MY_ICON_NUMERIC_9_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-10-circle-outline") return MY_ICON_NUMERIC_10_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-0-circle") return MY_ICON_NUMERIC_0_CIRCLE;
    if (mdi_icon == "mdi:numeric-1-circle") return MY_ICON_NUMERIC_1_CIRCLE;
    if (mdi_icon == "mdi:numeric-2-circle") return MY_ICON_NUMERIC_2_CIRCLE;
    if (mdi_icon == "mdi:numeric-3-circle") return MY_ICON_NUMERIC_3_CIRCLE;
    if (mdi_icon == "mdi:numeric-4-circle") return MY_ICON_NUMERIC_4_CIRCLE;
    if (mdi_icon == "mdi:numeric-5-circle") return MY_ICON_NUMERIC_5_CIRCLE;
    if (mdi_icon == "mdi:numeric-6-circle") return MY_ICON_NUMERIC_6_CIRCLE;
    if (mdi_icon == "mdi:numeric-7-circle") return MY_ICON_NUMERIC_7_CIRCLE;
    if (mdi_icon == "mdi:numeric-8-circle") return MY_ICON_NUMERIC_8_CIRCLE;
    if (mdi_icon == "mdi:numeric-9-circle") return MY_ICON_NUMERIC_9_CIRCLE;
    if (mdi_icon == "mdi:numeric-10-circle") return MY_ICON_NUMERIC_10_CIRCLE;
    if (mdi_icon == "mdi:numeric-negative-1") return MY_ICON_NUMERIC_NEGATIVE_1;
    if (mdi_icon == "mdi:numeric-off") return MY_ICON_NUMERIC_OFF;
    if (mdi_icon == "mdi:numeric-9-plus-box-multiple-outline") return MY_ICON_NUMERIC_9_PLUS_BOX_MULTIPLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-9-plus-box-multiple") return MY_ICON_NUMERIC_9_PLUS_BOX_MULTIPLE;
    if (mdi_icon == "mdi:numeric-9-plus-box-outline") return MY_ICON_NUMERIC_9_PLUS_BOX_OUTLINE;
    if (mdi_icon == "mdi:numeric-9-plus-box") return MY_ICON_NUMERIC_9_PLUS_BOX;
    if (mdi_icon == "mdi:numeric-9-plus-circle-outline") return MY_ICON_NUMERIC_9_PLUS_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:numeric-9-plus-circle") return MY_ICON_NUMERIC_9_PLUS_CIRCLE;
    if (mdi_icon == "mdi:numeric-9-plus") return MY_ICON_NUMERIC_9_PLUS;
    if (mdi_icon == "mdi:numeric-positive-1") return MY_ICON_NUMERIC_POSITIVE_1;
    if (mdi_icon == "mdi:numeric") return MY_ICON_NUMERIC;
    if (mdi_icon == "mdi:numeric-0") return MY_ICON_NUMERIC_0;
    if (mdi_icon == "mdi:numeric-1") return MY_ICON_NUMERIC_1;
    if (mdi_icon == "mdi:numeric-2") return MY_ICON_NUMERIC_2;
    if (mdi_icon == "mdi:numeric-3") return MY_ICON_NUMERIC_3;
    if (mdi_icon == "mdi:numeric-4") return MY_ICON_NUMERIC_4;
    if (mdi_icon == "mdi:numeric-5") return MY_ICON_NUMERIC_5;
    if (mdi_icon == "mdi:numeric-6") return MY_ICON_NUMERIC_6;
    if (mdi_icon == "mdi:numeric-7") return MY_ICON_NUMERIC_7;
    if (mdi_icon == "mdi:numeric-8") return MY_ICON_NUMERIC_8;
    if (mdi_icon == "mdi:numeric-9") return MY_ICON_NUMERIC_9;
    if (mdi_icon == "mdi:numeric-10") return MY_ICON_NUMERIC_10;
    if (mdi_icon == "mdi:order-alphabetical-ascending") return MY_ICON_ORDER_ALPHABETICAL_ASCENDING;
    if (mdi_icon == "mdi:order-alphabetical-descending") return MY_ICON_ORDER_ALPHABETICAL_DESCENDING;
    if (mdi_icon == "mdi:order-numeric-ascending") return MY_ICON_ORDER_NUMERIC_ASCENDING;
    if (mdi_icon == "mdi:order-numeric-descending") return MY_ICON_ORDER_NUMERIC_DESCENDING;
    if (mdi_icon == "mdi:package") return MY_ICON_PACKAGE;
    if (mdi_icon == "mdi:parking") return MY_ICON_PARKING;
    if (mdi_icon == "mdi:pause-circle-outline") return MY_ICON_PAUSE_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:pause-circle") return MY_ICON_PAUSE_CIRCLE;
    if (mdi_icon == "mdi:pause") return MY_ICON_PAUSE;
    if (mdi_icon == "mdi:pencil-circle-outline") return MY_ICON_PENCIL_CIRCLE_OUTLINE;
    if (mdi_icon == "mdi:pencil-circle") return MY_ICON_PENCIL_CIRCLE;
    if (mdi_icon == "mdi:pencil") return MY_ICON_PENCIL;
    if (mdi_icon == "mdi:pig") return MY_ICON_PIG;
    if (mdi_icon == "mdi:pine-tree") return MY_ICON_PINE_TREE;
    if (mdi_icon == "mdi:play") return MY_ICON_PLAY;
    if (mdi_icon == "mdi:plus-thick") return MY_ICON_PLUS_THICK;
    if (mdi_icon == "mdi:plus") return MY_ICON_PLUS;
    if (mdi_icon == "mdi:pool") return MY_ICON_POOL;
    if (mdi_icon == "mdi:post-lamp") return MY_ICON_POST_LAMP;
    if (mdi_icon == "mdi:power") return MY_ICON_POWER;
    if (mdi_icon == "mdi:rabbit") return MY_ICON_RABBIT;
    if (mdi_icon == "mdi:radiator") return MY_ICON_RADIATOR;
    if (mdi_icon == "mdi:refresh") return MY_ICON_REFRESH;
    if (mdi_icon == "mdi:remote") return MY_ICON_REMOTE;
    if (mdi_icon == "mdi:repeat") return MY_ICON_REPEAT;
    if (mdi_icon == "mdi:rewind") return MY_ICON_REWIND;
    if (mdi_icon == "mdi:road") return MY_ICON_ROAD;
    if (mdi_icon == "mdi:robot-vacuum") return MY_ICON_ROBOT_VACUUM;
    if (mdi_icon == "mdi:roller-shade") return MY_ICON_ROLLER_SHADE;
    if (mdi_icon == "mdi:router-wireless") return MY_ICON_ROUTER_WIRELESS;
    if (mdi_icon == "mdi:scooter") return MY_ICON_SCOOTER;
    if (mdi_icon == "mdi:screwdriver") return MY_ICON_SCREWDRIVER;
    if (mdi_icon == "mdi:security") return MY_ICON_SECURITY;
    if (mdi_icon == "mdi:server") return MY_ICON_SERVER;
    if (mdi_icon == "mdi:shield-check") return MY_ICON_SHIELD_CHECK;
    if (mdi_icon == "mdi:shield") return MY_ICON_SHIELD;
    if (mdi_icon == "mdi:shower") return MY_ICON_SHOWER;
    if (mdi_icon == "mdi:shuffle") return MY_ICON_SHUFFLE;
    if (mdi_icon == "mdi:sign-direction") return MY_ICON_SIGN_DIRECTION;
    if (mdi_icon == "mdi:silverware") return MY_ICON_SILVERWARE;
    if (mdi_icon == "mdi:smoke-detector") return MY_ICON_SMOKE_DETECTOR;
    if (mdi_icon == "mdi:snowflake") return MY_ICON_SNOWFLAKE;
    if (mdi_icon == "mdi:sofa-outline") return MY_ICON_SOFA_OUTLINE;
    if (mdi_icon == "mdi:sofa") return MY_ICON_SOFA;
    if (mdi_icon == "mdi:solar-panel") return MY_ICON_SOLAR_PANEL;
    if (mdi_icon == "mdi:solar-power") return MY_ICON_SOLAR_POWER;
    if (mdi_icon == "mdi:sort-alphabetical-ascending-variant") return MY_ICON_SORT_ALPHABETICAL_ASCENDING_VARIANT;
    if (mdi_icon == "mdi:sort-alphabetical-ascending") return MY_ICON_SORT_ALPHABETICAL_ASCENDING;
    if (mdi_icon == "mdi:sort-alphabetical-descending-variant") return MY_ICON_SORT_ALPHABETICAL_DESCENDING_VARIANT;
    if (mdi_icon == "mdi:sort-alphabetical-descending") return MY_ICON_SORT_ALPHABETICAL_DESCENDING;
    if (mdi_icon == "mdi:sort-alphabetical-variant") return MY_ICON_SORT_ALPHABETICAL_VARIANT;
    if (mdi_icon == "mdi:sort-numeric-ascending-variant") return MY_ICON_SORT_NUMERIC_ASCENDING_VARIANT;
    if (mdi_icon == "mdi:sort-numeric-ascending") return MY_ICON_SORT_NUMERIC_ASCENDING;
    if (mdi_icon == "mdi:sort-numeric-descending-variant") return MY_ICON_SORT_NUMERIC_DESCENDING_VARIANT;
    if (mdi_icon == "mdi:sort-numeric-descending") return MY_ICON_SORT_NUMERIC_DESCENDING;
    if (mdi_icon == "mdi:sort-numeric-variant") return MY_ICON_SORT_NUMERIC_VARIANT;
    if (mdi_icon == "mdi:speaker") return MY_ICON_SPEAKER;
    if (mdi_icon == "mdi:spotify") return MY_ICON_SPOTIFY;
    if (mdi_icon == "mdi:spotlight") return MY_ICON_SPOTLIGHT;
    if (mdi_icon == "mdi:stairs") return MY_ICON_STAIRS;
    if (mdi_icon == "mdi:stop") return MY_ICON_STOP;
    if (mdi_icon == "mdi:stove") return MY_ICON_STOVE;
    if (mdi_icon == "mdi:table-furniture") return MY_ICON_TABLE_FURNITURE;
    if (mdi_icon == "mdi:table") return MY_ICON_TABLE;
    if (mdi_icon == "mdi:television-classic") return MY_ICON_TELEVISION_CLASSIC;
    if (mdi_icon == "mdi:television") return MY_ICON_TELEVISION;
    if (mdi_icon == "mdi:thermometer-chevron-down") return MY_ICON_THERMOMETER_CHEVRON_DOWN;
    if (mdi_icon == "mdi:thermometer-chevron-up") return MY_ICON_THERMOMETER_CHEVRON_UP;
    if (mdi_icon == "mdi:thermometer-lines") return MY_ICON_THERMOMETER_LINES;
    if (mdi_icon == "mdi:thermometer") return MY_ICON_THERMOMETER;
    if (mdi_icon == "mdi:thermostat") return MY_ICON_THERMOSTAT;
    if (mdi_icon == "mdi:toaster") return MY_ICON_TOASTER;
    if (mdi_icon == "mdi:toggle-switch-off-outline") return MY_ICON_TOGGLE_SWITCH_OFF_OUTLINE;
    if (mdi_icon == "mdi:toggle-switch-off") return MY_ICON_TOGGLE_SWITCH_OFF;
    if (mdi_icon == "mdi:toggle-switch-outline") return MY_ICON_TOGGLE_SWITCH_OUTLINE;
    if (mdi_icon == "mdi:toggle-switch-variant-off") return MY_ICON_TOGGLE_SWITCH_VARIANT_OFF;
    if (mdi_icon == "mdi:toggle-switch-variant") return MY_ICON_TOGGLE_SWITCH_VARIANT;
    if (mdi_icon == "mdi:toggle-switch") return MY_ICON_TOGGLE_SWITCH;
    if (mdi_icon == "mdi:toilet") return MY_ICON_TOILET;
    if (mdi_icon == "mdi:toolbox") return MY_ICON_TOOLBOX;
    if (mdi_icon == "mdi:train") return MY_ICON_TRAIN;
    if (mdi_icon == "mdi:transmission-tower") return MY_ICON_TRANSMISSION_TOWER;
    if (mdi_icon == "mdi:trash-can") return MY_ICON_TRASH_CAN;
    if (mdi_icon == "mdi:tree") return MY_ICON_TREE;
    if (mdi_icon == "mdi:truck") return MY_ICON_TRUCK;
    if (mdi_icon == "mdi:tumble-dryer") return MY_ICON_TUMBLE_DRYER;
    if (mdi_icon == "mdi:umbrella-outline") return MY_ICON_UMBRELLA_OUTLINE;
    if (mdi_icon == "mdi:umbrella") return MY_ICON_UMBRELLA;
    if (mdi_icon == "mdi:undo") return MY_ICON_UNDO;
    if (mdi_icon == "mdi:upload") return MY_ICON_UPLOAD;
    if (mdi_icon == "mdi:vacuum") return MY_ICON_VACUUM;
    if (mdi_icon == "mdi:vanity-light") return MY_ICON_VANITY_LIGHT;
    if (mdi_icon == "mdi:video-box") return MY_ICON_VIDEO_BOX;
    if (mdi_icon == "mdi:video") return MY_ICON_VIDEO;
    if (mdi_icon == "mdi:volume-high") return MY_ICON_VOLUME_HIGH;
    if (mdi_icon == "mdi:volume-medium") return MY_ICON_VOLUME_MEDIUM;
    if (mdi_icon == "mdi:volume-minus") return MY_ICON_VOLUME_MINUS;
    if (mdi_icon == "mdi:volume-off") return MY_ICON_VOLUME_OFF;
    if (mdi_icon == "mdi:volume-plus") return MY_ICON_VOLUME_PLUS;
    if (mdi_icon == "mdi:wall-sconce") return MY_ICON_WALL_SCONCE;
    if (mdi_icon == "mdi:wardrobe") return MY_ICON_WARDROBE;
    if (mdi_icon == "mdi:washing-machine") return MY_ICON_WASHING_MACHINE;
    if (mdi_icon == "mdi:water-boiler") return MY_ICON_WATER_BOILER;
    if (mdi_icon == "mdi:water-circle") return MY_ICON_WATER_CIRCLE;
    if (mdi_icon == "mdi:water-percent") return MY_ICON_WATER_PERCENT;
    if (mdi_icon == "mdi:watering-can") return MY_ICON_WATERING_CAN;
    if (mdi_icon == "mdi:water") return MY_ICON_WATER;
    if (mdi_icon == "mdi:weather-cloudy") return MY_ICON_WEATHER_CLOUDY;
    if (mdi_icon == "mdi:weather-lightning") return MY_ICON_WEATHER_LIGHTNING;
    if (mdi_icon == "mdi:weather-night") return MY_ICON_WEATHER_NIGHT;
    if (mdi_icon == "mdi:weather-partly-cloudy") return MY_ICON_WEATHER_PARTLY_CLOUDY;
    if (mdi_icon == "mdi:weather-pouring") return MY_ICON_WEATHER_POURING;
    if (mdi_icon == "mdi:weather-rainy") return MY_ICON_WEATHER_RAINY;
    if (mdi_icon == "mdi:weather-snowy") return MY_ICON_WEATHER_SNOWY;
    if (mdi_icon == "mdi:weather-sunny") return MY_ICON_WEATHER_SUNNY;
    if (mdi_icon == "mdi:weather-windy") return MY_ICON_WEATHER_WINDY;
    if (mdi_icon == "mdi:wifi-strength-2") return MY_ICON_WIFI_STRENGTH_2;
    if (mdi_icon == "mdi:wifi") return MY_ICON_WIFI;
    if (mdi_icon == "mdi:wind-turbine") return MY_ICON_WIND_TURBINE;
    if (mdi_icon == "mdi:window-closed") return MY_ICON_WINDOW_CLOSED;
    if (mdi_icon == "mdi:window-close") return MY_ICON_WINDOW_CLOSE;
    if (mdi_icon == "mdi:window-open") return MY_ICON_WINDOW_OPEN;
    if (mdi_icon == "mdi:wrap-disabled") return MY_ICON_WRAP_DISABLED;
    if (mdi_icon == "mdi:wrench") return MY_ICON_WRENCH;
    if (mdi_icon == "mdi:youtube-studio") return MY_ICON_YOUTUBE_STUDIO;
    if (mdi_icon == "mdi:youtube-subscription") return MY_ICON_YOUTUBE_SUBSCRIPTION;
    if (mdi_icon == "mdi:youtube-tv") return MY_ICON_YOUTUBE_TV;
    if (mdi_icon == "mdi:youtube") return MY_ICON_YOUTUBE;
    return "";
}
