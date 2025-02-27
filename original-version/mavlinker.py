import subprocess
from dataclasses import dataclass

"""
ADC0    24      0.86V   tensao-bateria
ADC1    75      -55.94A corrente-bombordo
ADC2    75      -55.78A corrente-boreste
ADC3    0       0.02A   corrente-painel
"""

@dataclass
class Measurement:
    channel: int
    adc: int
    value: float
    unit: str
    field_name: str

def extract_value(value: str) -> float:
    return float(value[:-1])

def extract_unit(value: str) -> str:
    return value[-1]

def extract_values(line: str) -> Measurement:
    parts = line.split()
    if len(parts) != 4:
        raise ValueError("Invalid line")
    channel = int(parts[0][3])
    adc = int(parts[1])
    value = extract_value(parts[2])
    unit = extract_unit(parts[2])
    field_name = parts[3]
    return Measurement(channel, adc, value, unit, field_name)

    

process = subprocess.Popen(
    ["stdbuf", "-oL", "build/instrumentation-app", "/dev/i2c-3", "0x48", "configA.txt"],
    stdout = subprocess.PIPE,
    stderr = subprocess.PIPE,
    text = True,
    universal_newlines = True,
    bufsize = 1,
)

for line in process.stdout:
    try:
        measurement = extract_values(line)
    except ValueError:
        continue
    print(measurement)

process.stdout.close()
process.wait()
