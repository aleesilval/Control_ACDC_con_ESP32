#pragma once
#include <Arduino.h>

float kp_lineal = 0.0015;   
float ki_lineal = 0.0080;   

const unsigned long TS_CONTROL_MS = 20; 

float error_acumulado = 0.0;
unsigned long ultimo_tiempo_control = 0;

const float COS_ALFA_MAX_VOLTAJE = 0.9848;  
const float COS_ALFA_MIN_VOLTAJE = -0.8660; 

void inicializar_controlador() {
    error_acumulado = 0.0;
    ultimo_tiempo_control = millis();
}

int calcular_alfa_control(float rpm_objetivo, float rpm_actual, int alfa_base, int lim_inf, int lim_sup, bool sistema_activo) {
    // PROTECCIÓN ANTI-WINDUP: Evita el latigazo de corriente al iniciar
    if (!sistema_activo) {
        error_acumulado = 0.0;
        return lim_sup; // Mantiene el ángulo en la zona de apagado seguro
    }

    unsigned long tiempo_actual = millis();
    unsigned long dt_ms = tiempo_actual - ultimo_tiempo_control;

    if (dt_ms < TS_CONTROL_MS) {
        return alfa_base; 
    }
    
    ultimo_tiempo_control = tiempo_actual;
    float dt = (float)dt_ms / 1000.0; 

    float error = rpm_objetivo - rpm_actual;
    float accion_p = kp_lineal * error;
    float posible_integral = error_acumulado + (error * dt);
    float accion_i = ki_lineal * posible_integral;

    float esfuerzo_coseno = accion_p + accion_i;

    if (esfuerzo_coseno > COS_ALFA_MAX_VOLTAJE) {
        esfuerzo_coseno = COS_ALFA_MAX_VOLTAJE;
    } else if (esfuerzo_coseno < COS_ALFA_MIN_VOLTAJE) {
        esfuerzo_coseno = COS_ALFA_MIN_VOLTAJE;
    } else {
        error_acumulado = posible_integral; 
    }

    float alfa_radianes = acos(esfuerzo_coseno);
    int nuevo_alfa = round(alfa_radianes * 180.0 / PI);

    return constrain(nuevo_alfa, lim_inf, lim_sup);
}