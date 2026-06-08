# ⚡ Control de Potencia AC/DC Centralizado con ESP32

Este repositorio contiene el firmware para un sistema de rectificación de onda completa totalmente controlado, diseñado para aplicaciones de electrónica industrial. El sistema utiliza un microcontrolador ESP32 para gestionar el ángulo de disparo ($\alpha$) de un puente de tiristores en tiempo real, permitiendo la regulación de voltaje RMS hacia cargas inductivas o resistivas.

## 🛠️ Características Principales

* **Detección de Cruce por Cero (ZCD):** Interrupción por hardware configurada con un *Blanking Time* (filtro anti-rebote de 6ms) para procesar ondas rectificadas provenientes del comparador sin falsos disparos.
* **Filtro Exponencial Pasa Bajos (EMA):** Suavizado matemático de las lecturas del ADC (potenciómetro) para garantizar transiciones de fase fluidas sin desgaste mecánico.
* **Soft-Start Integrado:** Algoritmo de rampa de aceleración paramétrica para proteger las cargas inductivas de picos de corriente durante el encendido.
* **Interfaz Web Asíncrona:** Panel de control accesible vía Wi-Fi (SoftAP) para el monitoreo de ángulos y conmutación de fase lógica de manera remota.
* **Over-The-Air (OTA):** Capacidad de actualización de firmware de manera inalámbrica, permitiendo reprogramaciones seguras con el circuito energizado.
* **Memoria No Volátil:** Almacenamiento del último setpoint operativo en la memoria flash interna mediante `Preferences.h`.

## 🔍 Arquitectura de Hardware y Flujo de Señales

El sistema está dividido en etapas galvánicamente aisladas para garantizar la seguridad del microcontrolador y la estabilidad del control de potencia:

### 1. Etapa de Detección de Cruce por Cero (ZCD)
Para sincronizar el microcontrolador con la red eléctrica, se extrae una muestra de la línea de 110V AC, la cual pasa primero por un **puente rectificador de diodos** estándar. Esta onda pulsante de onda completa ingresa a un amplificador operacional **LM358** configurado como comparador. El voltaje de referencia de este operacional se ajusta mediante un potenciómetro, lo que permite calibrar con precisión el ancho y el umbral del pulso de cruce por cero. 

Este pulso resultante alimenta el LED infrarrojo de un optoacoplador **4N35**. En el lado de baja tensión, el fototransistor del 4N35 está polarizado con los **3.3V** del ESP32, lo que permite entregar un pulso digital limpio, seguro y aislado al GPIO 13 cada vez que la onda de red toca el nivel de cero voltios.

### 2. Etapa de Potencia y Puente Totalmente Controlado
Una vez que el ESP32 detecta el pulso ZCD, el algoritmo calcula el tiempo de retardo (ángulo $\alpha$) dictado por el potenciómetro principal y envía la orden de disparo a las salidas lógicas. 

Estos pulsos de salida alimentan a los optoacopladores **MOC3020**, cuyas resistencias limitadoras fueron calculadas meticulosamente para saturar el fotodiodo interno de manera óptima. Al recibir la señal óptica, los MOC3020 inyectan la corriente de compuerta a sus respectivos **Tiristores (SCR)**. Al estar dispuestos en una topología de puente completo, los pares de SCRs se disparan en los semiciclos correspondientes, rectificando y recortando la onda simultáneamente para entregar a la carga un voltaje DC totalmente controlado.

## 🧠 Detalles de la Arquitectura de Software

El código fuente está diseñado para maximizar la estabilidad aislando los procesos pesados de las tareas críticas de tiempo real, explotando la capacidad dual-core (FreeRTOS) del ESP32:

* **Procesamiento Asíncrono (Núcleo 0):** Aloja la pila de red en modo Punto de Acceso (SoftAP), el servidor web (`ESPAsyncWebServer`) y las rutinas de actualización remota (OTA). Al ser totalmente asíncrono, las peticiones HTTP de la interfaz (como encender el sistema o cambiar la inversión de canales) se procesan mediante *endpoints* superligeros (`/set`, `/get`, `/power`) sin interrumpir ni bloquear la matemática del sistema.
* **Control de Tiempo Real (Núcleo 1):** Dedicado exclusivamente a las interrupciones, el conteo de hardware y el filtrado de señales:
    * **Ejecución en IRAM:** Las funciones de interrupción (`zeroCrossISR` y `onTimer`) están decoradas con el atributo `IRAM_ATTR`. Esto fuerza su carga en la memoria RAM del ESP32, garantizando una ejecución ultrarrápida (en microsegundos) al evitar la latencia de lectura de la memoria Flash.
    * **Máquina de Estados con Hardware Timer:** Para la generación de los pulsos de disparo, se emplea un temporizador por hardware acoplado a una máquina de estados finita (Idle -> Disparo -> Apagado). Esto asegura que el ancho del pulso que satura al MOC3020 sea matemáticamente perfecto y estable en todos los ciclos.
    * **Filtro Digital EMA y Zona Muerta:** La lectura analógica pura es ruidosa. El software implementa un Filtro Exponencial Pasa Bajos emparejado con una zona muerta de tolerancia. Esto absorbe el ruido eléctrico del cableado y proporciona un valor de ángulo de disparo ($\alpha$) absolutamente estático, previniendo fluctuaciones erráticas en el motor o la carga.
    * **Enrutamiento Lógico Cruzado:** Se incorporó una capa de software que permite intercambiar el destino físico de los pulsos (`invertir_canales`). Esto compensa en caliente cualquier inversión de fase de la red eléctrica o asimetrías de la carga sin necesidad de modificar el hardware.

## 🧰 Resumen de Componentes y Pines

| Etapa | Componentes Principales | Pin ESP32 Asignado |
| :--- | :--- | :--- |
| **Detección ZCD** | Puente de Diodos, LM358, 4N35 | `GPIO 13` (Input, RISING) |
| **Control Analógico** | Potenciómetro Lineal | `GPIO 34` (ADC) |
| **Disparo (Semiciclo A)** | MOC3020, BT151 (Q1 y Q4) | `GPIO 5` (Output) |
| **Disparo (Semiciclo B)** | MOC3020, BT151 (Q2 y Q3) | `GPIO 18` (Output) |

---
**Desarrollado por:** Alejandro Silva  
*Universidad Nacional Experimental Politécnica "Antonio José de Sucre" (UNEXPO)*