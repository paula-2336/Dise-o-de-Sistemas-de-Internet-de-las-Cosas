# Tarea 1: Diseño de Sistemas IoT

## Arquitectura del Sistema
El sistema consta de tres componentes principales conectados mediante Bluetooth Low Energy (BLE):
1. **ESP32 (Servidor BLE):** Simula sensores de acelerómetro (1000 Hz) y temperatura (cada 15s).
2. **Smartphone (Servidor BLE):** Actúa como un periférico adicional enviando datos manuales/personalizados.
3. **Raspberry Pi (Cliente BLE Central):** Gestiona conexiones simultáneas, procesa datos en tiempo real y ofrece una interfaz gráfica (PyQt5).

## UUIDs
- **Servicio Acelerómetro (ESP32):** `0x1800`
  - Característica: `0x2A58` (Notify)
- **Servicio Temperatura (ESP32):** `0x1809`
  - Característica: `0x246E` (Notify)
- **Smartphone:** Configurado según la aplicación (ej. nRF Connect).

## Formato de Paquetes (Binary)
### Acelerómetro (16 bytes)
| Offset | Campo | Tipo |
|--------|-------|------|
| 0 | Timestamp | uint32 |
| 4 | Ax | float32 |
| 8 | Ay | float32 |
| 12| Az | float32 |

### Temperatura (8 bytes)
| Offset | Campo | Tipo |
|--------|-------|------|
| 0 | Timestamp | uint32 |
| 4 | Temp | float32 |

## Instrucciones de Ejecución
### ESP32
1. Instalar ESP-IDF.
2. Navegar a `/esp32/`.
3. Ejecutar `idf.py build flash monitor`.

### Raspberry Pi
1. Instalar dependencias: `pip install -r requirements.txt`.
2. Configurar `config.json` con la MAC del ESP32.
3. Ejecutar `python main.py`.
