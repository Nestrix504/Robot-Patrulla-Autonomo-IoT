/*
 * ================================================================
 *   CARRO PATRULLA AUTÓNOMO - ESP32
 * ================================================================
 *
 *  Enfoque de esta versión:
 *    - Estructura modular y fácil de mantener
 *    - Máquina de estados clara
 *    - Patrullaje no bloqueante
 *    - Recuperación automática si el robot no puede seguir avanzando
 *    - Telemetría a ThingsBoard por HTTP
 *
 *  NOTA IMPORTANTE:
 *    Con el hardware listado aquí (PIR + motores), el ESP32 NO puede
 *    saber con certeza si el carro está físicamente atascado.
 *    Para detectar bloqueo real se requiere al menos uno de estos:
 *      - sensor ultrasónico (HC-SR04 o similar)
 *      - sensores de línea / borde
 *      - encoders en los motores
 *      - bumper / microswitch frontal
 *
 *    Aun así, esta versión incluye una lógica de recuperación que:
 *      - patrulla por tramos
 *      - gira en forma automática al terminar un tramo
 *      - permite reemplazar fácilmente la función detectarBloqueo()
 *        por un sensor real sin reescribir el resto del programa
 *
 *  Hardware:
 *    - ESP32
 *    - L298N
 *    - 2 motores DC
 *    - PIR HC-SR501
 *
 *  Librerías:
 *    - Arduino.h
 *    - WiFi.h
 *    - HTTPClient.h
 *
 *  Autor  : Sistema de Vigilancia Autónoma
 *  Versión: 2.0
 * ================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ================================================================
//  CONFIGURACIÓN GENERAL
// ================================================================
namespace Config {

  // ---------------- WiFi ----------------
  constexpr const char* WIFI_SSID     = "PIPIKIWI";
  constexpr const char* WIFI_PASSWORD = "olamamahuevo";

  // ---------------- ThingsBoard ----------------
  constexpr const char* TB_HOST         = "thingsboard.cloud";
  constexpr int         TB_PORT         = 80;   // 443 si usas HTTPS
  constexpr const char* TB_ACCESS_TOKEN  = "039y03nn5bu4pyxpeqwb";

  // ---------------- PWM ----------------
  constexpr uint32_t PWM_FRECUENCIA = 1000;
  constexpr uint8_t   PWM_RESOLUCION = 8;
  constexpr int       PWM_MAX        = 255;

  // ---------------- Velocidades ----------------
  constexpr int VEL_CRUCERO = 200;
  constexpr int VEL_GIRO     = 175;
  constexpr int VEL_RECUP    = 180;

  // ---------------- Tiempos ----------------
  constexpr unsigned long DURACION_ALERTA_MS = 6000;
  constexpr unsigned long DEBOUNCE_PIR_MS    = 2000;
  constexpr unsigned long WIFI_RETRY_MS      = 15000;

  // Tiempo máximo permitido en un tramo antes de asumir que conviene maniobrar
  constexpr unsigned long TRAMO_MAX_MS       = 5000;

  // Pausas cortas entre maniobras para estabilizar el movimiento
  constexpr unsigned long PAUSA_GIRO_MS      = 150;
}

// ================================================================
//  PINOUT
// ================================================================
namespace Pins {
  // Motor izquierdo (A)
  constexpr int ENA = 14;
  constexpr int IN1 = 27;
  constexpr int IN2 = 26;

  // Motor derecho (B)
  constexpr int ENB = 25;
  constexpr int IN3 = 33;
  constexpr int IN4 = 32;

  // PIR
  constexpr int PIR = 35;
}

// ================================================================
//  TIPOS Y ESTADOS
// ================================================================
enum class EstadoMovimiento : uint8_t {
  Avanzar,
  Retroceder,
  GirarDerecha,
  GirarIzquierda,
  Detenido
};

enum class EstadoPatrulla : uint8_t {
  Patrullando,
  Maniobra,
  Alerta
};

struct PasoRuta {
  EstadoMovimiento estado;
  unsigned long duracionMs;
};

// Ruta base. Es fácil de editar: agrega, quita o cambia pasos.
const PasoRuta RUTA_PATRULLA[] = {
  { EstadoMovimiento::Avanzar,       3000 },
  { EstadoMovimiento::Detenido,       250 },
  { EstadoMovimiento::GirarDerecha,   850 },
  { EstadoMovimiento::Detenido,       250 },
  { EstadoMovimiento::Avanzar,       2500 },
  { EstadoMovimiento::Detenido,       250 },
  { EstadoMovimiento::GirarDerecha,   850 },
  { EstadoMovimiento::Detenido,       250 },
  { EstadoMovimiento::Avanzar,       3000 },
  { EstadoMovimiento::Detenido,       250 },
  { EstadoMovimiento::GirarDerecha,   850 },
  { EstadoMovimiento::Detenido,       250 },
  { EstadoMovimiento::Avanzar,       2500 },
  { EstadoMovimiento::Detenido,       250 },
  { EstadoMovimiento::GirarDerecha,   850 },
  { EstadoMovimiento::Detenido,       400 },
};

constexpr size_t TOTAL_PASOS = sizeof(RUTA_PATRULLA) / sizeof(RUTA_PATRULLA[0]);

// ================================================================
//  VARIABLES GLOBALES DE CONTROL
// ================================================================
namespace Estado {
  EstadoPatrulla modo = EstadoPatrulla::Patrullando;

  size_t pasoActual = 0;
  unsigned long inicioPasoMs = 0;

  unsigned long inicioAlertaMs = 0;
  unsigned long ultimoLogAlertaMs = 0;

  bool pirAnterior = false;
  unsigned long ultimoDisparoPirMs = 0;

  bool wifiConectado = false;
  unsigned long ultimoIntentoWifiMs = 0;

  // Maniobra de recuperación si el robot no avanza como se esperaba.
  // 0 = sigue; 1 = retrocede corto; 2 = gira; 3 = reanuda.
  uint8_t faseRecuperacion = 0;
  unsigned long inicioFaseMs = 0;
}

// ================================================================
//  PROTOTIPOS
// ================================================================
void inicializarMotores();
void inicializarPIR();
void conectarWiFi();
bool verificarConexionWiFi();
bool intentarReconectarWiFi();

void aplicarEstado(EstadoMovimiento estado, int velocidad = Config::VEL_CRUCERO);
void motorAvanzar(int velocidad = Config::VEL_CRUCERO);
void motorRetroceder(int velocidad = Config::VEL_CRUCERO);
void motorGirarDerecha(int velocidad = Config::VEL_GIRO);
void motorGirarIzquierda(int velocidad = Config::VEL_GIRO);
void motorDetener();

bool leerPIR();
bool detectarBloqueo();

void actualizarPatrullaje();
void ejecutarRecuperacion();
void activarAlerta();
void gestionarAlerta();
void enviarTelemetriaThingsBoard(bool movimiento, const char* descripcion);

void aplicarPasoActual();
void avanzarAlSiguientePaso();
void resetRutaDesde(size_t nuevoPaso);

// ================================================================
//  SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("================================================");
  Serial.println("     CARRO PATRULLA AUTÓNOMO - ESP32 v2.0");
  Serial.println("================================================");

  inicializarMotores();
  inicializarPIR();
  conectarWiFi();

  Estado::modo = EstadoPatrulla::Patrullando;
  Estado::pasoActual = 0;
  Estado::inicioPasoMs = millis();

  Serial.println("[SYS] Sistema listo. Iniciando patrullaje.");
  Serial.println("================================================");
}

// ================================================================
//  LOOP PRINCIPAL
// ================================================================
void loop() {
  // 1) PIR siempre primero
  if (leerPIR()) {
    activarAlerta();
  }

  // 2) Gestión de alerta
  if (Estado::modo == EstadoPatrulla::Alerta) {
    gestionarAlerta();
    return;
  }

  // 3) Mantener WiFi vivo sin bloquear el resto del programa
  if (!verificarConexionWiFi()) {
    unsigned long ahora = millis();
    if (ahora - Estado::ultimoIntentoWifiMs >= Config::WIFI_RETRY_MS) {
      Estado::ultimoIntentoWifiMs = ahora;
      intentarReconectarWiFi();
    }
  }

  // 4) Patrullaje normal
  actualizarPatrullaje();
}

// ================================================================
//  MOTORES
// ================================================================
void inicializarMotores() {
  pinMode(Pins::IN1, OUTPUT);
  pinMode(Pins::IN2, OUTPUT);
  pinMode(Pins::IN3, OUTPUT);
  pinMode(Pins::IN4, OUTPUT);

  ledcAttach(Pins::ENA, Config::PWM_FRECUENCIA, Config::PWM_RESOLUCION);
  ledcAttach(Pins::ENB, Config::PWM_FRECUENCIA, Config::PWM_RESOLUCION);

  motorDetener();
  Serial.println("[MOTOR] L298N inicializado.");
}

void motorAvanzar(int velocidad) {
  digitalWrite(Pins::IN1, HIGH);
  digitalWrite(Pins::IN2, LOW);
  digitalWrite(Pins::IN3, HIGH);
  digitalWrite(Pins::IN4, LOW);
  ledcWrite(Pins::ENA, velocidad);
  ledcWrite(Pins::ENB, velocidad);
}

void motorRetroceder(int velocidad) {
  digitalWrite(Pins::IN1, LOW);
  digitalWrite(Pins::IN2, HIGH);
  digitalWrite(Pins::IN3, LOW);
  digitalWrite(Pins::IN4, HIGH);
  ledcWrite(Pins::ENA, velocidad);
  ledcWrite(Pins::ENB, velocidad);
}

void motorGirarDerecha(int velocidad) {
  digitalWrite(Pins::IN1, HIGH);
  digitalWrite(Pins::IN2, LOW);
  digitalWrite(Pins::IN3, LOW);
  digitalWrite(Pins::IN4, HIGH);
  ledcWrite(Pins::ENA, velocidad);
  ledcWrite(Pins::ENB, velocidad);
}

void motorGirarIzquierda(int velocidad) {
  digitalWrite(Pins::IN1, LOW);
  digitalWrite(Pins::IN2, HIGH);
  digitalWrite(Pins::IN3, HIGH);
  digitalWrite(Pins::IN4, LOW);
  ledcWrite(Pins::ENA, velocidad);
  ledcWrite(Pins::ENB, velocidad);
}

void motorDetener() {
  digitalWrite(Pins::IN1, LOW);
  digitalWrite(Pins::IN2, LOW);
  digitalWrite(Pins::IN3, LOW);
  digitalWrite(Pins::IN4, LOW);
  ledcWrite(Pins::ENA, 0);
  ledcWrite(Pins::ENB, 0);
}

void aplicarEstado(EstadoMovimiento estado, int velocidad) {
  switch (estado) {
    case EstadoMovimiento::Avanzar:        motorAvanzar(velocidad);        break;
    case EstadoMovimiento::Retroceder:     motorRetroceder(velocidad);     break;
    case EstadoMovimiento::GirarDerecha:   motorGirarDerecha(velocidad);   break;
    case EstadoMovimiento::GirarIzquierda: motorGirarIzquierda(velocidad); break;
    case EstadoMovimiento::Detenido:
    default:                               motorDetener();                 break;
  }
}

// ================================================================
//  PIR
// ================================================================
void inicializarPIR() {
  pinMode(Pins::PIR, INPUT);
  Serial.println("[PIR] HC-SR501 configurado.");
  Serial.println("[PIR] Recomendado: esperar 20-30 s de estabilización del sensor.");
}

bool leerPIR() {
  bool estadoActual = (digitalRead(Pins::PIR) == HIGH);
  unsigned long ahora = millis();

  if (estadoActual && !Estado::pirAnterior &&
      (ahora - Estado::ultimoDisparoPirMs) >= Config::DEBOUNCE_PIR_MS) {

    Estado::pirAnterior = true;
    Estado::ultimoDisparoPirMs = ahora;

    Serial.printf("[PIR] Movimiento detectado (%lu ms)\n", ahora);
    return true;
  }

  if (!estadoActual && Estado::pirAnterior) {
    Estado::pirAnterior = false;
  }

  return false;
}

// ================================================================
//  DETECCIÓN DE BLOQUEO / RECUPERACIÓN
// ================================================================
/*
 * Esta función está preparada para integrar un sensor real.
 *
 * Reemplázala por una lectura de:
 *   - ultrasonido
 *   - bumper
 *   - encoders
 *   - línea/borde
 *
 * Con el hardware actual, devuelve false por defecto.
 */
