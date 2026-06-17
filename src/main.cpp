#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>    
#include <Preferences.h>   

#include "interfaz_web.h"
#include "algoritmo_control.h"

// ==============================================================================
// DEFINICIÓN DE PINES (Sin Potenciómetro Analógico)
// ==============================================================================
const int ZCD_PIN = 13;    
const int SCR_PAR_P_PIN = 5;  
const int SCR_PAR_N_PIN = 18; 
const int TACO_PIN = 35;   

// ==============================================================================
// VARIABLES DE POTENCIA Y PLL DEDICADO
// ==============================================================================
volatile uint32_t tiempo_semiciclo = 8333; 
volatile int estado_timer = 0;             
volatile bool es_semiciclo_positivo = true; 
volatile unsigned long tiempo_ultimo_cruce = 0;
volatile uint32_t retardo_actual_us = 0; 
hw_timer_t *timer = NULL;         

volatile int zcd_muestras = 0;
volatile uint32_t zcd_suma_tiempos = 0;
volatile bool zcd_sintonizado = false;

// ==============================================================================
// VARIABLES DE SISTEMA Y CONFIGURACIÓN
// ==============================================================================
Preferences preferencias;

int modo_operacion = 0;           
volatile bool sistema_encendido = false; 

int limite_inferior_alfa = 10;
int limite_superior_alfa = 150;
int desfase_zcd_us = 0;

int alpha_objetivo = 150;         
volatile int alpha_actual = 180; 

int rpm_objetivo_web = 0; 

volatile int velocidad_rampa_on = 15; 
volatile int velocidad_rampa_off = 45;
volatile int velocidad_rampa_p7 = 3;
volatile int ancho_pulso_us = 1000;  

bool invertir_canales = false;

unsigned long tiempoAnteriorRampa = 0;

float voltaje_taco_actual = 0.0;
int rpm_actual = 0;

float calib_voltaje[10];
float calib_rpm[10];

const char* ssid = "Control_Potencia";
const char* password = "laboratorioel"; 
AsyncWebServer server(80);
TaskHandle_t TareaWeb;

// ==============================================================================
// RUTINAS DE INTERRUPCIÓN (NÚCLEO 1) - CAPA DE HARDWARE PURA
// ==============================================================================
void IRAM_ATTR onTimer() {
  int pin_par_p = invertir_canales ? SCR_PAR_N_PIN : SCR_PAR_P_PIN;
  int pin_par_n = invertir_canales ? SCR_PAR_P_PIN : SCR_PAR_N_PIN;
  
  if (estado_timer == 1) { 
    if (es_semiciclo_positivo) {
      GPIO.out_w1ts = ((uint32_t)1 << pin_par_p); 
    } else {
      GPIO.out_w1ts = ((uint32_t)1 << pin_par_n); 
    }
    
    uint32_t margen_seguridad = 250; 
    uint32_t tiempo_restante = tiempo_semiciclo - retardo_actual_us;
    uint32_t pulso = ancho_pulso_us; 
    
    if (tiempo_restante < (ancho_pulso_us + margen_seguridad)) {
        pulso = (tiempo_restante > margen_seguridad) ? (tiempo_restante - margen_seguridad) : 10; 
    }

    estado_timer = 2;
    timerAlarmWrite(timer, pulso, false);
    timerWrite(timer, 0);
    timerAlarmEnable(timer);
    
  } else if (estado_timer == 2) {
    GPIO.out_w1tc = ((uint32_t)1 << pin_par_p) | ((uint32_t)1 << pin_par_n);
    estado_timer = 0; 
  }
}

