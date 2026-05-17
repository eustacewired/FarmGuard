import numpy as np
import pandas as pd
from sklearn.tree import DecisionTreeClassifier
from micromlgen import port

# Set random seed for reproducibility matching JKUAT research conditions
np.random.seed(42)
num_samples = 300

# ==============================================================================
# PHASE 1: SYNTHESIZE THE AGRICULTURAL SENSOR DATASET
# ==============================================================================
# Target Labels: 0 = NORMAL, 1 = DRY_STRESS, 2 = HEAT_STRESS, 3 = SENSOR_FAULT

data = []

for _ in range(num_samples):
    state = np.random.choice([0, 1, 2, 3], p=[0.5, 0.2, 0.2, 0.1])
    
    if state == 0:  # NORMAL: Moderate temp, stable humidity, moist soil
        temp = np.random.uniform(22.0, 30.0)
        hum = np.random.uniform(50.0, 75.0)
        soil = np.random.uniform(40.0, 80.0)
    elif state == 1:  # DRY_STRESS: High/Normal temp, dry soil
        temp = np.random.uniform(24.0, 34.0)
        hum = np.random.uniform(35.0, 55.0)
        soil = np.random.uniform(0.0, 30.0)
    elif state == 2:  # HEAT_STRESS: Extreme ambient heat, low air humidity
        temp = np.random.uniform(36.0, 45.0)
        hum = np.random.uniform(10.0, 30.0)
        soil = np.random.uniform(20.0, 50.0)
    else:  # SENSOR_FAULT: Defective hardware, short-circuits, or open lines
        # Mix in values that match missing or hardware-faulted DHT/ADC boundaries
        fault_type = np.random.choice(["nan_temp", "zero_hum", "max_adc"])
        if fault_type == "nan_temp":
            temp = -99.0  # Sentinel representation for invalid reads
            hum = np.random.uniform(20.0, 80.0)
            soil = np.random.uniform(0.0, 100.0)
        elif fault_type == "zero_hum":
            temp = np.random.uniform(15.0, 40.0)
            hum = 0.0
            soil = np.random.uniform(0.0, 100.0)
        else:
            temp = np.random.uniform(15.0, 40.0)
            hum = np.random.uniform(20.0, 80.0)
            soil = 100.0  # Fully saturated shorted sensor indication

    data.append([temp, hum, soil, state])

# Structure features into a Pandas DataFrame
dataset = pd.DataFrame(data, columns=['temperature', 'humidity', 'soil_moisture', 'target'])
X = dataset[['temperature', 'humidity', 'soil_moisture']].values
y = dataset['target'].values

# ==============================================================================
# PHASE 2: TRAIN THE EMBEDDED DECISION TREE
# ==============================================================================
# We limit max_depth to 4 to prevent overfitting and minimize the C++ nested depth
classifier = DecisionTreeClassifier(max_depth=4)
classifier.fit(X, y)

print(f"Model successfully trained! Feature shapes: {X.shape}")

# ==============================================================================
# PHASE 3: CONVERT MODEL TO C++ FOR ESP32 DEPLOYMENT
# ==============================================================================
# Porting our scikit-learn decision tree directly into low-level code strings
cpp_code = port(classifier, classname="TinyMLBrain", features=['temp', 'hum', 'soil'])

print("\n================ COPY THE CODE BELOW THIS LINE ================\n")
print(cpp_code)
print("\n========================= END OF CODE =========================\n")