bool detectarBloqueo() {
  return false;
}

void ejecutarRecuperacion() {
  switch (Estado::faseRecuperacion) {
    case 0:
      // Fase 0: retroceder un poco
      motorRetroceder(Config::VEL_RECUP);
      Estado::inicioFaseMs = millis();
      Estado::faseRecuperacion = 1;
      Serial.println("[RECUP] Retroceso de seguridad.");
      break;

    case 1:
      if (millis() - Estado::inicioFaseMs >= 450) {
        motorDetener();
        Estado::inicioFaseMs = millis();
        Estado::faseRecuperacion = 2;
        Serial.println("[RECUP] Pausa corta.");
      }
      break;

    case 2:
      // Giro alternado para explorar otra dirección
      // Cambia el sentido del giro según el paso actual para evitar quedarse repitiendo la misma maniobra
      if ((Estado::pasoActual % 2) == 0) {
        motorGirarDerecha(Config::VEL_GIRO);
        Serial.println("[RECUP] Giro a la derecha.");
      } else {
        motorGirarIzquierda(Config::VEL_GIRO);
        Serial.println("[RECUP] Giro a la izquierda.");
      }
      Estado::inicioFaseMs = millis();
      Estado::faseRecuperacion = 3;
      break;

    case 3:
      if (millis() - Estado::inicioFaseMs >= 700) {
        motorDetener();
        Estado::faseRecuperacion = 0;

        // Reanudar patrullaje desde el siguiente paso para no quedar atrapado en el mismo tramo
        avanzarAlSiguientePaso();
        Estado::inicioPasoMs = millis();
        Serial.println("[RECUP] Recuperación completada. Reanudando ruta.");
      }
      break;
  }
}

