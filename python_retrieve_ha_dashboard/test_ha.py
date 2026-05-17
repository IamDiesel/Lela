import asyncio
import websockets
import json

# --- HIER ANPASSEN ---
HA_URL = "ws://192.168.2.44:8123/api/websocket"  # IP anpassen!
HA_TOKEN = "ey...your_token_here"  # Ein langlebiger Zugangstoken


async def test_ha():
    print(f"Verbinde mit {HA_URL}...")
    try:
        async with websockets.connect(HA_URL) as websocket:
            # 1. Warten auf auth_required
            msg = await websocket.recv()
            print("HA fordert Authentifizierung:", json.loads(msg)["type"])

            # 2. Token senden
            await websocket.send(json.dumps({
                "type": "auth",
                "access_token": TOKEN
            }))
            msg = await websocket.recv()
            auth_result = json.loads(msg)

            if auth_result.get("type") != "auth_ok":
                print("Auth fehlgeschlagen:", auth_result)
                return

            print("Authentifizierung erfolgreich! Rufe get_states ab...\n")

            # 3. get_states abfragen
            await websocket.send(json.dumps({
                "id": 1,
                "type": "get_states"
            }))

            # 4. Antwort parsen
            while True:
                msg = await websocket.recv()
                data = json.loads(msg)

                if data.get("id") == 1 and data.get("type") == "result":
                    print("--- GEFUNDENE SELECT / INPUT_SELECT ENTITÄTEN ---")
                    found = False
                    for entity in data.get("result", []):
                        entity_id = entity.get("entity_id", "")

                        if entity_id.startswith("input_select.") or entity_id.startswith("select."):
                            found = True

                            # --- NEU: Den aktuellen Status (Wert) auslesen ---
                            current_state = entity.get("state", "Unbekannt")

                            print(f"\nEntity: {entity_id}")
                            print(f"Aktueller Wert (State): >>> {current_state} <<<")
                            print("Attribute:")
                            print(json.dumps(entity.get("attributes", {}), indent=2, ensure_ascii=False))

                    if not found:
                        print("Keine Entitäten dieses Typs gefunden.")
                    break

    except Exception as e:
        print("Ein Fehler ist aufgetreten:", e)


if __name__ == "__main__":
    asyncio.run(test_ha())