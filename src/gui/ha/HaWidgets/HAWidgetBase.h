#pragma once
#include <Arduino.h>
#include <time.h> 
#include <vector>
#include "LVGL_Driver.h"
#include "HaConfigLogic.h" // HACondition, HAWidgetDef
#include "HaWidgetKind.h"

String formatEntityName(String entity_id, String name);
String getSafeIcon(String mdi);
String getIconForEntity(String entity_id, String mdi_icon);

class HAWidget {
protected:
    int tab_index;      
    String type;          // Typ wie in der Konfiguration ("light", ...)
    HaWidgetKind kind;    // derselbe Typ als Aufzaehlung (fuer Vergleiche)
    String entity_id;
    String widget_name;  
    String mdi_icon;     
    String color_on;     
    String color_off;    
    
    int icon_align;
    int text_align;
    int state_align; 
    
    int icon_margin; 
    int text_margin; 
    int state_margin; 
    
    bool snap_to_grid; 

    bool show_chart = false;
    int chart_w_pct = 60;
    int chart_h_pct = 40;
    int chart_x_ofs = 10;
    int chart_y_ofs = 10;
    String chart_min = "";
    String chart_max = "";

    String tap_domain = "";
    String tap_service = "";
    String tap_target = "";

    String baked_options = "";
    float baked_min = 0.0f;
    float baked_max = 100.0f;
    float baked_step = 1.0f;

    // Sichtbarkeitsbedingungen
    String conditions_type = "AND";
    std::vector<HACondition> conditions;

    // Kind eines Ordners: Sichtbarkeit steuert der Ordner, nicht das Widget.
    bool in_folder = false;

    // Im Edit-Modus per Muelleimer geloescht. Wird beim Speichern uebergangen.
    // Eigener Merker, weil HIDDEN jetzt auch "Bedingung nicht erfuellt" heisst.
    bool marked_deleted = false;

    lv_obj_t* icon_label;
    lv_obj_t* name_label;
    String current_state;

    static void widget_event_cb(lv_event_t * e);

    // Blendet das Widget gemaess seiner eigenen Bedingungen ein oder aus.
    // Rueckgabe: true, wenn das Widget sichtbar sein soll.
    //   - geloescht            -> false
    //   - Ordnerkind / keine Bedingungen -> true, Sichtbarkeit unveraendert
    //   - Edit-Modus           -> immer sichtbar (sonst nicht bearbeitbar)
    bool applyOwnVisibility();

    // --- Gemeinsame Helfer fuer alle Widget-Typen ---
    // Konfigurierte Farbe "#RRGGBB" als 0xRRGGBB, sonst 'fallback'.
    static uint32_t parseColor(const String& hex, uint32_t fallback);
    uint32_t colorOnOr(uint32_t fallback) const  { return parseColor(color_on, fallback); }
    uint32_t colorOffOr(uint32_t fallback) const { return parseColor(color_off, fallback); }

    // Fuehrt die konfigurierte Klick-Aktion aus (Domain/Service/Ziel aus dem
    // Bearbeiten-Dialog). false, wenn keine konfiguriert ist.
    bool runTapAction();

    // Optionen fuer Select/Klima: live aus HA, sonst die beim Anlegen
    // gespeicherten ("gebackenen") Werte.
    std::vector<String> optionsOrBaked();

    // --- Bedienung vs. Serverwerte ---
    // Nach einer Bedienung (Taste, Regler, Auswahl) meldet HA zunaechst noch
    // den alten Wert zurueck, erst danach den neuen. Ohne Sperre sprang die
    // Anzeige kurz zurueck, und bei mehrfachem "+" wurde vom alten Wert aus
    // weitergezaehlt. Waehrend der Sperre ignoriert updateState() Serverwerte.
    uint32_t hold_remote_until = 0;
    bool     refresh_after_hold = false;
    void holdRemoteUpdates(uint32_t ms = 2500);
    bool remoteUpdatesHeld() const;

public:
    lv_obj_t* container;
    static bool editModeActive;
    
    static void (*onEditRequested)(HAWidget*);
    static void (*onDeleteRequested)(HAWidget*);
    static void (*onLightControlRequested)(HAWidget* w); 
    static void (*onMediaControlRequested)(HAWidget* w); 
    static void (*onVacuumControlRequested)(HAWidget* w); 