// ================================================================
//  PATRULLAJE
// ================================================================
void aplicarPasoActual() {
  aplicarEstado(RUTA_PATRULLA[Estado::pasoActual].estado, Config::VEL_CRUCERO);
}

void avanzarAlSiguientePaso() {
  Estado::pasoActual = (Estado::pasoActual + 1) % TOTAL_PASOS;
  Estado::inicioPasoMs = millis();

  Serial.printf("[RUTA] Paso %u/%u | estado=%u | duracion=%lums\n",
                static_cast<unsigned>(Estado::pasoActual + 1),
                static_cast<unsigned>(TOTAL_PASOS),
                static_cast<unsigned>(RUTA_PATRULLA[Estado::pasoActual].estado),
                RUTA_PATRULLA[Estado::pasoActual].duracionMs);
}

void resetRutaDesde(size_t nuevoPaso) {
  if (nuevoPaso >= TOTAL_PASOS) nuevoPaso = 0;
  Estado::pasoActual = nuevoPaso;
  Estado::inicioPasoMs = millis();
}

void actualizarPatrullaje() {
  unsigned long ahora = millis();
  unsigned long transcurrido = ahora - Estado::inicioPasoMs;

  // Si un sensor real detecta bloqueo, ejecutar recuperación inmediata
  if (detectarBloqueo()) {
    Estado::modo = EstadoPatrulla::Maniobra;
    Estado::faseRecuperacion = 0;
    Serial.println("[RECUP] Posible bloqueo detectado.");
    ejecutarRecuperacion();
    return;
  }

  const PasoRuta& paso = RUTA_PATRULLA[Estado::pasoActual];

  if (transcurrido >= paso.duracionMs) {
    avanzarAlSiguientePaso();
    return;
  }

  // Si el paso es un tramo de avance y supera cierto límite, se puede forzar una maniobra
  // Esto ayuda a evitar que el robot se quede repitiendo trayectos largos sin reajustar.
  if (paso.estado == EstadoMovimiento::Avanzar && transcurrido >= Config::TRAMO_MAX_MS) {
    // Estrategia simple: girar y continuar. Cambia el sentido para alternar rutas.
    Estado::modo = EstadoPatrulla::Maniobra;
    Estado::faseRecuperacion = 0;
    ejecutarRecuperacion();
    return;
  }

  // Movimiento normal del paso actual
  aplicarPasoActual();
}

