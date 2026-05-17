import asyncio
import json
import websockets

# --- KONFIGURATION ---
HA_URL = "ws://192.168.2.44:8123/api/websocket"  # IP anpassen!
HA_TOKEN = "ey...your_token_here"  # Ein langlebiger Zugangstoken

# Lass dies leer ("") für das Standard-Dashboard,
# oder trage den url_path ein (z.B. "lovelace-dash"), falls es ein separates ist.
DASHBOARD_URL_PATH = "dashboard-ecoflow"


async def analyze_ha():
    try:
        async with websockets.connect(HA_URL) as ws:
            # 1. Auth Challenge empfangen
            await ws.recv()

            # 2. Authentifizieren
            await ws.send(json.dumps({
                "type": "auth",
                "access_token": TOKEN
            }))

            auth_res = json.loads(await ws.recv())
            if auth_res.get("type") != "auth_ok":
                print("Auth fehlgeschlagen:", auth_res)
                return
            print("Erfolgreich mit Home Assistant verbunden!")

            # 3. Lovelace Config abfragen (Exakt wie im ESP32 Code)
            req = {
                "id": 1,
                "type": "lovelace/config"
            }
            if DASHBOARD_URL_PATH:
                req["url_path"] = DASHBOARD_URL_PATH

            await ws.send(json.dumps(req))

            # 4. Antwort empfangen und verarbeiten
            response_str = await ws.recv()
            data = json.loads(response_str)

            # JSON in Datei speichern für unsere Fehleranalyse
            filename = "ha_lovelace_dump.json"
            with open(filename, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2, ensure_ascii=False)

            print(f"\nKomplette JSON-Struktur wurde in '{filename}' gespeichert.")

            # Kurze Vorschau in der Konsole
            if data.get("success"):
                result = data.get("result", {})
                # HA verschachtelt das manchmal unterschiedlich
                views = result.get("views", result.get("result", {}).get("views", []))

                print(f"\n--- Gefundene Tabs (Views): {len(views)} ---")
                for i, v in enumerate(views):
                    title = v.get("title", v.get("path", "Unbenannt"))
                    visible = v.get("visible", True)
                    subview = v.get("subview", False)
                    print(f"Index {i}: {title} (Sichtbar: {visible}, Subview: {subview})")
            else:
                print("Home Assistant hat einen Fehler zurückgegeben:", data)

    except Exception as e:
        print(f"Verbindungsfehler: {e}")


if __name__ == "__main__":
    asyncio.run(analyze_ha())