#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>    
#include <Preferences.h>   

// ==============================================================================
// DEFINICIÓN DE PINES
// ==============================================================================
const int ZCD_PIN = 13;    
const int TRIAC1_PIN = 5;  
const int TRIAC2_PIN = 18; 
const int POT_PIN = 34;    

const int tiempo_semiciclo = 8333;

// ==============================================================================
// VARIABLES GLOBALES (DINÁMICAS Y CONFIGURABLES)
// ==============================================================================
Preferences preferencias;

int alpha_objetivo = 180;         
volatile int alpha_actual = 180;  
volatile bool sistema_encendido = false; 
volatile bool invertir_canales = false;  

// Variables de sintonización en caliente (Cargadas desde Flash)
volatile int umbral_apagado = 160; 
volatile int velocidad_rampa = 15; 

unsigned long tiempoAnteriorSoftStart = 0;

volatile int estado_timer = 0;    
const uint32_t ancho_pulso_us = 1000; 
hw_timer_t *timer = NULL;         

volatile bool es_semiciclo_positivo = true; 
volatile unsigned long tiempo_ultimo_cruce = 0;

const char* ssid = "Control_Potencia";
const char* password = "laboratorioel"; 
AsyncWebServer server(80);
TaskHandle_t TareaWeb;

// ==============================================================================
// RUTINAS DE INTERRUPCIÓN
// ==============================================================================

void IRAM_ATTR onTimer() {
  if (estado_timer == 1) { 
    bool disparar_canal_1 = es_semiciclo_positivo;
    if (invertir_canales) {
      disparar_canal_1 = !disparar_canal_1; 
    }

    if (disparar_canal_1) {
      digitalWrite(TRIAC1_PIN, HIGH);  
    } else {
      digitalWrite(TRIAC2_PIN, HIGH);  
    }
    
    estado_timer = 2;
    timerAlarmWrite(timer, ancho_pulso_us, false);
    timerWrite(timer, 0);
    timerAlarmEnable(timer);
  } else if (estado_timer == 2) {
    digitalWrite(TRIAC1_PIN, LOW);   
    digitalWrite(TRIAC2_PIN, LOW);
    estado_timer = 0; 
  }
}

void IRAM_ATTR zeroCrossISR() {
  unsigned long tiempo_actual = micros();
  if (tiempo_actual - tiempo_ultimo_cruce < 6000) {
    return; 
  }
  tiempo_ultimo_cruce = tiempo_actual; 

  timerAlarmDisable(timer);
  
  if (!sistema_encendido || alpha_actual >= umbral_apagado) {
    digitalWrite(TRIAC1_PIN, LOW);
    digitalWrite(TRIAC2_PIN, LOW);
    return;
  }
  
  es_semiciclo_positivo = !es_semiciclo_positivo;
  
  uint32_t retardo_us = (alpha_actual * tiempo_semiciclo) / 180;
  if(retardo_us < 10) retardo_us = 10; 
  if(retardo_us > 8300) return; 

  estado_timer = 1; 
  timerAlarmWrite(timer, retardo_us, false); 
  timerWrite(timer, 0); 
  timerAlarmEnable(timer);
}