// ================================================================
//  ALERTA PIR
// ================================================================
void activarAlerta() {
  if (Estado::modo == EstadoPatrulla::Alerta) return;

  Estado::modo = EstadoPatrulla::Alerta;
  Estado::inicioAlertaMs = millis();
  Estado::ultimoLogAlertaMs = 0;

  motorDetener();

  Serial.println();
  Serial.println("╔══════════════════════════════════╗");
  Serial.println("║   ALERTA: MOVIMIENTO DETECTADO   ║");
  Serial.println("╚══════════════════════════════════╝");
  Serial.printf("[ALERTA] t=%lu ms\n", Estado::inicioAlertaMs);

  enviarTelemetriaThingsBoard(true, "INTRUSION_DETECTADA");
}

void gestionarAlerta() {
  motorDetener();

  unsigned long tiempoEnAlerta = millis() - Estado::inicioAlertaMs;

  if (tiempoEnAlerta >= Config::DURACION_ALERTA_MS) {
    Estado::modo = EstadoPatrulla::Patrullando;
    Estado::inicioPasoMs = millis();

    // Al reanudar, no reiniciamos desde cero; seguimos el paso actual.
    Serial.println("[ALERTA] Finalizada. Retomando patrullaje.");
    return;
  }

  if (millis() - Estado::ultimoLogAlertaMs >= 1000) {
    Estado::ultimoLogAlertaMs = millis();
    Serial.printf("[ALERTA] Robot detenido... (%lu/%lu ms)\n",
                  tiempoEnAlerta, Config::DURACION_ALERTA_MS);
  }
}

