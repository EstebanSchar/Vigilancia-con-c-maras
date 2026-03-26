from flask import Flask, render_template, Response, request, jsonify
import cv2
import requests
import time

app = Flask(__name__)

# === CONFIGURACIÓN DE CÁMARAS Y ESTADO ===
CAMERAS = {
    "1": {
        "ip": "192.168.137.201", 
        "name": "Cámara 1",
        "pan": 90,  # Estado inicial guardado
        "tilt": 90,
        "led": 0
    },
    "2": {
        "ip": "192.168.137.202", 
        "name": "Cámara 2",
        "pan": 90,
        "tilt": 90,
        "led": 0
    }
}

# Función auxiliar para obtener URL base
def get_base_url(cam_id):
    cam = CAMERAS.get(cam_id)
    if not cam:
        return None
    return f"http://{cam['ip']}"

# === STREAM GENERATOR ===
def generate_stream(ip):
    stream_url = f"http://{ip}:81/stream"
    print(f"Conectando al stream: {stream_url}")
    
    cap = cv2.VideoCapture(stream_url)  #Captura el flujo de video de la IP establecida por el puerto 81.
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1) #Guardamos solo un frame en el buffer.
    
    while True:
        success, frame = cap.read()
        if not success:
            # Si falla, intenta reconectar sin tumbar el servidor
            time.sleep(2)
            cap = cv2.VideoCapture(stream_url)
            continue

        ret, buffer = cv2.imencode('.jpg', frame, [int(cv2.IMWRITE_JPEG_QUALITY), 70])
        frame_bytes = buffer.tobytes()
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')

@app.route('/')
def index():
    # Pasamos la lista de cámaras al HTML
    return render_template('index.html', cameras=CAMERAS)

@app.route('/video_feed/<cam_id>')
def video_feed(cam_id):
    cam = CAMERAS.get(cam_id)
    if cam:
        return Response(generate_stream(cam['ip']),
                        mimetype='multipart/x-mixed-replace; boundary=frame')
    return "Cámara no encontrada", 404

# === NUEVA RUTA: OBTENER ESTADO ===
# Esto permite que al cambiar de cámara, los sliders se actualicen solos
@app.route('/get_state/<cam_id>')
def get_state(cam_id):
    cam = CAMERAS.get(cam_id)
    if cam:
        return jsonify({
            "pan": cam["pan"],
            "tilt": cam["tilt"],
            "led": cam["led"]
        })
    return jsonify({}), 404

# === CONTROL DE LED ===
@app.route('/led/<cam_id>', methods=['POST'])
def led(cam_id):
    base_url = get_base_url(cam_id)
    if not base_url: return jsonify({"status": "error"}), 400

    try:
        data = request.json
        pwm_value = int(data.get("intensity"))
        
        # 1. Enviar comando físico
        requests.get(f"{base_url}:80/led", params={'val': pwm_value}, timeout=3.0)
        
        # 2. Guardar en memoria
        CAMERAS[cam_id]["led"] = pwm_value
        
        return jsonify({"status": "ok"})
    except Exception as e:
        print(f"Error LED: {e}")
        return jsonify({"status": "error"}), 500

# === CONTROL DE SERVOS ===
@app.route('/move_servo/<cam_id>', methods=['POST'])
def move_servo(cam_id):
    base_url = get_base_url(cam_id)
    if not base_url: return jsonify({"status": "error"}), 400

    try:
        data = request.json
        # Usamos los valores actuales si no se envían nuevos
        current_pan = CAMERAS[cam_id]["pan"]
        current_tilt = CAMERAS[cam_id]["tilt"]

        new_pan = data.get("pan", current_pan)
        new_tilt = data.get("tilt", current_tilt)
        
        # 1. Enviar comando físico
        payload = {'pan': new_pan, 'tilt': new_tilt}
        requests.get(f"{base_url}:80/servo", params=payload, timeout=1.0)
        
        # 2. Guardar en memoria
        CAMERAS[cam_id]["pan"] = int(new_pan)
        CAMERAS[cam_id]["tilt"] = int(new_tilt)

        return jsonify({"status": "ok"})
    except Exception as e:
        print(f"Error Servo: {e}")
        return jsonify({"status": "error"}), 500
    
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True, threaded=True)