// ==============================================================================
// INTERFAZ WEB CON PANEL DE CONFIGURACIÓN OCULTO
// ==============================================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Práctica 5 | Control Avanzado</title>
  <style>
    * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
    body { background-color: #111827; color: #ffffff; font-family: system-ui, sans-serif; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; }
    .container { background-color: #1f2937; width: 100%; max-width: 400px; padding: 24px; border-radius: 16px; border: 1px solid #374151; display: flex; flex-direction: column; gap: 20px; box-shadow: 0 10px 25px rgba(0,0,0,0.5); position: relative; }
    .header { display: flex; justify-content: space-between; align-items: center; width: 100%; }
    .title-group { display: flex; flex-direction: column; gap: 4px; }
    h1 { margin: 0; font-size: 24px; color: #22d3ee; font-weight: 800; }
    .subtitle { margin: 0; font-size: 11px; color: #9ca3af; text-transform: uppercase; letter-spacing: 1px; }
    
    .config-trigger { background: none; border: none; font-size: 24px; color: #9ca3af; cursor: pointer; transition: color 0.2s; padding: 4px; }
    .config-trigger:hover { color: #22d3ee; }

    .btn-maestro { width: 100%; padding: 16px; border-radius: 12px; border: none; font-size: 18px; font-weight: 900; cursor: pointer; text-transform: uppercase; transition: all 0.2s; letter-spacing: 1px; }
    .power-off { background-color: #ef4444; color: white; box-shadow: 0 5px #b91c1c; }
    .power-off:active { box-shadow: 0 0 #b91c1c; transform: translateY(5px); }
    .power-on { background-color: #10b981; color: white; box-shadow: 0 5px #047857; }
    .power-on:active { box-shadow: 0 0 #047857; transform: translateY(5px); }

    .invert-btn { background-color: #4b5563; color: #e5e7eb; border: 1px solid #6b7280; font-size: 13px; font-weight: 700; padding: 12px; border-radius: 10px; width: 100%; cursor: pointer; text-transform: uppercase; letter-spacing: 1px; }
    .invert-active { background-color: #7c3aed !important; color: white !important; border-color: #a78bfa !important; box-shadow: 0 0 12px rgba(124, 58, 237, 0.4); }

    .display-box { background-color: #111827; padding: 20px; border-radius: 12px; text-align: center; border: 1px solid #374151; display: flex; flex-direction: column; align-items: center; width: 100%; position: relative; }
    .display-box span.lbl { color: #6b7280; font-size: 12px; font-weight: 700; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 5px; }
    
    .input-wrapper { display: flex; align-items: center; justify-content: center; margin-bottom: 15px; }
    .value-input { background: transparent; border: none; font-size: 56px; font-weight: 900; color: #facc15; padding: 0; margin: 0; width: 110px; text-align: right; outline: none; font-family: inherit; }
    .value-input::-webkit-outer-spin-button, .value-input::-webkit-inner-spin-button { -webkit-appearance: none; margin: 0; }
    .degree-symbol { font-size: 40px; font-weight: 900; color: #facc15; margin-left: 2px; }
    
    .value-off, .value-off + .degree-symbol { color: #4b5563 !important; } 
    .ramping, .ramping + .degree-symbol { color: #f97316 !important; } 
    
    .canvas-container { width: 100%; height: 80px; background: #0f172a; border-radius: 8px; border: 1px solid #334155; position: relative; overflow: hidden; }
    canvas { display: block; width: 100%; height: 100%; }
    .sync-dot { position: absolute; top: 12px; right: 12px; width: 8px; height: 8px; background-color: #10b981; border-radius: 50%; box-shadow: 0 0 8px #10b981; }
    
    .labels-container { display: flex; justify-content: space-between; width: 100%; padding: 0 5px; margin-bottom: -5px; }
    .tick-label { color: #9ca3af; font-size: 10px; font-weight: 700; width: 20px; text-align: center; }
    .slider { width: 100%; height: 8px; border-radius: 4px; background: #374151; outline: none; -webkit-appearance: none; margin: 10px 0; }
    .slider::-webkit-slider-thumb { -webkit-appearance: none; width: 28px; height: 28px; border-radius: 50%; background: #22d3ee; }

    .modal-overlay { display: none; position: absolute; top: 0; left: 0; width: 100%; height: 100%; background: rgba(17, 24, 39, 0.95); border-radius: 16px; padding: 24px; flex-direction: column; gap: 20px; z-index: 10; border: 1px solid #4b5563; }
    .modal-header { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #374151; padding-bottom: 10px; }
    .modal-title { font-size: 18px; font-weight: 800; color: #22d3ee; margin: 0; }
    .close-btn { background: none; border: none; color: #ef4444; font-size: 20px; font-weight: 900; cursor: pointer; }
    .config-group { display: flex; flex-direction: column; gap: 6px; }
    .config-group label { font-size: 12px; color: #9ca3af; font-weight: 700; text-transform: uppercase; }
    .config-val { font-size: 14px; color: #facc15; font-weight: 800; text-align: right; margin-top: -20px; }
  </style>
  <script>
    let isInteracting = false; 
    let sysPower = false; 
    let sysInvert = false;
    let limitUmbral = 160;

    function togglePower() {
      sysPower = !sysPower;
      updatePowerUI();
      fetch('/power?state=' + (sysPower ? '1' : '0'));
    }

    function toggleInvert() {
      sysInvert = !sysInvert;
      updateInvertUI();
      fetch('/invert?state=' + (sysInvert ? '1' : '0'));
    }

    function updatePowerUI() {
      const btn = document.getElementById('powerBtn');
      const valInput = document.getElementById('alphaInput');
      if (sysPower) {
        btn.innerText = '🔴 DETENER SISTEMA';
        btn.className = 'btn-maestro power-off';
        valInput.classList.remove('value-off');
      } else {
        btn.innerText = '🟢 INICIAR SISTEMA';
        btn.className = 'btn-maestro power-on';
        valInput.classList.add('value-off');
        valInput.classList.remove('ramping');
      }
    }

    function updateInvertUI() {
      const btn = document.getElementById('invertBtn');
      if (sysInvert) {
        btn.innerText = '🔄 CANALES INVERTIDOS (LOGIC SWAP)';
        btn.classList.add('invert-active');
      } else {
        btn.innerText = '⚡ POLARIDAD NORMAL';
        btn.classList.remove('invert-active');
      }
    }

    function drawWave(alphaActual) {
      const canvas = document.getElementById('waveCanvas');
      const ctx = canvas.getContext('2d');
      canvas.width = canvas.offsetWidth;
      canvas.height = canvas.offsetHeight;
      const w = canvas.width; const h = canvas.height;
      
      ctx.clearRect(0, 0, w, h);
      ctx.beginPath(); ctx.moveTo(0, h/2); ctx.lineTo(w, h/2);
      ctx.strokeStyle = '#334155'; ctx.lineWidth = 1; ctx.stroke();

      for(let x = 0; x < w; x++) {
        let deg = (x / w) * 360; 
        let y = (h/2) - (Math.sin(deg * (Math.PI / 180)) * (h/2 - 10)); 
        let cycleDeg = deg % 180; 
        
        ctx.beginPath();
        ctx.arc(x, y, 1.5, 0, Math.PI * 2);
        
        let renderAngle = alphaActual >= limitUmbral ? 180 : alphaActual;
        
        if (!sysPower || cycleDeg < renderAngle) {
          ctx.fillStyle = '#4b5563'; 
        } else {
          ctx.fillStyle = '#facc15'; 
          ctx.lineTo(x, h/2);
          ctx.strokeStyle = 'rgba(250, 204, 21, 0.1)';
          ctx.stroke();
        }
        ctx.fill();
      }
    }

    function sendAlphaUpdate(val) {
      let num = parseInt(val);
      if(isNaN(num)) return;
      if(num < 0) num = 0;
      if(num > 180) num = 180;
      
      document.getElementById('slider').value = num;
      document.getElementById('alphaInput').value = num;
      fetch('/set?alpha=' + num);
    }

    function sendConfigUpdate() {
      const umbral = document.getElementById('cfgUmbral').value;
      const rampa = document.getElementById('cfgRampa').value;
      limitUmbral = parseInt(umbral);
      
      document.getElementById('lblUmbral').innerText = umbral + '°';
      document.getElementById('lblRampa').innerText = rampa + ' ms';
      
      fetch('/config?umbral=' + umbral + '&rampa=' + rampa);
    }

    function toggleModal(show) {
      document.getElementById('configModal').style.display = show ? 'flex' : 'none';
    }

    window.onload = function() {
      const alphaInput = document.getElementById('alphaInput');
      const slider = document.getElementById('slider');

      slider.addEventListener('input', (e) => { alphaInput.value = e.target.value; });
      alphaInput.addEventListener('focus', () => { isInteracting = true; });
      alphaInput.addEventListener('blur', () => { isInteracting = false; sendAlphaUpdate(alphaInput.value); });
      alphaInput.addEventListener('keypress', (e) => { if(e.key === 'Enter') { alphaInput.blur(); } });

      fetch('/get')
        .then(response => response.text())
        .then(val => {
          let data = val.split(',');
          sysPower = (data[1] === '1');
          sysInvert = (data[3] === '1');
          limitUmbral = parseInt(data[4]);
          
          updatePowerUI();
          updateInvertUI();
          
          alphaInput.value = data[2];
          slider.value = data[0];
          
          document.getElementById('cfgUmbral').value = data[4];
          document.getElementById('lblUmbral').innerText = data[4] + '°';
          document.getElementById('cfgRampa').value = data[5];
          document.getElementById('lblRampa').innerText = data[5] + ' ms';
          
          drawWave(parseInt(data[2]));
        });
    };

    setInterval(() => {
      if (!isInteracting) {
        fetch('/get')
          .then(response => response.text())
          .then(val => {
            let data = val.split(',');
            let sObjetivo = data[0];
            let sPower = (data[1] === '1');
            let sActual = data[2];
            let sInvert = (data[3] === '1');

            if(sysPower !== sPower) { sysPower = sPower; updatePowerUI(); }
            if(sysInvert !== sInvert) { sysInvert = sInvert; updateInvertUI(); }

            document.getElementById('slider').value = sObjetivo;
            document.getElementById('alphaInput').value = sActual;
            
            if(sysPower && sObjetivo !== sActual) {
                document.getElementById('alphaInput').classList.add('ramping');
            } else {
                document.getElementById('alphaInput').classList.remove('ramping');
            }
            drawWave(parseInt(sActual));
          });
      }
    }, 100);
  </script>
</head>
<body>

  <div class="container">
    <div id="configModal" class="modal-overlay">
      <div class="modal-header">
        <h2 class="modal-title">⚙️ Parámetros de Calibración</h2>
        <button class="close-btn" onclick="toggleModal(false)">✕</button>
      </div>
      
      <div class="config-group">
        <label>Umbral de Apagado (Mute)</label>
        <input type="range" min="140" max="175" id="cfgUmbral" class="slider" oninput="sendConfigUpdate()">
        <div id="lblUmbral" class="config-val">160°</div>
      </div>

      <div class="config-group">
        <label>Velocidad de Rampa Soft-Start</label>
        <input type="range" min="5" max="50" id="cfgRampa" class="slider" oninput="sendConfigUpdate()">
        <div id="lblRampa" class="config-val">15 ms</div>
      </div>
    </div>

    <div class="header">
      <div class="title-group">
        <h1>Práctica 5</h1>
        <p class="subtitle">Monitoreo de Fase Rectificada</p>
      </div>
      <button class="config-trigger" onclick="toggleModal(true)">⚙️</button>
    </div>

    <button id="powerBtn" onclick="togglePower()" class="btn-maestro power-on">🟢 INICIAR SISTEMA</button>
    <button id="invertBtn" onclick="toggleInvert()" class="invert-btn">⚡ POLARIDAD NORMAL</button>

    <div class="display-box">
      <div class="sync-dot" title="Enlace en vivo"></div>
      <span class="lbl">Ángulo Real (SCR)</span>
      
      <div class="input-wrapper">
        <input type="number" id="alphaInput" class="value-input value-off" min="0" max="180" value="180">
        <span class="degree-symbol">°</span>
      </div>

      <div class="canvas-container"><canvas id="waveCanvas"></canvas></div>
    </div>

    <div style="width: 100%; display: flex; flex-direction: column; gap: 5px;">
      <div class="labels-container">
        <span class="tick-label">0</span>
        <span class="tick-label">30</span>
        <span class="tick-label">60</span>
        <span class="tick-label">90</span>
        <span class="tick-label">120</span>
        <span class="tick-label">150</span>
        <span class="tick-label">180</span>
      </div>
      
      <input type="range" min="0" max="180" value="180" id="slider" class="slider" list="alphas"
             onmousedown="isInteracting=true" ontouchstart="isInteracting=true" 
             onmouseup="isInteracting=false; sendAlphaUpdate(this.value)"
             ontouchend="isInteracting=false; sendAlphaUpdate(this.value)">
      
      <datalist id="alphas">
        <option value="0"></option><option value="30"></option><option value="60"></option>
        <option value="90"></option><option value="120"></option><option value="150"></option>
        <option value="180"></option>
      </datalist>
    </div>
  </div>

</body>
</html>
)rawliteral";

// ==============================================================================
// TAREA DEL NÚCLEO 0 (Wi-Fi, OTA y Web)
// ==============================================================================
void codigoWeb(void * parameter) {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  
  ArduinoOTA.setHostname("ESP32_Potencia");
  ArduinoOTA.begin();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("alpha")) {
      alpha_objetivo = request->getParam("alpha")->value().toInt(); 
      if(alpha_objetivo > 180) alpha_objetivo = 180;
      preferencias.putInt("alpha_obj", alpha_objetivo);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("umbral")) {
      umbral_apagado = request->getParam("umbral")->value().toInt();
      preferencias.putInt("cfg_umbral", umbral_apagado);
    }
    if (request->hasParam("rampa")) {
      velocidad_rampa = request->getParam("rampa")->value().toInt();
      preferencias.putInt("cfg_rampa", velocidad_rampa);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/power", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      sistema_encendido = (request->getParam("state")->value() == "1"); 
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/invert", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      invertir_canales = (request->getParam("state")->value() == "1"); 
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request){
    String payload = String(alpha_objetivo) + "," + 
                     String(sistema_encendido ? 1 : 0) + "," + 
                     String(alpha_actual) + "," + 
                     String(invertir_canales ? 1 : 0) + "," +
                     String(umbral_apagado) + "," +
                     String(velocidad_rampa);
    request->send(200, "text/plain", payload);
  });

  server.begin();

  for(;;) {
    ArduinoOTA.handle(); 
    vTaskDelay(20 / portTICK_PERIOD_MS); 
  }
}

// ==============================================================================
// CONFIGURACIÓN PRINCIPAL (NÚCLEO 1)
// ==============================================================================
void setup() {
  Serial.begin(115200);

  preferencias.begin("control", false);
  alpha_objetivo = preferencias.getInt("alpha_obj", 180);
  umbral_apagado = preferencias.getInt("cfg_umbral", 160);
  velocidad_rampa = preferencias.getInt("cfg_rampa", 15);

  pinMode(ZCD_PIN, INPUT); 
  
  pinMode(TRIAC1_PIN, OUTPUT);
  pinMode(TRIAC2_PIN, OUTPUT);
  digitalWrite(TRIAC1_PIN, LOW); 
  digitalWrite(TRIAC2_PIN, LOW); 

  analogReadResolution(12);

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  
  attachInterrupt(digitalPinToInterrupt(ZCD_PIN), zeroCrossISR, RISING);

  xTaskCreatePinnedToCore(codigoWeb, "Tarea_WiFi", 10000, NULL, 1, &TareaWeb, 0);
}

// ==============================================================================
// BUCLE PRINCIPAL
// ==============================================================================
void loop() {
  static float pot_filtrado = -1;
  int rawValue = analogRead(POT_PIN);

  if (pot_filtrado < 0) {
    pot_filtrado = rawValue;
  }

  pot_filtrado = (0.05 * rawValue) + (0.95 * pot_filtrado);

  int pot_suave = round(pot_filtrado);
  static int last_pot_suave = pot_suave;

  if (abs(pot_suave - last_pot_suave) > 15) {
    last_pot_suave = pot_suave;
    alpha_objetivo = map(pot_suave, 0, 4095, 0, 180);
  }

  if (sistema_encendido) {
    if (millis() - tiempoAnteriorSoftStart >= (unsigned long)velocidad_rampa) {
      tiempoAnteriorSoftStart = millis();
      
      if (alpha_actual > alpha_objetivo) alpha_actual--;
      else if (alpha_actual < alpha_objetivo) alpha_actual++;
    }
  } else {
    alpha_actual = 180;
  }

  delay(2); 
}