void IRAM_ATTR zeroCrossISR() {
  unsigned long tiempo_actual = micros();
  unsigned long delta = tiempo_actual - tiempo_ultimo_cruce;

  // GUILLOTINA: Apagar SCRs de inmediato
  GPIO.out_w1tc = ((uint32_t)1 << SCR_PAR_P_PIN) | ((uint32_t)1 << SCR_PAR_N_PIN);
  timerAlarmDisable(timer);
  estado_timer = 0; 
  
  // FILTRO ANTI-REBOTE CLÁSICO: Ignora el "chispazo" del switch sin actualizar el tiempo
  if (delta < 6500) return; 

  if (!zcd_sintonizado) {
    // VENTANA CLÍNICA (7.5ms a 10ms). Solo acepta ondas puras para aprender.
    if (delta > 7500 && delta < 10000) {
      zcd_suma_tiempos += delta;
      zcd_muestras++;
      es_semiciclo_positivo = !es_semiciclo_positivo; 
      
      if (zcd_muestras >= 60) {
        tiempo_semiciclo = zcd_suma_tiempos / 60;
        zcd_sintonizado = true;
      }
    } else {
      zcd_muestras = 0;
      zcd_suma_tiempos = 0;
    }
    tiempo_ultimo_cruce = tiempo_actual;
    return; 
  }
  
  // === MODO SINTONIZADO (RUNNING) ===
  
  if (delta > 10000 || delta < 7500) {
     if (delta > 10000) {
        int ciclos_perdidos = round((float)delta / tiempo_semiciclo);
        if (ciclos_perdidos % 2 != 0) {
            es_semiciclo_positivo = !es_semiciclo_positivo; 
        }
     }
     tiempo_ultimo_cruce = tiempo_actual;
     return; 
  }

  tiempo_semiciclo = (tiempo_semiciclo * 63 + delta) / 64;
  tiempo_ultimo_cruce = tiempo_actual; 
  es_semiciclo_positivo = !es_semiciclo_positivo;
  
  // MURO DE SILENCIO ABSOLUTO
  if (!sistema_encendido && alpha_actual >= limite_superior_alfa) return;
  if (alpha_actual > limite_superior_alfa) return; 
  
  int retardo_teorico = (alpha_actual * tiempo_semiciclo) / 180;
  retardo_actual_us = retardo_teorico + desfase_zcd_us;
  
  if(retardo_actual_us < 150) retardo_actual_us = 150; 
  if(retardo_actual_us >= (tiempo_semiciclo - 350)) return; 

  estado_timer = 1; 
  timerAlarmWrite(timer, retardo_actual_us, false); 
  timerWrite(timer, 0); 
  timerAlarmEnable(timer);
}

// ==============================================================================
// METROLOGÍA / INTERPOLACIÓN
// ==============================================================================
int extrapolarRPM(float v) {
  if (v <= calib_voltaje[0]) return calib_rpm[0];
  for (int i = 0; i < 9; i++) {
    if (v >= calib_voltaje[i] && v <= calib_voltaje[i+1]) {
      float deltaV = calib_voltaje[i+1] - calib_voltaje[i];
      if (deltaV == 0) return calib_rpm[i];
      float deltaRPM = calib_rpm[i+1] - calib_rpm[i];
      return round(calib_rpm[i] + ((v - calib_voltaje[i]) / deltaV) * deltaRPM);
    }
  }
  if (v > calib_voltaje[9]) {
    if(calib_voltaje[9] == 0) return 0;
    float factor = calib_rpm[9] / calib_voltaje[9];
    return round(v * factor);
  }
  return 0;
}

