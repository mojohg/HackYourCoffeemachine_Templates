import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.pipeline import Pipeline
from sklearn.impute import SimpleImputer
from sklearn.preprocessing import StandardScaler
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import classification_report, accuracy_score
import joblib

# === 1) Daten laden ===
df = pd.read_csv("training_data.csv")

# Erwartete Spalten (aus der neuen Feature-Extraction)
expected_cols = [
    "product_label",
    "segment_id",
    "samples",
    "cycle_duration_s",
    "area_under_curve_A*s",
    "mean_current_A",
    "variance_current_A2",
    "rms_current_A",
    "max_peak_A",
    "time_to_first_peak_s",
]

missing = [c for c in expected_cols if c not in df.columns]
if missing:
    raise ValueError(f"Fehlende Spalten in training_data.csv: {missing}\nGefunden: {list(df.columns)}")

# === 2) Ziel & Features ===
y = df["product_label"].astype(str)

feature_cols = [
    "samples",
    "cycle_duration_s",
    "area_under_curve_A*s",
    "mean_current_A",
    "variance_current_A2",
    "rms_current_A",
    "max_peak_A",
    "time_to_first_peak_s",
    # 'segment_id' lassen wir als reine ID weg (kein inhaltliches Feature)
]
X = df[feature_cols].copy()

# === 3) Train/Test Split (stratifiziert) ===
X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.30, random_state=42, stratify=y if y.nunique() > 1 else None
)

# === 4) Pipeline: Imputer + (optional) Scaler + RandomForest ===
# Hinweis: RF braucht kein Scaling, aber das stört auch nicht. Imputer ist wichtig.
pipeline = Pipeline(steps=[
    ("imputer", SimpleImputer(strategy="median")),
    ("scaler", StandardScaler(with_mean=True, with_std=True)),
    ("model", RandomForestClassifier(
        n_estimators=300,
        max_depth=None,
        random_state=42,
        n_jobs=-1
    ))
])

# === 5) Trainieren ===
pipeline.fit(X_train, y_train)

# === 6) Evaluieren ===
y_pred = pipeline.predict(X_test)
print("Klassifikationsbericht:")
print(classification_report(y_test, y_pred, zero_division=0))
acc = accuracy_score(y_test, y_pred)
print(f"Genauigkeit: {acc*100:.2f}%")

# === 7) Speichern ===
joblib.dump(pipeline, "coffee_rf_pipeline.pkl")
joblib.dump({"feature_cols": feature_cols}, "model_meta.pkl")
print("Pipeline gespeichert: coffee_rf_pipeline.pkl")
print("Meta gespeichert: model_meta.pkl")
