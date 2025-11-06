#!/usr/bin/env python3
# vim: set fileencoding=utf-8 :
import asyncio
import csv
import json
import os
import sys
from aiomqtt import Client

class CoffeeEnergyLogger:
    def __init__(self):
        # MQTT
        self.BROKER = "192.168.178.21"
        self.PORT = 1883

        # Topics (pass bei Bedarf an)
        self.TOPIC_COFFEE  = "bipfinnland/hackyourcoffee11/data"
        self.TOPIC_ENERGY  = "bipfinnland/monitoring11/data"
        self.TOPIC_CONTROL = "bipfinnland/monitoring11/control"

        # CSVs
        self.COFFEE_DB = "coffee_data.csv"      # timestamp,label,info
        self.ENERGY_DB = "energy_log.csv"       # ts_shelly,current_A

        # Zustand
        self.saving_data = False

        # CSV-Header anlegen falls neu
        self._init_csv(self.COFFEE_DB, ["timestamp", "label", "info"])
        self._init_csv(self.ENERGY_DB, ["ts_shelly", "current_A"])

    def _init_csv(self, path, headers):
        if not os.path.exists(path):
            with open(path, "w", newline="") as f:
                csv.writer(f).writerow(headers)

    # ---------- Handler ----------

    async def handle_control(self, message):
        try:
            payload = json.loads(message.payload.decode("utf-8"))
            control = (payload.get("control") or "").strip().lower()
        except Exception as e:
            print("[WARN] control parse error:", e)
            return

        if control == "start":
            self.saving_data = True
            print("[INFO] logging STARTED")
        elif control in ("end", "stop"):
            self.saving_data = False
            print("[INFO] logging STOPPED")
        else:
            print("[INFO] unknown control:", control)

    async def handle_coffee(self, message):
        try:
            payload = json.loads(message.payload.decode("utf-8"))
        except Exception as e:
            print("[WARN] coffee parse error:", e)
            return

        ts_iso = payload.get("timestamp")  # z.B. "2025-11-05T18:07:22.165Z"
        label  = payload.get("label")
        info   = payload.get("info")

        if not ts_iso or not label:
            print("[WARN] coffee payload missing fields:", payload)
            return

        with open(self.COFFEE_DB, "a", newline="") as f:
            csv.writer(f).writerow([ts_iso, label, info])
        print(f"[COFFEE] {ts_iso}  label={label}")

    async def handle_energy(self, message):
        # nur loggen, wenn aktiv
        if not self.saving_data:
            return
        try:
            payload = json.loads(message.payload.decode("utf-8"))
        except Exception as e:
            print("[ERROR] energy parse error:", e)
            return

        # Erwartet: Shelly-Format mit 'ts' (Sekunden) und 'current' (Ampere)
        ts = payload.get("ts")
        current = payload.get("current")

        if ts is None or current is None:
            print("[WARN] missing ts/current in energy payload:", payload)
            return

        with open(self.ENERGY_DB, "a", newline="") as f:
            csv.writer(f).writerow([ts, current])

        print(f"[ENERGY] ts={ts}  current={current} A")

    # ---------- Main ----------

    async def main(self):
        async with Client(hostname=self.BROKER, port=self.PORT) as client:
            # alle drei Topics
            await client.subscribe(self.TOPIC_COFFEE)
            await client.subscribe(self.TOPIC_ENERGY)
            await client.subscribe(self.TOPIC_CONTROL)
            print("[INFO] subscribed to topics")

            async for message in client.messages:
                t = message.topic
                if t == self.TOPIC_CONTROL:
                    await self.handle_control(message)
                elif t == self.TOPIC_COFFEE:
                    await self.handle_coffee(message)
                elif t == self.TOPIC_ENERGY:
                    await self.handle_energy(message)

if __name__ == "__main__":
    # Windows Event Loop Fix
    if sys.platform.lower() == "win32" or os.name.lower() == "nt":
        from asyncio import set_event_loop_policy, WindowsSelectorEventLoopPolicy
        set_event_loop_policy(WindowsSelectorEventLoopPolicy())

    app = CoffeeEnergyLogger()
    asyncio.run(app.main())
