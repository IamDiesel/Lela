import sys
import re
import os
import json
import subprocess
from PyQt5.QtWidgets import *
from PyQt5.QtGui import *
from PyQt5.QtCore import *


class IconStudio(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Lela OS - Icon Studio")
        self.resize(1150, 800)

        font_path = "materialdesignicons-webfont.ttf"
        self.mdi_font = None
        if os.path.exists(font_path):
            font_id = QFontDatabase.addApplicationFont(font_path)
            if font_id != -1:
                family = QFontDatabase.applicationFontFamilies(font_id)[0]
                self.mdi_font = QFont(family, 24)
        else:
            QMessageBox.critical(self, "Fehler", f"'{font_path}' nicht im Ordner gefunden!")
            sys.exit()

        self.all_icons = []
        self.selected_icons = {}

        self.ha_top_picks = [
            "home", "power", "lightbulb", "lightbulb-on", "menu", "check", "close", "cog", "plus", "minus",
            "television", "speaker", "play", "pause", "stop", "volume-high", "volume-off", "arrow-left", "arrow-right",
            "arrow-up",
            "arrow-down", "chevron-left", "chevron-right", "chevron-up", "chevron-down", "circle", "circle-outline",
            "check-circle", "undo", "refresh",
            "thermometer", "water-percent", "fire", "snowflake", "weather-sunny", "weather-night", "weather-cloudy",
            "weather-rainy", "battery", "battery-outline",
            "battery-charging", "plug", "wifi", "door", "door-open", "window-closed", "window-open", "sofa", "bed",
            "chair",
            "table", "shower", "bathtub", "toilet", "fridge", "stove", "microwave", "dishwasher", "washing-machine",
            "tumble-dryer",
            "robot-vacuum", "vacuum", "iron", "coffee", "cup", "blender", "kettle", "oven", "shield", "shield-check",
            "lock", "lock-open", "bell", "cctv", "motion-sensor", "smoke-detector", "lightning-bolt", "flash",
            "solar-panel", "solar-power",
            "router-wireless", "lan", "server", "nas", "bluetooth", "cast", "apple", "netflix", "youtube", "spotify",
            "headphones", "music", "video", "movie", "camera", "remote", "gamepad", "trash-can", "pencil",
            "content-save",
            "ceiling-light", "desk-lamp", "floor-lamp", "wall-sconce", "led-strip", "chandelier", "spotlight",
            "toggle-switch", "toggle-switch-off", "dimmer",
            "fan", "air-conditioner", "radiator", "fireplace", "umbrella", "thermostat", "gauge", "water",
            "home-assistant", "garage",
            "stairs", "balcony", "pool", "fence", "blinds", "roller-shade", "wardrobe", "sink", "mirror", "cat",
            "dog", "fish", "bird", "cow", "pig", "horse", "rabbit", "leaf", "flower", "tree",
            "pine-tree", "car", "car-electric", "car-sports", "truck", "bus", "train", "bicycle", "motorbike",
            "scooter",
            "airplane", "ferry", "coffee-maker", "coffee-outline", "coffee-to-go", "water-boiler", "grill", "toaster",
            "fire-alert", "leak",
            "molecule-co2", "alert", "information", "upload", "download", "keyboard-return", "backspace",
            "fast-forward", "rewind", "shuffle",
            "repeat", "volume-medium", "television-classic", "image", "microphone", "transmission-tower",
            "wind-turbine", "battery-50", "wifi-strength-2", "network",
            "door-closed", "window-variant", "bed-double", "sofa-outline", "chair-rolling", "table-furniture",
            "silverware", "glass-wine", "bottle-wine", "beer",
            "food-apple", "food-variant", "hammer", "wrench", "screwdriver", "toolbox", "watering-can", "mower",
            "flower-tulip", "mushroom",
            "weather-partly-cloudy", "weather-pouring", "weather-lightning", "weather-snowy", "weather-windy",
            "thermometer-lines", "fan-speed-1", "fan-speed-2", "fan-speed-3", "air-purifier",
            "security", "alarm-light", "alarm-panel", "fingerprint", "key", "key-variant", "doorbell", "doorbell-video",
            "mailbox", "package",
            "car-connected", "ev-station", "gas-station", "parking", "road", "sign-direction", "map-marker", "map",
            "compass", "navigation"
        ]

        # Timer für die ultraschnelle Such-Verzögerung (Debouncing)
        self.search_timer = QTimer()
        self.search_timer.setSingleShot(True)
        self.search_timer.timeout.connect(self.apply_filters)

        self.init_ui()

    def init_ui(self):
        main_layout = QVBoxLayout(self)

        top_layout = QHBoxLayout()

        btn_load = QPushButton("1. CSS Datei laden")
        btn_load.setStyleSheet("background-color: #3498db; color: white; font-weight: bold; padding: 8px;")
        btn_load.clicked.connect(self.load_css)
        top_layout.addWidget(btn_load)

        # NEU: Speichern & Laden Buttons
        btn_load_sel = QPushButton("Auswahl laden")
        btn_load_sel.setStyleSheet("background-color: #f39c12; color: white; font-weight: bold; padding: 8px;")
        btn_load_sel.clicked.connect(self.load_selection)
        top_layout.addWidget(btn_load_sel)

        btn_save_sel = QPushButton("Auswahl speichern")
        btn_save_sel.setStyleSheet("background-color: #d35400; color: white; font-weight: bold; padding: 8px;")
        btn_save_sel.clicked.connect(self.save_selection)
        top_layout.addWidget(btn_save_sel)

        self.cat_combo = QComboBox()
        self.cat_combo.addItem("Alle")
        self.cat_combo.addItems(["UI & Navigation", "Fernbedienung & Media", "Beleuchtung", "Klima & Wetter",
                                 "Räume & Möbel", "Küche & Haushalt", "Energie & Tech", "Sicherheit",
                                 "Natur & Tiere", "Fahrzeuge", "Alphabet & Zahlen", "Andere"])
        self.cat_combo.currentTextChanged.connect(self.apply_filters)
        top_layout.addWidget(QLabel(" Kategorie:"))
        top_layout.addWidget(self.cat_combo)

        self.search_input = QLineEdit()
        self.search_input.setPlaceholderText("Icon suchen...")
        # Debounce: Warte 200ms nach dem letzten Tastendruck, bevor die Suche startet
        self.search_input.textChanged.connect(lambda: self.search_timer.start(200))
        top_layout.addWidget(QLabel(" Suche:"))
        top_layout.addWidget(self.search_input)

        self.size_input = QLineEdit("48")
        self.size_input.setFixedWidth(50)
        top_layout.addWidget(QLabel(" LVGL-Größe:"))
        top_layout.addWidget(self.size_input)

        main_layout.addLayout(top_layout)

        rec_group = QGroupBox("Schnellstart: Home Assistant Empfehlungen (Basis-Auswahl)")
        rec_layout = QHBoxLayout()
        rec_group.setLayout(rec_layout)

        for count in [50, 100, 150, 200, 300, 400, 500, 600, 700]:
            btn = QPushButton(f"{count} Icons")
            btn.setStyleSheet("background-color: #8e44ad; color: white; font-weight: bold;")
            btn.clicked.connect(lambda checked, c=count: self.apply_recommendations(c))
            rec_layout.addWidget(btn)

        main_layout.addWidget(rec_group)

        table_layout = QHBoxLayout()

        left_layout = QVBoxLayout()
        avail_header_layout = QHBoxLayout()
        avail_header_layout.addWidget(QLabel("Verfügbare Icons (Unterstützt Shift, Strg, Strg+A):"))
        left_layout.addLayout(avail_header_layout)

        self.table_avail = QTableWidget(0, 3)
        self.table_avail.setHorizontalHeaderLabels(["Name", "Icon", "Hex"])
        self.table_avail.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.table_avail.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeToContents)
        self.table_avail.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeToContents)
        self.table_avail.verticalHeader().setDefaultSectionSize(45)
        self.table_avail.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self.table_avail.setSelectionBehavior(QAbstractItemView.SelectRows)
        left_layout.addWidget(self.table_avail)

        btn_layout = QVBoxLayout()
        btn_add = QPushButton("Hinzufügen >>")
        btn_add.setStyleSheet("background-color: #2ecc71; color: white; font-weight: bold; padding: 10px;")
        btn_add.clicked.connect(self.add_selected)

        btn_rem = QPushButton("<< Entfernen")
        btn_rem.setStyleSheet("background-color: #e74c3c; color: white; font-weight: bold; padding: 10px;")
        btn_rem.clicked.connect(self.remove_selected)

        btn_layout.addStretch()
        btn_layout.addWidget(btn_add)
        btn_layout.addWidget(btn_rem)
        btn_layout.addStretch()

        right_layout = QVBoxLayout()
        self.lbl_selected = QLabel("Ausgewählt (0)  |  Geschätzter Flash: 0.00 MB")
        self.lbl_selected.setStyleSheet("font-weight: bold; color: #d35400;")
        right_layout.addWidget(self.lbl_selected)

        self.table_sel = QTableWidget(0, 3)
        self.table_sel.setHorizontalHeaderLabels(["Name", "Icon", "Hex"])
        self.table_sel.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.table_sel.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeToContents)
        self.table_sel.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeToContents)
        self.table_sel.verticalHeader().setDefaultSectionSize(45)
        self.table_sel.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self.table_sel.setSelectionBehavior(QAbstractItemView.SelectRows)
        right_layout.addWidget(self.table_sel)

        table_layout.addLayout(left_layout, 5)
        table_layout.addLayout(btn_layout, 1)
        table_layout.addLayout(right_layout, 4)
        main_layout.addLayout(table_layout)

        bottom_layout = QHBoxLayout()
        self.chk_alphabet = QCheckBox("Standard-Alphabet & Zahlen (A-Z, 0-9) als Fallback einbauen")
        self.chk_alphabet.setChecked(False)
        bottom_layout.addWidget(self.chk_alphabet)

        btn_generate = QPushButton("2. Auto-Code & LVGL-Font generieren!")
        btn_generate.setStyleSheet(
            "background-color: #e67e22; color: white; font-size: 16px; font-weight: bold; padding: 15px;")
        btn_generate.clicked.connect(self.generate_files)
        bottom_layout.addWidget(btn_generate)
        main_layout.addLayout(bottom_layout)

    # --- NEU: JSON Export / Import ---
    def save_selection(self):
        if not self.selected_icons:
            QMessageBox.warning(self, "Fehler", "Es sind keine Icons ausgewählt, die gespeichert werden könnten.")
            return

        filepath, _ = QFileDialog.getSaveFileName(self, "Auswahl speichern", "Lela_Icon_Preset.json",
                                                  "JSON Files (*.json)")
        if filepath:
            try:
                with open(filepath, 'w', encoding='utf-8') as f:
                    json.dump(self.selected_icons, f, indent=4)
                QMessageBox.information(self, "Erfolg", "Deine Icon-Auswahl wurde erfolgreich gespeichert!")
            except Exception as e:
                QMessageBox.critical(self, "Fehler", f"Konnte Datei nicht speichern:\n{str(e)}")

    def load_selection(self):
        if not self.all_icons:
            QMessageBox.warning(self, "Fehler", "Bitte lade zuerst die CSS Datei, bevor du ein Preset importierst!")
            return

        filepath, _ = QFileDialog.getOpenFileName(self, "Auswahl laden", "", "JSON Files (*.json)")
        if filepath:
            try:
                with open(filepath, 'r', encoding='utf-8') as f:
                    data = json.load(f)

                # Wir ergänzen die aktuelle Auswahl um die importierte Liste
                self.selected_icons.update(data)
                self.refresh_selected_table()
                self.apply_filters()
                QMessageBox.information(self, "Erfolg",
                                        f"Auswahl geladen! Du hast nun {len(self.selected_icons)} Icons markiert.")
            except Exception as e:
                QMessageBox.critical(self, "Fehler", f"Konnte Datei nicht laden. Ist es ein gültiges Preset?\n{str(e)}")

    def smart_sort_key(self, item):
        name = item if isinstance(item, str) else item["name"]
        parts = name.split('-')
        base_parts = []
        var_parts = []

        directions = {'up', 'down', 'left', 'right', 'top', 'bottom', 'forward', 'backward', 'next', 'previous'}

        for part in parts:
            if part in directions or (len(part) == 1 and part.isalpha()) or part.isdigit():
                if part.isdigit():
                    var_parts.append(part.zfill(4))
                else:
                    var_parts.append(part)
            else:
                base_parts.append(part)

        return "-".join(base_parts) + "|" + "-".join(var_parts)

    def categorize(self, name):
        if len(name) == 1 or "numeric" in name or "alpha" in name:
            return "Alphabet & Zahlen"

        cats = {
            "UI & Navigation": ["menu", "check", "close", "plus", "minus", "cog", "refresh", "trash", "sync", "upload",
                                "download", "pencil", "save", "undo", "return", "power", "alert", "info"],
            "Fernbedienung & Media": ["play", "pause", "stop", "video", "music", "volume", "tv", "television",
                                      "speaker", "cast", "movie", "camera", "headphone", "remote", "forward", "rewind",
                                      "shuffle", "repeat", "spotify", "netflix", "youtube", "arrow", "chevron",
                                      "circle", "square", "triangle"],
            "Räume & Möbel": ["home", "door", "window", "bed", "sofa", "chair", "table", "shower", "bath", "toilet",
                              "garage", "stairs", "balcony", "pool", "fence", "blind", "wardrobe", "mirror", "sink"],
            "Küche & Haushalt": ["fridge", "stove", "microwave", "washing", "vacuum", "iron", "coffee", "blender",
                                 "kettle", "oven", "cup", "dish", "grill", "boiler"],
            "Energie & Tech": ["solar", "battery", "flash", "lightning", "plug", "wifi", "bluetooth", "router",
                               "network", "server", "nas", "lan"],
            "Klima & Wetter": ["thermometer", "water", "fan", "fire", "snow", "weather", "sun", "moon", "cloud",
                               "umbrella", "thermostat", "gauge", "ac", "radiator"],
            "Beleuchtung": ["light", "lamp", "led", "chandelier", "spotlight", "dimmer", "toggle"],
            "Sicherheit": ["lock", "shield", "cctv", "bell", "motion", "smoke", "leak"],
            "Natur & Tiere": ["cat", "dog", "fish", "bird", "cow", "pig", "horse", "leaf", "flower", "tree", "rabbit"],
            "Fahrzeuge": ["car", "bus", "truck", "bike", "scooter", "airplane", "train", "ferry", "motor"]
        }
        for cat, keywords in cats.items():
            if any(kw in name for kw in keywords):
                return cat
        return "Andere"

    def load_css(self):
        filepath, _ = QFileDialog.getOpenFileName(self, "CSS öffnen", "", "CSS Files (*.css)")
        if not filepath: return
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        pattern = r"\.mdi-([a-zA-Z0-9-]+)::before\s*\{\s*content:\s*\"\\([0-9a-fA-F]+)\";\s*\}"
        matches = re.findall(pattern, content)

        self.all_icons.clear()
        for name, hex_code in matches:
            self.all_icons.append({"name": name, "hex": hex_code.upper(), "cat": self.categorize(name)})

        self.all_icons.sort(key=self.smart_sort_key)

        # PERFORMANCE BOOST: Wir bauen die linke Tabelle genau 1x auf.
        self.table_avail.setRowCount(len(self.all_icons))
        for row, ic in enumerate(self.all_icons):
            chk = QTableWidgetItem(ic['name'])
            chk.setFlags(Qt.ItemIsSelectable | Qt.ItemIsEnabled)
            self.table_avail.setItem(row, 0, chk)

            icon_item = QTableWidgetItem(chr(int(ic['hex'], 16)))
            icon_item.setFont(self.mdi_font)
            icon_item.setTextAlignment(Qt.AlignCenter)
            self.table_avail.setItem(row, 1, icon_item)

            hex_item = QTableWidgetItem(ic['hex'])
            hex_item.setTextAlignment(Qt.AlignCenter)
            self.table_avail.setItem(row, 2, hex_item)

        self.apply_filters()
        QMessageBox.information(self, "Erfolg",
                                f"{len(self.all_icons)} Icons in den Cache geladen! Suche ist nun blitzschnell.")

    # PERFORMANCE BOOST: Versteckt/Zeigt nur noch Reihen an (Dauert 0,05 Sekunden statt 1 Sekunde)
    def apply_filters(self):
        search = self.search_input.text().lower()
        cat_filter = self.cat_combo.currentText()

        for row, ic in enumerate(self.all_icons):
            name = ic['name']

            # Soll das Icon angezeigt werden?
            is_match = (search in name.lower() and
                        (cat_filter == "Alle" or ic['cat'] == cat_filter) and
                        name not in self.selected_icons)

            # Hide = True (wenn is_match = False ist)
            self.table_avail.setRowHidden(row, not is_match)

    # Aktualisiert nur die rechte Tabelle
    def refresh_selected_table(self):
        self.table_sel.setRowCount(len(self.selected_icons))

        count = len(self.selected_icons)
        flash_kb = count * 1.2
        flash_mb = flash_kb / 1024

        if flash_mb > 2.0:
            self.lbl_selected.setStyleSheet("font-weight: bold; color: #c0392b;")
        else:
            self.lbl_selected.setStyleSheet("font-weight: bold; color: #27ae60;")

        self.lbl_selected.setText(f"Ausgewählt ({count})  |  Geschätzter Flash: {flash_mb:.2f} MB")

        sorted_selected_names = sorted(self.selected_icons.keys(), key=self.smart_sort_key)

        for row, name in enumerate(sorted_selected_names):
            hex_code = self.selected_icons[name]
            chk = QTableWidgetItem(name)
            chk.setFlags(Qt.ItemIsSelectable | Qt.ItemIsEnabled)
            self.table_sel.setItem(row, 0, chk)

            icon_item = QTableWidgetItem(chr(int(hex_code, 16)))
            icon_item.setFont(self.mdi_font)
            icon_item.setTextAlignment(Qt.AlignCenter)
            self.table_sel.setItem(row, 1, icon_item)

            hex_item = QTableWidgetItem(hex_code)
            hex_item.setTextAlignment(Qt.AlignCenter)
            self.table_sel.setItem(row, 2, hex_item)

    def apply_recommendations(self, target_count):
        if not self.all_icons:
            QMessageBox.warning(self, "Fehler", "Bitte lade zuerst eine CSS-Datei!")
            return

        self.selected_icons.clear()
        added = 0

        for name in self.ha_top_picks:
            if added >= target_count:
                break
            for ic in self.all_icons:
                if ic['name'] == name:
                    self.selected_icons[name] = ic['hex']
                    added += 1
                    break

        if added < target_count:
            cats = {}
            for ic in self.all_icons:
                if ic['name'] not in self.selected_icons and ic['cat'] != "Alphabet & Zahlen":
                    cats.setdefault(ic['cat'], []).append(ic)

            cat_names = list(cats.keys())
            idx = 0
            while added < target_count and len(cat_names) > 0:
                cat = cat_names[idx % len(cat_names)]
                if len(cats[cat]) > 0:
                    ic = cats[cat].pop(0)
                    self.selected_icons[ic['name']] = ic['hex']
                    added += 1
                else:
                    cat_names.remove(cat)
                    if len(cat_names) == 0: break
                    continue
                idx += 1

        self.refresh_selected_table()
        self.apply_filters()
        QMessageBox.information(self, "Erfolg", f"{added} empfohlene Home Assistant Icons vorausgewählt!")

    def add_selected(self):
        rows_to_add = set([item.row() for item in self.table_avail.selectedItems()])
        for row in rows_to_add:
            name = self.table_avail.item(row, 0).text()
            hex_code = self.table_avail.item(row, 2).text()
            self.selected_icons[name] = hex_code

        self.table_avail.clearSelection()
        self.refresh_selected_table()
        self.apply_filters()

    def remove_selected(self):
        rows_to_remove = set([item.row() for item in self.table_sel.selectedItems()])
        names_to_remove = [self.table_sel.item(row, 0).text() for row in rows_to_remove]
        for name in names_to_remove:
            if name in self.selected_icons:
                del self.selected_icons[name]

        self.table_sel.clearSelection()
        self.refresh_selected_table()
        self.apply_filters()

    def compact_hex_list(self, hex_strings):
        ints = sorted([int(x, 16) for x in hex_strings])
        if not ints: return ""

        ranges = []
        start = ints[0]
        end = ints[0]

        for val in ints[1:]:
            if val == end + 1:
                end = val
            else:
                ranges.append(f"0x{start:X}" if start == end else f"0x{start:X}-0x{end:X}")
                start = val
                end = val

        ranges.append(f"0x{start:X}" if start == end else f"0x{start:X}-0x{end:X}")
        return ",".join(ranges)

    def hex_to_utf8_c_string(self, hex_str):
        code = int(hex_str, 16)
        if code <= 0x7F:
            return f'"\\x{code:02x}"'
        elif code <= 0x7FF:
            b1 = 0xC0 | (code >> 6)
            b2 = 0x80 | (code & 0x3F)
            return f'"\\x{b1:02x}\\x{b2:02x}"'
        elif code <= 0xFFFF:
            b1 = 0xE0 | (code >> 12)
            b2 = 0x80 | ((code >> 6) & 0x3F)
            b3 = 0x80 | (code & 0x3F)
            return f'"\\x{b1:02x}\\x{b2:02x}\\x{b3:02x}"'
        else:
            b1 = 0xF0 | (code >> 18)
            b2 = 0x80 | ((code >> 12) & 0x3F)
            b3 = 0x80 | ((code >> 6) & 0x3F)
            b4 = 0x80 | (code & 0x3F)
            return f'"\\x{b1:02x}\\x{b2:02x}\\x{b3:02x}\\x{b4:02x}"'

    def generate_files(self):
        if not self.selected_icons:
            QMessageBox.warning(self, "Halt!", "Keine Icons ausgewählt!")
            return

        font_size = self.size_input.text()

        header = "#pragma once\n#include <Arduino.h>\n#include \"lvgl.h\"\n\nLV_FONT_DECLARE(lela_icons);\n\n"
        header += "// --- 1. C++ MACROS (RAW UTF-8 BYTES) ---\n"
        hex_list = []

        cat_dict = {}

        sorted_selected = sorted(self.selected_icons.items(), key=lambda item: self.smart_sort_key(item[0]))

        for name, hex_code in sorted_selected:
            macro_name = "MY_ICON_" + name.replace("-", "_").upper()

            raw_utf8 = self.hex_to_utf8_c_string(hex_code)
            header += f'#define {macro_name.ljust(35)} {raw_utf8}\n'

            hex_list.append(hex_code)

            cat = self.categorize(name)
            if cat not in cat_dict: cat_dict[cat] = []
            cat_dict[cat].append(name)

        header += "\n// --- 2. DROPDOWN KATEGORIEN (FÜR UI EDITOR) ---\n"
        header += "struct IconCategory { const char* name; const char* options; };\n\n"
        header += "static const IconCategory icon_categories[] = {\n"
        header += '    {"Standard", "--- Unveraendert ---\\nStandard"},\n'

        for cat_name in sorted(cat_dict.keys()):
            opts = "--- Unveraendert ---\\nStandard"
            sorted_cat_icons = sorted(cat_dict[cat_name], key=self.smart_sort_key)
            for ic in sorted_cat_icons:
                opts += f"\\nmdi:{ic}"
            header += f'    {{"{cat_name}", "{opts}"}},\n'

        header += "};\n"
        header += f"static const int num_icon_categories = {len(cat_dict) + 1};\n\n"

        header += "// --- 3. AUTO-MAPPER FÜR HOME ASSISTANT ---\n"
        header += "inline String getAutoIcon(String mdi_icon) {\n"

        for name in sorted(self.selected_icons.keys(), key=self.smart_sort_key):
            macro_name = "MY_ICON_" + name.replace("-", "_").upper()
            header += f'    if (mdi_icon == "mdi:{name}") return {macro_name};\n'
        header += '    return "";\n}\n'

        with open("MdiMapper.h", "w", encoding="utf-8") as f:
            f.write(header)

        range_str = self.compact_hex_list(hex_list)
        QMessageBox.information(self, "Bitte warten",
                                "C-Datei wird kompiliert. Bitte Terminal-Fenster im Hintergrund beachten.")

        cmd = [
            "npx", "lv_font_conv@1.5.3",
            "--bpp", "4",
            "--size", font_size,
            "--font", "materialdesignicons-webfont.ttf",
            "--range", range_str,
        ]

        if self.chk_alphabet.isChecked():
            cmd.extend(["--symbols", "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"])

        cmd.extend([
            "--format", "lvgl",
            "--force-fast-kern-format",
            "--no-compress",
            "--output", "lela_icons.c"
        ])

        try:
            subprocess.run(cmd, check=True, shell=True)

            try:
                with open("lela_icons.c", "r", encoding="utf-8") as f_c:
                    c_content = f_c.read()

                c_content = c_content.replace('#include "lvgl/lvgl.h"', '#include "lvgl.h"')

                with open("lela_icons.c", "w", encoding="utf-8") as f_c:
                    f_c.write(c_content)
            except Exception as e:
                pass

            QMessageBox.information(self, "Perfekt!", "Dateien erfolgreich generiert!")
        except subprocess.CalledProcessError:
            QMessageBox.critical(self, "Fehler", "Der Node.js Converter ist fehlgeschlagen.")


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = IconStudio()
    window.show()
    sys.exit(app.exec_())