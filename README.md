# Robot Autónomo Patrulla IoT

Proyecto de **titulación** y código abierto sobre vigilancia autónoma con **ESP32-WROOM-32**, sensor PIR **HC-SR501** y plataforma **ThingsBoard**.  
El sistema detecta movimiento en tiempo real y envía notificaciones vía **Telegram** usando el protocolo **MQTT**.  

Este prototipo fue desarrollado como parte de mi proceso de aprendizaje. No es perfecto: presenta **errores ocasionales**, **tasas de detecciones falsas** y aún no está optimizado al máximo. El código está **comentado** y fue creado con apoyo de **IA**, lo que lo hace accesible para que cualquiera pueda **modificarlo, estudiarlo y mejorarlo**.

---

## 🚀 Objetivo
Diseñar y evaluar un robot patrulla accesible y reproducible que combine:
- Detección de movimiento aceptable (≈90% en pruebas interiores).  
- Notificación inmediata (2–4 s vía Telegram).  
- Autonomía de ~4.5 h con batería 3 Ah.  
- Integración con plataformas IoT abiertas como **ThingsBoard CE**.  

---

## 🔧 Componentes Clave
- **ESP32-WROOM-32** — Wi-Fi, Bluetooth, doble núcleo 240 MHz.  
- **HC-SR501** — detección de movimiento 3–7 m, <100°.  
- **L298N** — control de motores DC.  
- **Motores TT 3–6 V** — propulsión básica.  
- **ThingsBoard CE** — dashboard, alarmas y rule chains.  
- **Telegram Bot** — notificación al operador en tiempo real.  

---

## 📊 Métricas del Prototipo
| Métrica                  | Resultado | Observación |
|---------------------------|-----------|-------------|
| Tasa de detección         | ~93%      | Buen desempeño en interiores |
| Falsas alarmas            | <7%       | Necesita optimización |
| Latencia MQTT             | 1.5–2.5 s | Estable |
| Notificación Telegram     | 2–4 s     | Funcional |
| Autonomía (3 Ah)          | ~4.5 h    | Limitada |
| Rango Wi-Fi               | 35–45 m   | Adecuado |

---

## 🛠️ Instalación Rápida
1. Configurar dispositivo en **ThingsBoard CE** y obtener *Access Token*.  
2. Programar firmware ESP32 con librerías `PubSubClient` y `ArduinoJson`.  
3. Crear **Rule Chain** en ThingsBoard para enviar alertas a Telegram.  
4. Probar detección y notificación en entorno controlado.  

---

## 📚 Licencia
Distribuido bajo **Apache 2.0** gracias a la plataforma ThingsBoard CE.  
Este proyecto busca demostrar que la seguridad autónoma puede ser **accesible, reproducible y abierta** para instituciones educativas y comunidades.  

---

## 🔮 Futuro
- Integración de cámara (ESP32-CAM).  
- Navegación dinámica con SLAM.  
- Reducción de falsas alarmas mediante IA.  

---

### Contribuye
Este repositorio está abierto a mejoras en hardware, firmware y documentación.  
Tu aporte puede ayudar a escalar la seguridad autónoma en entornos con recursos limitados.  
