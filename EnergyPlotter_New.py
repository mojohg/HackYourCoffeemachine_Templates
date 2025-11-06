import pandas as pd
import matplotlib.pyplot as plt

# CSV-Datei laden
energy_data = pd.read_csv('energy_log_1.csv')

# Spalten prüfen
print(energy_data.head())

# Zeitachse (Shelly liefert 'ts' in Sekunden)
timestamps = energy_data['ts_shelly']
current = energy_data['current_A']

# Plot erstellen
plt.figure(figsize=(14, 7))
plt.plot(timestamps, current, label='Current (A)', color='blue', linewidth=1)

# Achsen und Titel
plt.xlabel('Time (s, Shelly timestamp)')
plt.ylabel('Current (A)')
plt.title('Current over Time')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.show()
