// ============================================================
// Servidor CoAP en ESP32 - Práctica de laboratorio
// Recursos ofrecidos: /sensor (lectura) y /led (acción)
// Transporte: UDP, puerto 5683 (puerto oficial de CoAP)
// Librería: CoAPSimple, de hirotakaster
// Placa: ESP32 (NodeMCU / DevKit) en Arduino IDE 2.x
// ============================================================

// --- Librerías ----------------------------------------------
// WiFi.h: maneja la conexión Wi-Fi de la ESP32.
// WiFiUdp.h: permite enviar y recibir datagramas UDP.
// coap-simple.h: CoAPSimple interpreta los mensajes CoAP
//   y despacha cada petición a su callback.
#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>

// --- Credenciales de la red ---------------------------------
// Sustituye estos dos valores por los de la red del laboratorio.
// OJO: la ESP32 solo trabaja en redes Wi-Fi de 2.4 GHz.
const char* ssid     = "TU_SSID_WIFI";
const char* password = "TU_PASSWORD_WIFI";

// --- Objetos globales ---------------------------------------
// WiFiUDP es el "buzón" UDP: por aquí entran y salen datagramas.
// Coap trabaja SOBRE ese buzón UDP. Separarlos es la pila de
// protocolos en código: CoAP es la capa de aplicación y UDP,
// la capa de transporte.
WiFiUDP udp;
Coap coap(udp);

// ============================================================
// CALLBACK DEL RECURSO /led
// Se ejecuta SOLO cuando llega una petición a coap://IP/led.
// Parámetros que entrega la librería:
//   packet: el mensaje CoAP recibido (payload, Message ID...)
//   ip:     dirección IP del cliente que preguntó
//   port:   puerto UDP de ese cliente (a dónde responder)
// ============================================================
void callback_led(CoapPacket &packet, IPAddress ip, int port) {

  // Aviso en el Monitor Serie: primera evidencia de que la
  // petición llegó y de que el despacho funcionó.
  Serial.println("Petición CoAP recibida en /led");

  // --- Leer el payload (el "cuerpo" del mensaje) -------------
  // packet.payload es un arreglo de bytes SIN carácter de fin,
  // así que copiamos payloadlen bytes y agregamos '\0' para
  // poder tratarlo como texto. En un POST real aquí vendría
  // la orden, por ejemplo "ON" u "OFF".
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = '\0';

  String payloadStr(p);           // Convertimos los bytes a String
  Serial.print("Payload recibido: ");
  Serial.println(payloadStr);     // Lo mostramos para depurar

  // --- Responder al cliente ----------------------------------
  // sendResponse necesita: a quién (ip, port), el Message ID
  // de la petición (para emparejar petición y respuesta) y el
  // texto que irá como payload de respuesta. La librería
  // completa el resto: tipo ACK y código 2.05 Content.
  coap.sendResponse(ip, port, packet.messageid,
                    "Estado: OK desde ESP32 CoAP Server");
}

// ============================================================
// CALLBACK DEL RECURSO /sensor
// Responde a GET coap://IP/sensor con una temperatura.
// En esta práctica el sensor es SIMULADO; en el Reto 2 del
// README se sustituye por una lectura real.
// ============================================================
void callback_sensor(CoapPacket &packet, IPAddress ip, int port) {

  // Valor base 25.4 °C + una variación aleatoria de 0.0 a 4.9 °C.
  // random(0, 50) devuelve enteros de 0 a 49; al dividir entre
  // 10.0 obtenemos décimas. Así, cada petición da un valor
  // ligeramente distinto, como un sensor real.
  float temperaturaSimulada = 25.4 + (random(0, 50) / 10.0);

  // Construimos el payload de respuesta como texto plano:
  // "Temp: 27.1 C". Concatenar con String es cómodo para
  // aprender; en proyectos grandes se prefiere snprintf()
  // porque String fragmenta la memoria con el tiempo.
  String respuesta = "Temp: " + String(temperaturaSimulada) + " C";

  // Enviamos la respuesta al cliente que preguntó.
  // respuesta.c_str() entrega el texto en formato C (char*),
  // que es lo que sendResponse espera.
  coap.sendResponse(ip, port, packet.messageid, respuesta.c_str());
}

// ============================================================
// setup(): se ejecuta UNA sola vez al encender o resetear
// ============================================================
void setup() {
  Serial.begin(115200);   // Monitor Serie a 115200 baudios
  delay(1000);            // Pausa breve: da tiempo a que el
                          // puerto serie esté listo al arrancar

  // --- Conexión Wi-Fi ---------------------------------------
  // WiFi.begin inicia la conexión; el ciclo while espera,
  // imprimiendo un punto cada 0.5 s, hasta que el router
  // asigne una IP. Si aquí se queda para siempre, el problema
  // es SSID, contraseña o red de 5 GHz (ver README, sec. 9).
  WiFi.begin(ssid, password);
  Serial.print("Conectando a Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // --- La IP que obtuvimos ES la dirección del servidor -----
  // Anótala: es la que usarás en coap://<IP>/sensor
  Serial.println("\nConectado a la red");
  Serial.print("Dirección IP del Servidor CoAP: ");
  Serial.println(WiFi.localIP());

  // --- Registrar los recursos (endpoints) -------------------
  // Esta es la "tabla de rutas" del servidor: cuando llegue
  // una petición a "led" se ejecutará callback_led, y a
  // "sensor", callback_sensor. Sin este registro, la ruta
  // no existe y el cliente recibe 4.04 Not Found.
  coap.server(callback_led, "led");
  coap.server(callback_sensor, "sensor");

  // --- Arrancar el servidor ---------------------------------
  // Abre el puerto UDP 5683 y deja el servidor listo.
  // A partir de aquí, el trabajo ocurre en loop().
  coap.start();
}

// ============================================================
// loop(): se repite para siempre. Su único trabajo es
// escuchar. Cada llamada a coap.loop() revisa si llegó un
// datagrama; si llegó, lo interpreta y llama al callback
// que corresponda. Debe ejecutarse lo más seguido posible:
// NADA de delays largos aquí.
// ============================================================
void loop() {
  coap.loop();
}
