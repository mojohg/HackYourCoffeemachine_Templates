import os
import numpy as np
import pandas as pd
from scipy.integrate import simpson
from scipy.signal import find_peaks, savgol_filter

# -----------------------------
# Dateien
# -----------------------------
coffee_file_path = 'coffee_data.csv'     # columns: timestamp,label,info
energy_file_path = 'energy_log.csv'      # columns: ts_shelly,current_A
output_file = 'training_data.csv'

# Schwelle für Produktgrenze (Sekunden Inaktivität)
product_boundary_threshold = 5

# -----------------------------
# Coffee-Labels laden
# -----------------------------
coffee_data = pd.read_csv(coffee_file_path)

# Timestamps optional parsen (hilfreich zum Sortieren/Fallback)
if 'timestamp' in coffee_data.columns:
    coffee_data['timestamp'] = pd.to_datetime(coffee_data['timestamp'], errors='coerce', utc=True)
    coffee_data = coffee_data.sort_values('timestamp')

coffee_labels = coffee_data['label'].tolist()

# -----------------------------
# Energy-Daten laden (Shelly)
# -----------------------------
energy_data = pd.read_csv(energy_file_path)

# Spalten prüfen
required = {'ts_shelly', 'current_A'}
missing = required - set(energy_data.columns)
if missing:
    raise ValueError(f"Missing columns in {energy_file_path}: {missing}")

# Nach Zeit sortieren (Sicherheit)
energy_data = energy_data.sort_values('ts_shelly').reset_index(drop=True)

# Zeitdifferenz + Segmentierung
energy_data['time_diff'] = energy_data['ts_shelly'].diff()
energy_data['product_id'] = (energy_data['time_diff'] > product_boundary_threshold).cumsum()

# -----------------------------
# Features je Segment
# -----------------------------
rows = []
label_idx = 0

for product_id, product_df in energy_data.groupby('product_id', sort=True):
    # Relativ-Zeit innerhalb Segment
    t0 = product_df['ts_shelly'].min()
    time_data = (product_df['ts_shelly'] - t0).to_numpy(dtype=float)
    current_data = product_df['current_A'].to_numpy(dtype=float)

    # Mindestens ein paar Samples nötig
    if len(current_data) < 3 or np.all(np.isnan(current_data)):
        continue

    # Savitzky-Golay: Fenster an Datenlänge anpassen, ungerade halten
    win = min(25, len(current_data))
    if win < 3:
        win = 3
    if win % 2 == 0:
        win -= 1
    poly = 2 if win >= 5 else 1

    # NaNs entfernen/auffüllen (falls vorhanden)
    if np.isnan(current_data).any():
        # simple forward-fill auf Series-Ebene
        s = pd.Series(current_data).fillna(method='ffill').fillna(method='bfill')
        current_data = s.to_numpy()

    smoothed = savgol_filter(current_data, window_length=win, polyorder=poly, mode='interp')

    # Features
    area_under_curve = float(simpson(smoothed, x=time_data))           # A*s
    cycle_duration_s = float(time_data[-1] - time_data[0]) if len(time_data) > 1 else 0.0
    mean_current_A = float(np.mean(smoothed))
    variance_current_A2 = float(np.var(smoothed))
    # adaptive Peak-Schwelle: 75. Perzentil
    thr = float(np.percentile(smoothed, 75)) if len(smoothed) > 0 else 0.0
    peaks, props = find_peaks(smoothed, height=thr)
    peak_values = smoothed[peaks] if len(peaks) else np.array([])
    time_to_first_peak_s = float(time_data[peaks[0]]) if len(peaks) > 0 else None
    rms_current_A = float(np.sqrt(np.mean(smoothed ** 2)))
    max_peak_A = float(np.max(peak_values)) if len(peak_values) > 0 else None

    # Label nach Reihenfolge zuordnen
    product_label = coffee_labels[label_idx] if label_idx < len(coffee_labels) else None
    label_idx += 1

    rows.append({
        'segment_id': int(product_id),
        'product_label': product_label,
        'samples': int(len(current_data)),
        'cycle_duration_s': cycle_duration_s,
        'area_under_curve_A*s': area_under_curve,
        'mean_current_A': mean_current_A,
        'variance_current_A2': variance_current_A2,
        'rms_current_A': rms_current_A,
        'max_peak_A': max_peak_A,
        'time_to_first_peak_s': time_to_first_peak_s
    })

# -----------------------------
# Schreiben
# -----------------------------
df_out = pd.DataFrame(rows)
if not df_out.empty:
    if not os.path.exists(output_file):
        df_out.to_csv(output_file, index=False)
        print(f"Created new file: {output_file} ({len(df_out)} rows)")
    else:
        df_out.to_csv(output_file, mode='a', header=False, index=False)
        print(f"Appended {len(df_out)} rows to: {output_file}")
else:
    print("No segments/features produced (check threshold and data).")
