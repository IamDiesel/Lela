#include "HaLovelaceParser.h"
#include "HaWebsocketLogic.h"
#include "HaConfigLogic.h"
#include "ViewHomeAssistant.h"
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <functional>

class ParserRamAllocator : public ArduinoJson::Allocator {
public:
    void* allocate(size_t size) override { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
    void deallocate(void* pointer) override { heap_caps_free(pointer); }
    void* reallocate(void* pointer, size_t new_size) override { return heap_caps_realloc(pointer, new_size, MALLOC_CAP_SPIRAM); }
};
static ParserRamAllocator parserAlloc;

void HaLovelaceParser::parseDashboardList(const String& payload) {
    JsonDocument filter(&parserAlloc);
    filter["result"] = true;
    JsonDocument doc(&parserAlloc);
    if (deserializeJson(doc, payload, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(200))) {
        pendingImportError = true; importErrorMessage = "Fehler beim Lesen\nder Dashboard-Liste."; return;
    }

    availableDashboardUrls.clear(); availableDashboardTitles.clear();
    availableDashboardTitles.push_back("Standard (Overview)"); availableDashboardUrls.push_back("");

    JsonObject panels = doc["result"];
    for (JsonPair kv : panels) {
        JsonObject panel = kv.value();
        if (String(panel["component_name"] | "") == "lovelace") {
            String url = panel["url_path"].as<String>();
            String title = panel["title"].as<String>();
            if (title.length() == 0) title = url;
            if (url != "lovelace" && url != "") { availableDashboardUrls.push_back(url); availableDashboardTitles.push_back(title); }
        }
    }
    pendingDashboardList = true; 
}

void HaLovelaceParser::parseViewsList(const String& payload) {
    availableViewTitles.clear();
    JsonDocument filter(&parserAlloc);
    filter["result"]["views"][0]["title"] = true; filter["result"]["views"][0]["path"] = true; filter["result"]["views"][0]["icon"] = true;
    filter["result"]["result"]["views"][0]["title"] = true; filter["result"]["result"]["views"][0]["path"] = true;
    
    JsonDocument doc(&parserAlloc);
    if (!deserializeJson(doc, payload, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(200))) {
        JsonVariant views = doc["result"]["views"];
        if (views.isNull()) views = doc["result"]["result"]["views"];
        
        if (views.is<JsonArray>()) {
            int idx = 0;
            for (JsonVariant v : views.as<JsonArray>()) {
                String title = v["title"] | "";
                if (title.length() == 0) title = v["path"] | "";
                if (title.length() == 0) title = v["icon"] | "";
                if (title.length() == 0) title = "Tab " + String(idx + 1);
                availableViewTitles.push_back(title); idx++;
            }
        }
    }
    if (availableViewTitles.empty()) availableViewTitles.push_back("Standard-Ansicht");
    pendingViewList = true;
}

void HaLovelaceParser::parseCards(const String& payload, int targetViewIndex, int currentImportTab) {
    int margin = 20, cols = 6, cardW = 160, cardH = 160, i = 0;
    std::vector<String> foundEntities; 

    JsonDocument filter(&parserAlloc);
    filter["result"]["views"][0]["cards"] = true; 
    filter["result"]["views"][0]["sections"] = true; 
    filter["result"]["result"]["views"][0]["cards"] = true; 
    filter["result"]["result"]["views"][0]["sections"] = true; 

    JsonDocument viewDoc(&parserAlloc);
    DeserializationError viewErr = deserializeJson(viewDoc, payload, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(200));

    if (viewErr) {
        importErrorMessage = String("JSON Fehler:\n") + viewErr.c_str();
        pendingImportError = true; isImporting = false; return;
    }

    JsonVariant views = viewDoc["result"]["views"];
    if (views.isNull()) views = viewDoc["result"]["result"]["views"];

    if (!views.is<JsonArray>() || targetViewIndex >= views.as<JsonArray>().size()) {
        importErrorMessage = "Keine Karten in\ndieser Ansicht gefunden.";
        pendingImportError = true; isImporting = false; return;
    }

    auto addWidget = [&](String entity, String customName, String customIcon, String ta_dom, String ta_srv, String ta_tgt) {
        if (entity.length() == 0 || entity.indexOf('.') == -1) return;
        if (std::find(foundEntities.begin(), foundEntities.end(), entity) != foundEntities.end()) return;
        foundEntities.push_back(entity);
        
        String widgetType = "light";
        int currentCardW = cardW;
        int itemsToAdvance = 1;

        if (entity.startsWith("sensor.") || entity.startsWith("binary_sensor.") || entity.startsWith("weather.") || entity.startsWith("sun.")) {
            widgetType = "sensor";
        }
        else if (entity.startsWith("button.") || entity.startsWith("input_button.") || entity.startsWith("script.") || entity.startsWith("automation.") || entity.startsWith("scene.")) {
            widgetType = "action";
        }
        else if (entity.startsWith("media_player.")) {
            widgetType = "media_player";
        }
        else if (entity.startsWith("input_select.") || entity.startsWith("select.")) {
            widgetType = "select";
        }
        else if (entity.startsWith("input_number.") || entity.startsWith("number.")) {
            widgetType = "number";
        }
        // --- NEU: Zuweisung fuer Rolllaeden ---
        else if (entity.startsWith("cover.")) {
            widgetType = "cover";
            currentCardW = (cardW * 2) + margin; // Doppelte Breite!
            itemsToAdvance = 2; // Nimmt 2 Spalten auf dem Dashboard ein
        }
        // --- NEU: Zuweisung fuer Klima / Thermostate ---
        else if (entity.startsWith("climate.")) {
            widgetType = "climate";
            currentCardW = (cardW * 2) + margin; // Doppelte Breite
            itemsToAdvance = 2; 
        }
        // --- NEU: Text-Eingaben erfassen ---
        else if (entity.startsWith("text.") || entity.startsWith("input_text.")) {
            widgetType = "text";
            currentCardW = (cardW * 2) + margin; // Doppelte Breite fuer gute Lesbarkeit!
            itemsToAdvance = 2; 
        }
        else if (entity.startsWith("vacuum.")) {
            widgetType = "vacuum";
            currentCardW = (cardW * 2) + margin;
            itemsToAdvance = 2; 
        }

        if (customIcon.length() == 0) customIcon = HaWebsocketLogic_GetCachedIcon(entity);
        if (customName.length() == 0) customName = HaWebsocketLogic_GetEntityName(entity);
        
        HAWidgetDef wDef;
        wDef.entity_id = entity; wDef.type = widgetType;
        wDef.x = margin + (i % cols) * (cardW + margin); wDef.y = margin + (i / cols) * (cardH + margin);
        wDef.name = customName; wDef.mdi_icon = customIcon; wDef.w = currentCardW; wDef.h = cardH;
        wDef.icon_align = LV_ALIGN_TOP_MID; wDef.text_align = LV_ALIGN_BOTTOM_MID;
        
        wDef.tap_action_domain = ta_dom;
        wDef.tap_action_service = ta_srv;
        wDef.tap_action_target = ta_tgt;

        // --- NEU: Backe Metadaten beim Import direkt aus dem globalen Speicher ein ---
        // --- ANPASSUNG: Limits und Optionen auch fuer climate einbacken ---
        if (widgetType == "select" || widgetType == "climate") {
            std::vector<String> opts = HaWebsocketLogic_GetGlobalOptions(entity);
            String opt_str = "";
            for (size_t k = 0; k < opts.size(); k++) {
                opt_str += opts[k];
                if (k < opts.size() - 1) opt_str += ",";
            }
            wDef.select_options = opt_str;
        } 
        
        if (widgetType == "number" || widgetType == "climate" || widgetType == "text") {
            wDef.slider_min = HaWebsocketLogic_GetGlobalMin(entity);
            wDef.slider_max = HaWebsocketLogic_GetGlobalMax(entity);
            wDef.slider_step = HaWebsocketLogic_GetGlobalStep(entity);
        }
        // -----------------------------------------------------------------------------
        
        HaConfigLogic::dashboards[currentImportTab].widgets.push_back(wDef); 
        i += itemsToAdvance;
    };

    std::function<void(JsonVariant)> extractCards = [&](JsonVariant node) {
        if (node.is<JsonObject>()) {
            JsonObject obj = node.as<JsonObject>();

            if (obj["entities"].is<JsonArray>()) {
                for (JsonVariant item : obj["entities"].as<JsonArray>()) {
                    if (item.is<JsonObject>()) {
                        JsonObject itemObj = item.as<JsonObject>();
                        addWidget(itemObj["entity"] | "", itemObj["name"] | "", itemObj["icon"] | "", "", "", "");
                    } else { 
                        addWidget(item.as<String>(), "", "", "", "", ""); 
                    }
                }
            }

            String entityId = obj["entity"] | "";
            if (entityId.length() == 0) entityId = obj["vacuum"] | "";
            if (entityId.length() == 0) entityId = obj["camera_image"] | "";
            
            if (entityId.length() > 0) {
                String cName = obj["name"] | ""; String cIcon = obj["icon"] | "";
                String ta_dom = ""; String ta_srv = ""; String ta_tgt = "";
                
                if (obj["tap_action"].is<JsonObject>()) {
                    JsonObject tap = obj["tap_action"].as<JsonObject>();
                    String action = tap["action"] | "";
                    if (action == "call-service" || action == "perform-action") {
                        String serviceFull = tap["service"] | "";
                        if (serviceFull.length() == 0) serviceFull = tap["perform_action"] | "";
                        
                        if (serviceFull.length() > 0) {
                            int dot = serviceFull.indexOf('.');
                            if (dot != -1) {
                                ta_dom = serviceFull.substring(0, dot);
                                ta_srv = serviceFull.substring(dot + 1);
                            }
                        }
                        
                        if (tap["target"]["entity_id"].is<String>()) ta_tgt = tap["target"]["entity_id"].as<String>();
                        else if (tap["data"]["entity_id"].is<String>()) ta_tgt = tap["data"]["entity_id"].as<String>();
                        else if (tap["service_data"]["entity_id"].is<String>()) ta_tgt = tap["service_data"]["entity_id"].as<String>();
                    }
                }
                
                addWidget(entityId, cName, cIcon, ta_dom, ta_srv, ta_tgt);
            }

            if (obj["cards"].is<JsonArray>()) {
                for (JsonVariant card : obj["cards"].as<JsonArray>()) extractCards(card);
            }
            if (obj["sections"].is<JsonArray>()) {
                for (JsonVariant sec : obj["sections"].as<JsonArray>()) extractCards(sec);
            }
        } 
        else if (node.is<JsonArray>()) {
            for (JsonVariant child : node.as<JsonArray>()) extractCards(child);
        }
    };

    JsonArray viewsArray = views.as<JsonArray>();
    extractCards(viewsArray[targetViewIndex]);

    if (i == 0) {
        importErrorMessage = "Es wurden keine\nunterstuetzten Entitaeten\ngefunden.";
        pendingImportError = true;
    } else { 
        HaConfigLogic::Save(); 
        HaWebsocketLogic_UpdateTrackedEntities(); 
        ViewHomeAssistant::pendingHaReload = true; 
    }
    isImporting = false;
}