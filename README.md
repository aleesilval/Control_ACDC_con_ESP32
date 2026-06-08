# ⚡ Control de Potencia AC/DC Centralizado con ESP32

Este repositorio contiene el firmware para un sistema de rectificación de onda completa totalmente controlado, diseñado para aplicaciones de electrónica industrial. El sistema utiliza un microcontrolador ESP32 para gestionar el ángulo de disparo ($\alpha$) de un puente de tiristores en tiempo real, permitiendo la regulación de voltaje RMS hacia cargas inductivas o resistivas.

## 🛠️ Características Principales

* **Detección de Cruce por Cero (ZCD):** Interrupción por hardware configurada con un *Blanking Time* (filtro anti-rebote de 6ms) para procesar ondas rectificadas provenientes de un LM358 sin falsos disparos.
* **Filtro Exponencial Pasa Bajos (EMA):** Suavizado matemático de las lecturas del ADC (potenciómetro) para garantizar transiciones de fase fluidas sin desgaste mecánico.
* **Soft-Start Integrado:** Algoritmo de rampa de aceleración paramétrica para proteger las cargas inductivas de picos de corriente durante el encendido.
* **Interfaz Web Asíncrona:** Panel de control accesible vía Wi-Fi (SoftAP) para el monitoreo de ángulos y conmutación de fase lógica de manera remota.
* **Over-The-Air (OTA):** Capacidad de actualización de firmware de manera inalámbrica, permitiendo reprogramaciones seguras con el circuito energizado.
* **Memoria No Volátil:** Almacenamiento del último setpoint operativo en la memoria flash interna mediante `Preferences.h`.

## 🧰 Hardware Implementado

* **Microcontrolador:** ESP32 DOIT DevKit V1
* **Etapa de Detección:** Amplificador Operacional LM358 + Optoacoplador 4N35
* **Etapa de Control de Disparo:** Optoaisladores MOC3020 + Transistores 2N3904 (Low-Side Switch)
* **Etapa de Potencia (110V AC):** Tiristores SCR BT151 en topología de puente

## ⚙️ Estructura del Software y Pines

El firmware emplea una arquitectura de doble núcleo (FreeRTOS) provista por el framework de Arduino bajo PlatformIO:
* **Núcleo 0:** Administra la pila Wi-Fi, el servidor web asíncrono y las peticiones OTA.
* **Núcleo 1:** Dedicado exclusivamente al control de fase crítico, filtros ADC y rutinas de interrupción (ISR).

| Función | Pin ESP32 |
| :--- | :--- |
| **ZCD Input** | `GPIO 13` |
| **Potenciómetro** | `GPIO 34` |
| **Disparo SCR (Q1 y Q4)** | `GPIO 5` |
| **Disparo SCR (Q2 y Q3)** | `GPIO 18` |

---
**Desarrollado por:** Alejandro Silva  
*Universidad Nacional Experimental Politécnica "Antonio José de Sucre" (UNEXPO)*