    // Alle lebenden Widgets (auch Ordnerkinder) und ihre Bedingungspruefung
    static std::vector<HAWidget*> all_active_widgets;
    static void checkAllConditions();
    virtual void checkConditions();

    // Wertet eine Bedingungsliste gegen die aktuellen Zustaende aus.
    // Gemeinsam fuer normale Widgets und Ordner. Leere Liste = erfuellt.
    // Numerische Vergleiche gelten nur, wenn BEIDE Seiten echte Zahlen sind:
    // "unavailable" < 5 ist damit falsch statt (wie frueher) wahr.
    static bool evaluateConditions(const String& type, const std::vector<HACondition>& conds);

    // Liest eine Zahl streng: die ganze Zeichenkette muss eine Zahl sein
    // ("unavailable", "" oder "12 W" liefern false). String::toFloat() gab
    // hier still 0 zurueck.
    static bool parseNumber(const String& s, float& out);

    // true genau einmal, wenn eine Sperre (holdRemoteUpdates) abgelaufen ist.
    // Die View ruft dann updateState() erneut auf, damit der aktuelle
    // Serverwert sichtbar wird, auch wenn sich seitdem nichts geaendert hat.
    bool refreshAfterHoldDue();

    void setInFolder(bool v) { in_folder = v; }
    bool isInFolder() const { return in_folder; }

    // Loeschen im Edit-Modus: ausblenden und fuer das Speichern markieren.
    void markDeleted();
    bool isMarkedDeleted() const { return marked_deleted; }

    // Senkrechtes Wischen auf einer Kachel scrollt im Normalbetrieb den Tab.
    // Im Edit-Modus wuerde das mit dem Verschieben kollidieren - dort wird
    // die Scroll-Kette gekappt. Nach jedem Wechsel von editModeActive aufrufen.
    static void applyEditModeScrollChain();

    HAWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off);
    virtual ~HAWidget();

    virtual void setAlignments(int i_align, int t_align, int s_align, int i_margin, int t_margin, int s_margin);
    void setName(String n);
    void setMdiIcon(String mdi);
    void setColors(String on, String off);
    void setSize(int w, int h);
    void setPosition(int x, int y, bool snap);
    void setSnapToGrid(bool snap);
    
    void setTapAction(String domain, String service, String target);
    String getTapDomain();
    String getTapService();
    String getTapTarget();

    void setBakedOptions(String opts) { baked_options = opts; }
    String getBakedOptions() { return baked_options; }
    void setBakedLimits(float mn, float mx, float stp) { baked_min = mn; baked_max = mx; baked_step = stp; }
    float getBakedMin() { return baked_min; }
    float getBakedMax() { return baked_max; }
    float getBakedStep() { return baked_step; }

    // Bedingungen
    void setConditions(String type, std::vector<HACondition> conds) { conditions_type = type; conditions = conds; }
    String getConditionsType() { return conditions_type; }
    const std::vector<HACondition>& getConditions() const { return conditions; }
    
    virtual void setChartConfig(bool show, int w_p, int h_p, int x_ofs, int y_ofs, String c_min, String c_max);

    String getEntityId();
    String getType();
    HaWidgetKind getKind() const { return kind; }
    String getName();
    String getMdiIcon();
    String getColorOn();
    String getColorOff();
    int getX();
    int getY();
    int getW();
    int getH();
    int getTabIndex();
    
    int getIconAlign();
    int getTextAlign();
    int getStateAlign();
    int getIconMargin();
    int getTextMargin();
    int getStateMargin();
    bool getSnapToGrid();
    
    virtual bool getShowChart();
    virtual int getChartWPct();
    virtual int getChartHPct();
    virtual int getChartXOfs();
    virtual int getChartYOfs();
    virtual String getChartMin();
    virtual String getChartMax();
    virtual String getMediaContentType();
    virtual String getMediaContentId();

    // Container (u. a. fuer den Ordner, der Kinder darin anlegt)
    lv_obj_t* getContainer() { return container; }

    virtual void updateState(String state);
    virtual void onClick() = 0; 
};

class HALightWidget : public HAWidget {
public:
    HALightWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    void updateState(String state) override;
    void onClick() override;
};

class HAActionWidget : public HAWidget {
public:
    HAActionWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    void updateState(String state) override;
    void onClick() override;
};