// ==============================================================================
// TAREA NÚCLEO 0
// ==============================================================================
void tareaNucleoCero(void * parameter) {
  preferencias.begin("control", false);
  
  kp_lineal = preferencias.getFloat("kp", 0.0015);
  ki_lineal = preferencias.getFloat("ki", 0.0080);
  
  rpm_objetivo_web = preferencias.getInt("rpm_obj", 0); 
  modo_operacion = preferencias.getInt("modo", 0);
  
  limite_inferior_alfa = preferencias.getInt("lim_inf", 10);
  limite_superior_alfa = preferencias.getInt("lim_sup", 150);
  velocidad_rampa_on = preferencias.getInt("rampa_on", 15);
  velocidad_rampa_off = preferencias.getInt("rampa_off", 45);
  velocidad_rampa_p7 = preferencias.getInt("rampa_p7", 3);
  ancho_pulso_us = preferencias.getInt("pulso", 1000);
  desfase_zcd_us = preferencias.getInt("desfase", 0);
  invertir_canales = preferencias.getBool("inv_ch", false);
  
  alpha_actual = limite_superior_alfa + 1; 
  alpha_objetivo = limite_superior_alfa; 

  for(int i=0; i<10; i++) {
    String keyV = "v" + String(i);
    String keyR = "r" + String(i);
    calib_voltaje[i] = preferencias.getFloat(keyV.c_str(), 0.33 * (i+1));
    calib_rpm[i] = preferencias.getFloat(keyR.c_str(), 100.0 * (i+1));
  }
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  ArduinoOTA.begin();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.on("/modo", HTTP_GET, [](AsyncWebServerRequest *request){
    if (sistema_encendido) { request->send(400, "text/plain", "DENEGADO"); return; }
    if (request->hasParam("val")) {
      modo_operacion = request->getParam("val")->value().toInt(); 
      preferencias.putInt("modo", modo_operacion);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/recalib_zcd", HTTP_GET, [](AsyncWebServerRequest *request){
    sistema_encendido = false; 
    zcd_sintonizado = false;
    zcd_muestras = 0;
    zcd_suma_tiempos = 0;
    GPIO.out_w1tc = ((uint32_t)1 << SCR_PAR_P_PIN) | ((uint32_t)1 << SCR_PAR_N_PIN);
    timerAlarmDisable(timer);
    estado_timer = 0;
    alpha_actual = limite_superior_alfa + 1;
    request->send(200, "text/plain", "OK");
  });
  
  server.on("/config_p5", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("lim_inf")) { limite_inferior_alfa = request->getParam("lim_inf")->value().toInt(); preferencias.putInt("lim_inf", limite_inferior_alfa); }
    if (request->hasParam("lim_sup")) { 
        int val_web = request->getParam("lim_sup")->value().toInt(); 
        // BLOQUEO HARDWARE CONTRA CORTOCIRCUITOS CRUZADOS
        limite_superior_alfa = constrain(val_web, limite_inferior_alfa, 160); 
        preferencias.putInt("lim_sup", limite_superior_alfa); 
    }
    if (request->hasParam("rampa_on")) { velocidad_rampa_on = request->getParam("rampa_on")->value().toInt(); preferencias.putInt("rampa_on", velocidad_rampa_on); }
    if (request->hasParam("rampa_off")) { velocidad_rampa_off = request->getParam("rampa_off")->value().toInt(); preferencias.putInt("rampa_off", velocidad_rampa_off); }
    if (request->hasParam("pulso")) { ancho_pulso_us = request->getParam("pulso")->value().toInt(); preferencias.putInt("pulso", ancho_pulso_us); }
    if (request->hasParam("desfase")) { desfase_zcd_us = request->getParam("desfase")->value().toInt(); preferencias.putInt("desfase", desfase_zcd_us); }
    if (request->hasParam("rampa_p7")) { velocidad_rampa_p7 = request->getParam("rampa_p7")->value().toInt(); preferencias.putInt("rampa_p7", velocidad_rampa_p7); }
    request->send(200, "text/plain", "OK");
  });

  server.on("/invert_ch", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      invertir_canales = (request->getParam("state")->value() == "1");
      preferencias.putBool("inv_ch", invertir_canales);
    }
    request->send(200, "text/plain", String(invertir_canales ? "1" : "0"));
  });

  server.on("/save_calib", HTTP_GET, [](AsyncWebServerRequest *request){
    for(int i=0; i<10; i++) {
      String keyV = "v" + String(i);
      String keyR = "rpm" + String(i);
      if(request->hasParam(keyV.c_str()) && request->hasParam(keyR.c_str())) {
        calib_voltaje[i] = request->getParam(keyV.c_str())->value().toFloat();
        calib_rpm[i] = request->getParam(keyR.c_str())->value().toFloat();
        preferencias.putFloat(String("v"+String(i)).c_str(), calib_voltaje[i]);
        preferencias.putFloat(String("r"+String(i)).c_str(), calib_rpm[i]);
      }
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("alpha")) {
      int a = request->getParam("alpha")->value().toInt(); 
      alpha_objetivo = constrain(a, limite_inferior_alfa, limite_superior_alfa);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/setpoint", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("rpm")) {
      rpm_objetivo_web = request->getParam("rpm")->value().toInt();
      preferencias.putInt("rpm_obj", rpm_objetivo_web);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/tune", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("kp")) { kp_lineal = request->getParam("kp")->value().toFloat(); preferencias.putFloat("kp", kp_lineal); }
    if (request->hasParam("ki")) { ki_lineal = request->getParam("ki")->value().toFloat(); preferencias.putFloat("ki", ki_lineal); }
    error_acumulado = 0.0; 
    request->send(200, "text/plain", "OK");
  });

  server.on("/power", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      sistema_encendido = (request->getParam("state")->value() == "1"); 
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request){
    String payload = String(alpha_objetivo) + "," + String(sistema_encendido ? 1 : 0) + "," + String(alpha_actual) + "," + String(rpm_actual) + "," + String(voltaje_taco_actual, 2) + "," + String(zcd_sintonizado ? 1 : 0) + "," + String(tiempo_semiciclo) + "," + String(invertir_canales ? 1 : 0);
    request->send(200, "text/plain", payload);
  });

  server.on("/get_config", HTTP_GET, [](AsyncWebServerRequest *request){
    String payload = String(kp_lineal, 4) + "," + String(ki_lineal, 4) + "," + String(rpm_objetivo_web) + "," + String(modo_operacion) + "," + String(sistema_encendido ? 1 : 0) + "," + 
                     String(limite_inferior_alfa) + "," + String(limite_superior_alfa) + "," + String(velocidad_rampa_on) + "," + String(velocidad_rampa_off) + "," + String(ancho_pulso_us) + "," + String(desfase_zcd_us) + "," + String(invertir_canales ? 1 : 0) + "," + String(velocidad_rampa_p7);
    for(int i=0; i<10; i++) {
      payload += "," + String(calib_voltaje[i], 2) + "," + String((int)calib_rpm[i]);
    }
    request->send(200, "text/plain", payload);
  });

  server.begin();

  for(;;) {
    ArduinoOTA.handle(); 
    vTaskDelay(20 / portTICK_PERIOD_MS); 
  }
}