// ================================================================
//  WiFi
// ================================================================
void conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);

  Serial.printf("[WiFi] Conectando a '%s'", Config::WIFI_SSID);

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - inicio) < 10000) {
    delay(400);
    Serial.print(".");
  }

  Estado::wifiConectado = (WiFi.status() == WL_CONNECTED);

  if (Estado::wifiConectado) {
    Serial.println();
    Serial.println("[WiFi] Conectado.");
    Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println();
    Serial.println("[WiFi] Sin conexión. Se mantiene modo local.");
  }
}

bool verificarConexionWiFi() {
  Estado::wifiConectado = (WiFi.status() == WL_CONNECTED);
  return Estado::wifiConectado;
}

bool intentarReconectarWiFi() {
  Serial.println("[WiFi] Intentando reconexión...");
  WiFi.disconnect();
  WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - inicio) < 6000) {
    delay(300);
    Serial.print("+");
  }

  Estado::wifiConectado = (WiFi.status() == WL_CONNECTED);

  if (Estado::wifiConectado) {
    Serial.println();
    Serial.println("[WiFi] Reconexión exitosa.");
    Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }

  Serial.println();
  Serial.println("[WiFi] No fue posible reconectar.");
  return false;
}

// ================================================================
//  THINGSBOARD
// ================================================================
void enviarTelemetriaThingsBoard(bool movimiento, const char* descripcion) {
  if (!verificarConexionWiFi()) {
    Serial.println("[TB] Sin WiFi. Telemetría solo local.");
    Serial.printf("[TB] motion=%s tipo=%s uptime=%lu paso=%u\n",
                  movimiento ? "true" : "false",
                  descripcion,
                  millis(),
                  static_cast<unsigned>(Estado::pasoActual));
    return;
  }

  char url[256];
  snprintf(url, sizeof(url),
           "http://%s:%d/api/v1/%s/telemetry",
           Config::TB_HOST,
           Config::TB_PORT,
           Config::TB_ACCESS_TOKEN);

  // Construcción segura del JSON.
  char payload[320];
  snprintf(payload, sizeof(payload),
           "{"
           "\"motion_detected\":%s,"
           "\"alert_type\":\"%s\","
           "\"uptime_ms\":%lu,"
           "\"paso_ruta\":%u,"
           "\"wifi_rssi\":%d"
           "}",
           movimiento ? "true" : "false",
           descripcion,
           millis(),
           static_cast<unsigned>(Estado::pasoActual),
           WiFi.RSSI());

  Serial.printf("[TB] POST %s\n", url);
  Serial.printf("[TB] JSON: %s\n", payload);

  HTTPClient http;
  WiFiClient client;

  if (!http.begin(client, url)) {
    Serial.println("[TB] No se pudo inicializar HTTPClient.");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  int codigoHTTP = http.POST(reinterpret_cast<uint8_t*>(payload), strlen(payload));

  if (codigoHTTP == 200 || codigoHTTP == 204) {
    Serial.printf("[TB] Telemetría enviada correctamente. HTTP %d\n", codigoHTTP);
  } else {
    Serial.printf("[TB] Error HTTP: %d | %s\n",
                  codigoHTTP,
                  http.errorToString(codigoHTTP).c_str());
  }

  http.end();
}
