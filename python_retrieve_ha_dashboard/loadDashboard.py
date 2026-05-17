import asyncio
import json
import websockets

# --- KONFIGURATION ---
HA_IP = "192.168.2.44"  # IP deines Home Assistant
HA_PORT = "8123"  # Meist 8123
HA_TOKEN = "ey...your_token_here"  # Ein langlebiger Zugangstoken
URL_PATH = ""  # Leer für das Standard-Dashboard, oder der URL-Pfad (z.B. "lovelace-custom")


async def analyze_dashboard():
    uri = f"ws://{HA_IP}:{HA_PORT}/api/websocket"

    print(f"Verbinde zu {uri}...")
    try:
        async with websockets.connect(uri) as websocket:
            # 1. Auf auth_required warten
            msg = await websocket.recv()
            auth_req = json.loads(msg)
            if auth_req.get("type") == "auth_required":
                print("Authentifizierung erforderlich, sende Token...")

                # 2. Authentifizieren
                await websocket.send(json.dumps({
                    "type": "auth",
                    "access_token": HA_TOKEN
                }))

                auth_res = json.loads(await websocket.recv())
                if auth_res.get("type") != "auth_ok":
                    print(f"Auth fehlgeschlagen: {auth_res}")
                    return
                print("Authentifizierung erfolgreich!")

            # 3. Lovelace Config abfragen (identisch zu Zeile 701 in deinem C++ Code)
            req_id = 1
            request_payload = {
                "id": req_id,
                "type": "lovelace/config"
            }
            if URL_PATH:
                request_payload["url_path"] = URL_PATH

            print("Frage Dashboard-Konfiguration ab...")
            await websocket.send(json.dumps(request_payload))

            # 4. Auf die Antwort warten
            while True:
                response_str = await websocket.recv()
                response = json.loads(response_str)

                # Wir suchen nach der Antwort mit unserer ID
                if response.get("id") == req_id:
                    print("\n--- ANTWORT EMPFANGEN ---")

                    # Speichere die rohe JSON in eine Datei für genaue Analyse
                    with open("dashboard_config.json", "w", encoding="utf-8") as f:
                        json.dump(response, f, indent=4, ensure_ascii=False)

                    print("Ergebnis wurde in 'dashboard_config.json' gespeichert!")

                    # Kurzer Check der views
                    success = response.get("success", False)
                    print(f"Erfolg: {success}")
                    if success:
                        result = response.get("result", {})
                        views = result.get("views", [])
                        print(f"Anzahl gefundener Views/Tabs: {len(views)}")
                        for i, view in enumerate(views):
                            print(
                                f"  View {i}: title='{view.get('title')}', path='{view.get('path')}', icon='{view.get('icon')}'")
                    else:
                        print("Fehler von HA:", response.get("error"))
                    break

    except Exception as e:
        print(f"Ein Fehler ist aufgetreten: {e}")


if __name__ == "__main__":
    asyncio.run(analyze_dashboard())