// ==============================================================================
// CONFIGURACIÓN DE HARDWARE (NÚCLEO 1)
// ==============================================================================
void setup() {
  Serial.begin(115200);

  pinMode(ZCD_PIN, INPUT); 
  pinMode(SCR_PAR_P_PIN, OUTPUT);
  pinMode(SCR_PAR_N_PIN, OUTPUT);
  digitalWrite(SCR_PAR_P_PIN, LOW); 
  digitalWrite(SCR_PAR_N_PIN, LOW); 

  analogReadResolution(12);

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  attachInterrupt(digitalPinToInterrupt(ZCD_PIN), zeroCrossISR, RISING);

  inicializar_controlador(); 

  Serial.println("[ZCD] Sintonizando PLL...");
  unsigned long timeout_calibracion = millis();
  
  while (!zcd_sintonizado) { 
    if (millis() - timeout_calibracion > 3000) {
      Serial.println("[ALERTA] Arrancando con parámetros base por timeout.");
      zcd_sintonizado = false; 
      break;
    }
    delay(10); 
  }

  xTaskCreatePinnedToCore(tareaNucleoCero, "Tarea_Red", 10000, NULL, 1, &TareaWeb, 0);
}

// ==============================================================================
// LAZO DE CONTROL ESTRICTO (NÚCLEO 1)
// ==============================================================================
void loop() {
  unsigned long tiempoActual = millis();

  // WATCHDOG ASÍNCRONO DE HARDWARE
  unsigned long micros_actuales = micros();
  if (micros_actuales - tiempo_ultimo_cruce > 200000) { 
    if (zcd_sintonizado || zcd_muestras > 0) {
      Serial.println("[WATCHDOG] Señal AC perdida. Reseteando sintonía...");
      sistema_encendido = false; 
      
      zcd_sintonizado = false;
      zcd_muestras = 0;
      zcd_suma_tiempos = 0;
      
      GPIO.out_w1tc = ((uint32_t)1 << SCR_PAR_P_PIN) | ((uint32_t)1 << SCR_PAR_N_PIN);
      timerAlarmDisable(timer);
      estado_timer = 0;
      alpha_actual = limite_superior_alfa + 1;
    }
  }

  // LECTURA EXCLUSIVA DE PERIFÉRICOS ÚTILES (El potenciómetro fue erradicado de aquí)
  int raw_taco = analogRead(TACO_PIN);
  voltaje_taco_actual = (raw_taco * 3.3) / 4095.0; 
  rpm_actual = extrapolarRPM(voltaje_taco_actual);

  // MODO 2: PID CON ANTI-WINDUP 
  if (modo_operacion == 2) {
    alpha_objetivo = calcular_alfa_control((float)rpm_objetivo_web, (float)rpm_actual, alpha_actual, limite_inferior_alfa, limite_superior_alfa, sistema_encendido);
    
    static unsigned long tiempoAnteriorRampaP7 = 0;
    if (sistema_encendido && abs(alpha_objetivo - alpha_actual) > 2) {
      if (tiempoActual - tiempoAnteriorRampaP7 >= (unsigned long)velocidad_rampa_p7) {
        tiempoAnteriorRampaP7 = tiempoActual;
        if (alpha_actual > alpha_objetivo) alpha_actual--;
        else if (alpha_actual < alpha_objetivo) alpha_actual++;
      }
    }
  }

  // GESTIÓN DE RAMPAS MANUALES
  if (sistema_encendido) {
    if (alpha_actual > limite_superior_alfa) {
      alpha_actual = limite_superior_alfa;
    }
    
    if (modo_operacion != 2) { 
      if (tiempoActual - tiempoAnteriorRampa >= (unsigned long)velocidad_rampa_on) {
        tiempoAnteriorRampa = tiempoActual;
        if (alpha_actual > alpha_objetivo) alpha_actual--;
        else if (alpha_actual < alpha_objetivo) alpha_actual++;
      }
    }
  } else {
    if (alpha_actual < limite_superior_alfa) {
      if (tiempoActual - tiempoAnteriorRampa >= (unsigned long)velocidad_rampa_off) {
        tiempoAnteriorRampa = tiempoActual;
        alpha_actual++; 
      }
    } else {
      alpha_actual = limite_superior_alfa + 1;
    }
  }

  delay(2); 
}