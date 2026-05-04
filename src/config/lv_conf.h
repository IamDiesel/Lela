/**
 * @file lv_conf.h
 * Configuration file for v8.4.0
 */

#if 1 /* Set this to "1" to enable content */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/*=========================
   MEMORY SETTINGS
 *=========================*/
/* 1: use custom malloc/free, 0: use the built-in `lv_mem_alloc()` and `lv_mem_free()` */
#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM == 0
    #define LV_MEM_SIZE (128U * 1024U)          /* [bytes] */
    #define LV_MEM_ADR 0
    #define LV_MEM_POOL_EXPAND_SIZE 0
#else       /*LV_MEM_CUSTOM*/
    #include <esp_heap_caps.h> // WICHTIG: Erlaubt uns die PSRAM-Steuerung
    #define LV_MEM_CUSTOM_ALLOC(size)   heap_caps_malloc(size, MALLOC_CAP_SPIRAM)
    #define LV_MEM_CUSTOM_FREE          heap_caps_free
    #define LV_MEM_CUSTOM_REALLOC(ptr, size) heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM)
#endif     /*LV_MEM_CUSTOM*/

/* Number of the intermediate memory buffer used during rendering and other internal processing mechanisms.
 * You will see an error log message if there wasn't enough buffers. */
#define LV_MEM_BUF_MAX_NUM 16

/*====================
   HAL SETTINGS
 *====================*/
#define LV_DISP_DEF_REFR_PERIOD  30      /* [ms] */
#define LV_INDEV_DEF_READ_PERIOD 30      /* [ms] */
#define LV_TICK_CUSTOM 0

/*=======================
 * FEATURE CONFIGURATION
 *=======================*/
#define LV_USE_DRAW_MASKS 1
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_USER_DATA 1

/*=====================
 * COMPILER SETTINGS
 *====================*/
#define LV_ATTRIBUTE_TICK_INC
#define LV_ATTRIBUTE_TIMER_HANDLER
#define LV_ATTRIBUTE_FLUSH_READY
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE 1
#define LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY
#define LV_ATTRIBUTE_FAST_MEM
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning

/*==================
 * FONT USAGE
 *===================*/
/* Montserrat fonts with ASCII range and some symbols using bpp = 4 */
#define LV_FONT_FMT_TXT_LARGE 1
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 1

#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_USE_FONT_PLACEHOLDER 1

/*=================
 * TEXT SETTINGS
 *=================*/
#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS " ,.;:-_)]}"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3
#define LV_TXT_COLOR_CMD "#"
#define LV_USE_BIDI 0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*==================
 * WIDGETS
 *================*/
#define LV_USE_ARC        1
#define LV_USE_ANIMIMG    1
#define LV_USE_BAR        1
#define LV_USE_BUTTON        1
#define LV_USE_BUTTONMATRIX  1
#define LV_USE_CALENDAR   1
#define LV_USE_CANVAS     1
#define LV_USE_CHART      1
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMAGE      1
#define LV_USE_IMAGEBUTTON     1
#define LV_USE_KEYBOARD   1
#define LV_USE_LABEL      1
#define LV_LABEL_TEXT_SELECTION 1
#define LV_LABEL_LONG_TXT_HINT 1
#define LV_USE_LED        1
#define LV_USE_LINE       1
#define LV_USE_LIST       1
#define LV_USE_MENU       1
#define LV_USE_METER      1
#define LV_USE_MSGBOX     1
#define LV_USE_ROLLER     1
#define LV_USE_SLIDER     1
#define LV_USE_SPAN       1
#define LV_USE_SPINBOX    1
#define LV_USE_SPINNER    1
#define LV_USE_SWITCH     1
#define LV_USE_TABLE      1
#define LV_USE_TABVIEW    1
#define LV_USE_TEXTAREA   1
#define LV_USE_TILEVIEW   1
#define LV_USE_WIN        1

/*==================
 * THEMES
 *==================*/
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 0
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80
#define LV_USE_THEME_BASIC 1

/*==================
 * LAYOUTS
 *==================*/
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/*====================
 * 3RD PARTS LIBRARIES
 *====================*/
#define LV_USE_FS_STDIO 0
#define LV_USE_FS_POSIX 0
#define LV_USE_FS_WIN32 0
#define LV_USE_FS_FATFS 0
#define LV_USE_PNG 0
#define LV_USE_BMP 0
#define LV_USE_SJPG 0
#define LV_USE_GIF 0
#define LV_USE_QRCODE 1
#define LV_USE_FREETYPE 0
#define LV_USE_RLOTTIE 0
#define LV_USE_FFMPEG 0

/*==================
 * OTHERS
 *==================*/
#define LV_USE_SNAPSHOT 1

#endif /*LV_CONF_H*/
#endif /*End of "Content enable"*/