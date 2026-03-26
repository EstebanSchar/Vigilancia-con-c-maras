# Sistema de Vigilancia Centralizado con ESP32-CAM

Este proyecto implementa un sistema de monitoreo remoto escalable utilizando nodos de captura basados en **ESP32-CAM** y un **Servidor Central** desarrollado en Python (Flask).

El sistema permite la transmisión fluida de video en tiempo real (MJPEG a 25 FPS) y el control asíncrono de actuadores (servomotores Pan/Tilt e iluminación LED) mediante una interfaz web unificada, aislando la carga de red de los microcontroladores.

## Arquitectura del Sistema

El proyecto se divide en dos componentes principales:
1. **Firmware del Nodo (ESP-IDF / FreeRTOS):** Escrito en C. Utiliza una arquitectura de afinidad de núcleos (Multicore) y servidores HTTP independientes (Dual-Server) en los puertos 80 y 81 para evitar el bloqueo concurrente entre el streaming de video y el control de hardware.
2. **Servidor Central (Python/Flask):** Actúa como túnel proxy utilizando OpenCV para decodificar y retransmitir el video, y procesa las peticiones REST (JSON) del frontend asíncrono para comandar el hardware.

## Hardware y Conexionado (Pinout)

El nodo de captura utiliza el módulo de cámara OV2640 y se conecta a los siguientes periféricos:

| Componente | Pin ESP32-CAM | Función |
| :--- | :--- | :--- |
| **Servo Pan (Horizontal)** | `GPIO 13` | Señal PWM (50 Hz) |
| **Servo Tilt (Vertical)** | `GPIO 12` | Señal PWM (50 Hz) |
| **Flash LED** | `GPIO 4` | Control de intensidad PWM (5 kHz) |
| **Debug Pin** | `GPIO 14` | Monitoreo de FPS vía Analizador Lógico |

## Instalación y Uso

### 1. Configuración del Nodo ESP32
El firmware se desarrollo utilizando el framework **ESP-IDF**.
1. Clonar el repositorio.
2. Configurar las credenciales de red Wi-Fi estáticas en el archivo `app_wifi.c` (IP, Gateway y Máscara de subred).
3. Compilar y flashear en el ESP32-CAM:
   ```bash
   idf.py build
   idf.py -p COMx flash monitor

### 2. Configuración del Servidor Central

Se requiere Python 3.x instalado en el equipo que actuará como servidor.
1. Navegar a la carpeta del servidor:
   ```bash
   cd Servidor_Central
2. Instalar las dependencias necesarias:
    ```bash
    pip install Flask opencv-python requests
3. Configurar la IP de los nodos: Abrir el archivo app.py y verificar que las direcciones IP de las cámaras coincidan con las IPs estáticas configuradas en el firmware.
4. Ejecutar la aplicación:
    ```bash
    python app.py
5. Acceder desde cualquier navegador en la misma red local ingresando a:
    ```bash
    http://localhost:5000
