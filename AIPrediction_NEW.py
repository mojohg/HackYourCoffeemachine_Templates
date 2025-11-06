#!/usr/bin/env python3
# vim: set fileencoding=utf-8 :
import asyncio
import json
import sys
import numpy as np
import pandas as pd
import joblib
from aiomqtt import Client
from scipy.integrate import simpson
from scipy.signal import find_peaks, savgol_filter

class EnergyOnlyRunner:
    def __init__(self):
        # MQTT
        self.BROKER = "192.168.178.21"
        self.PORT = 1883
        self.TOPIC_ENERGY = "bipfinnland/monitoring11/data"   # erwartet: {"ts": <s>, "current": <A>}
        self.TOPIC_AI_OUT = "bipfinnland/hackyourcoffee11/ai" # publish: {"prediction": "<label>"}

        # Modell
        self.MODEL_PATH = "coffee_rf_pipeline.pkl"
        self.META_PATH  = "model_meta.pkl"

        self.pipeline = joblib.load(self.MODEL_PATH)
        try:
            meta = joblib.load(self.META_PATH)
            self.feature_cols = meta.get("feature_cols")
        except Exception:
            self.feature_cols = [
                "samples",
                "cycle_duration_s",
                "area_under_curve_A*s",
                "mean_current_A",
                "variance_current_A2",
                "rms_current_A",
                "max_peak_A",
                "time_to_first_peak_s",
            ]

        # Segmentierung: genau deine Logik (ein Schwellenwert 0.05 A)
        self.threshold = 0.05  # A
        self.in_segment = False
        self.buffer = []       # sammelt (ts, current)
        self.min_samples = 10  # für stabile Features

    async def handle_energy(self, message, client):
        try:
            payload = json.loads(message.payload.decode("utf-8"))
            ts = float(payload.get("ts"))
            current = float(payload.get("current"))
        except Exception:
            return  # ignorieren, wenn das Format nicht passt

        if not self.in_segment and current > self.threshold:
            # Start
            self.in_segment = True
            self.buffer = [(ts, current)]
            print(f"[SEGMENT] start: ts={ts:.3f}, I={current:.3f} A")
            return

        if self.in_segment:
            self.buffer.append((ts, current))
            if current < self.threshold:
                # Ende
                self.in_segment = False
                print(f"[SEGMENT] end: samples={len(self.buffer)}")
                await self._finalize_segment_and_predict(client)
                self.buffer = []

    async def _finalize_segment_and_predict(self, client):
        if len(self.buffer) < self.min_samples:
            print(f"[SEGMENT] skipped (samples {len(self.buffer)} < {self.min_samples})")
            return

        feats = self._extract_features_df(self.buffer)
        if feats is None:
            print("[FEATURES] extraction failed/empty")
            return

        # sicherstellen, dass alle erwarteten Spalten vorhanden sind
        for c in self.feature_cols:
            if c not in feats.columns:
                feats[c] = np.nan
        feats = feats[self.feature_cols]

        try:
            pred = self.pipeline.predict(feats)[0]
            print(f"[PREDICTION] {pred}")
            await client.publish(self.TOPIC_AI_OUT, json.dumps({"prediction": str(pred)}))
        except Exception as e:
            print("[PREDICTION] failed:", e)

    def _extract_features_df(self, samples):
        """
        samples: Liste [(ts, current)] mit ts in s (Shelly), current in A
        Rückgabe: DataFrame mit den Feature-Spalten
        """
        arr = np.asarray(samples, dtype=float)
        if arr.ndim != 2 or arr.shape[0] < 3:
            return None

        ts = arr[:, 0]
        I  = arr[:, 1]

        # relative Zeitachse
        t_rel = ts - ts.min()

        # NaNs füllen falls nötig
        if np.isnan(I).any():
            I = pd.Series(I).fillna(method="ffill").fillna(method="bfill").to_numpy()

        # Savitzky-Golay smoothing (Fenster dynamisch, ungerade)
        win = min(25, len(I))
        if win < 3: win = 3
        if win % 2 == 0: win -= 1
        poly = 2 if win >= 5 else 1
        try:
            I_s = savgol_filter(I, window_length=win, polyorder=poly, mode="interp")
        except Exception:
            I_s = I  # fallback

        area = float(simpson(I_s, x=t_rel))                               # A*s
        dur  = float(t_rel[-1] - t_rel[0]) if len(t_rel) > 1 else 0.0
        mean = float(np.mean(I_s))
        var  = float(np.var(I_s))
        rms  = float(np.sqrt(np.mean(I_s**2)))
        thr  = float(np.percentile(I_s, 75)) if len(I_s) else 0.0
        peaks, _ = find_peaks(I_s, height=thr)
        max_peak = float(np.max(I_s[peaks])) if len(peaks) > 0 else None
        t_first  = float(t_rel[peaks[0]]) if len(peaks) > 0 else None

        row = {
            "samples": int(len(I)),
            "cycle_duration_s": dur,
            "area_under_curve_A*s": area,
            "mean_current_A": mean,
            "variance_current_A2": var,
            "rms_current_A": rms,
            "max_peak_A": max_peak,
            "time_to_first_peak_s": t_first,
        }
        return pd.DataFrame([row])

    async def main(self):
        async with Client(hostname=self.BROKER, port=self.PORT) as client:
            await client.subscribe(self.TOPIC_ENERGY)
            print(f"[MQTT] subscribed: {self.TOPIC_ENERGY}")

            async for message in client.messages:
                if message.topic == self.TOPIC_ENERGY:
                    await self.handle_energy(message, client)

if __name__ == "__main__":
    if sys.platform.lower() == "win32":
        from asyncio import set_event_loop_policy, WindowsSelectorEventLoopPolicy
        set_event_loop_policy(WindowsSelectorEventLoopPolicy())

    app = EnergyOnlyRunner()
    asyncio.run(app.main())
