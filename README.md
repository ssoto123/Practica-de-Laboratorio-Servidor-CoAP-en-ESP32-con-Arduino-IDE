# 📘 Práctica de Laboratorio: Implementación de un Servidor CoAP en ESP32 con Arduino IDE

**Institución:** Instituto Tecnológico Superior del Occidente del Estado de Hidalgo
**División:** Ingeniería en Tecnologías de la Información y Comunicaciones
**Elaborado por:** Prof. Saúl Isaí Soto Ortiz

---

## 📖 Introducción

CoAP (Constrained Application Protocol, RFC 7252) es un protocolo de transferencia web diseñado para dispositivos con restricciones: poca memoria, procesadores modestos, baterías pequeñas y redes lentas o con pérdidas.

Su idea central es conservar el modelo de la web —recursos con URI y métodos `GET`, `POST`, `PUT` y `DELETE`— eliminando casi todo el costo de transportarlo. A diferencia de HTTP, que opera sobre TCP, **CoAP opera sobre UDP**. Eso elimina el establecimiento de conexión (el *handshake* de tres vías de TCP), el control de flujo y la retransmisión ordenada de un flujo continuo. Cada mensaje CoAP viaja en un solo datagrama, y la fiabilidad, cuando se necesita, la aporta el propio CoAP con un mecanismo simple de confirmación y reintento.

En esta práctica, la ESP32 se convierte en un **servidor CoAP** con dos recursos:

| Recurso | Método | Qué hace |
| :--- | :--- | :--- |
| `/sensor` | `GET` | Devuelve una temperatura simulada, por ejemplo `Temp: 27.1 C` |
| `/led` | `POST` | Recibe un payload (`ON` / `OFF`) y responde `Estado: OK desde ESP32 CoAP Server` |

**Duración estimada:** 2 sesiones de laboratorio (90–120 min en total).
**Entregable:** reporte con evidencias de las pruebas (sección 8) y respuestas de análisis (sección 10).

### 🎯 Objetivo

Comprender el funcionamiento del protocolo CoAP enfocado en arquitecturas IoT, configurar la placa ESP32 como un servidor CoAP utilizando el entorno de Arduino IDE, y realizar pruebas de comunicación mediante peticiones desde un cliente externo.

Al terminar, el alumno será capaz de:

- Explicar por qué CoAP usa UDP y en qué se diferencia de HTTP sobre TCP en sobrecarga, latencia y consumo de energía.
- Identificar las partes de un mensaje CoAP: tipo (CON, NON, ACK, RST), código (método o respuesta), Message ID, Token y opciones como Uri-Path.
- Implementar un servidor con dos recursos, `/sensor` y `/led`, registrando cada uno con su función *callback*.
- Validar la comunicación con un cliente real, interpretando la respuesta y el registro del Monitor Serie.
- Diagnosticar los fallos típicos de red, puerto y configuración usando la tabla de la sección 9.

> **Regla de oro del laboratorio:** un paso a la vez y una evidencia por paso. Si algo falla, cambia una sola cosa y vuelve a probar; si cambias tres, no sabrás cuál era el problema.

---

## 📁 Archivos de este repositorio

Siguiendo la estructura de los demás laboratorios de este autor, los archivos están en la raíz del repositorio, listos para descargar:

| Archivo | Contenido |
| :--- | :--- |
| `README.md` | Esta práctica completa: teoría, esquemas, procedimiento, pruebas y rúbrica. |
| `Servidor_CoAP_ESP32.ino` | Código completo del servidor CoAP, **comentado línea por línea**. Ábrelo en Arduino IDE, cambia el SSID y la contraseña, y cárgalo. |
| `cliente_coap.py` | Cliente CoAP en Python con `aiocoap`, también comentado. Requiere `pip install aiocoap`. |

Para descargar todo el laboratorio: **Code → Download ZIP**, o descarga cada archivo con el botón *Raw*.

---

## 🛠 2. Materiales y requisitos previos

### Hardware

- 1 tarjeta de desarrollo ESP32 (NodeMCU / DevKit).
- 1 cable USB **de datos** (muchos cables de carga no transmiten datos: es la causa núm. 1 de que la placa no aparezca).
- 1 computadora (Windows, macOS o Linux).
- Red Wi-Fi de **2.4 GHz** accesible en el laboratorio. La ESP32 no se conecta a redes de 5 GHz.
- Opcional para el reto: 1 LED, 1 resistencia de 220 ohm y cables Dupont.

### Software

- Arduino IDE (versión 2.x recomendada).
- Soporte para tarjetas ESP32 instalado en el Administrador de Tarjetas.
- Librería **CoAPSimple** (autor: hirotakaster) instalada desde el Administrador de Librerías de Arduino.
- Herramienta de cliente CoAP (por ejemplo, la extensión CoAP Client para navegadores o el script `cliente_coap.py` de este repositorio).
- Opcional: Wireshark (sección 8.3).

> Antes de empezar: alumno y ESP32 deben estar en la **misma red Wi-Fi**. Si la computadora está en la red de alumnos y la placa en otra red, o la red tiene aislamiento entre clientes (*client isolation*), la petición nunca llegará aunque el código sea correcto.

---

## 🧠 3. Marco teórico: CoAP por dentro

### 3.1 Un protocolo web pensado para dispositivos pequeños

CoAP conserva el modelo REST y cambia el transporte. La forma de la pila es la misma; lo que cambia es el peso de cada capa:

| Capa | HTTP tradicional | CoAP (esta práctica) |
| :--- | :--- | :--- |
| Aplicación | HTTP (texto) | CoAP (binario, cabecera de 4 bytes) |
| Seguridad (opcional) | TLS | DTLS |
| Transporte | TCP (con conexión) | **UDP (sin conexión)** |
| Red | IP | IP |

#### Comparación directa: HTTP frente a CoAP

| Aspecto | HTTP sobre TCP | CoAP sobre UDP |
| :--- | :--- | :--- |
| Conexión previa | Sí: *handshake* TCP (y otro TLS si es HTTPS) | No: el primer datagrama ya es la petición |
| Formato | Texto, cabeceras de cientos de bytes | Binario, cabecera fija de solo 4 bytes |
| Fiabilidad | La garantiza TCP, siempre activa | Opcional: solo en mensajes CON, con ACK y reintento |
| Consumo | Mayor: conexión abierta y más radio encendida | Menor: la placa duerme entre datagramas |
| Caso típico | Páginas y API en servidores y teléfonos | Sensores, actuadores y nodos con batería |

### 3.2 Anatomía de un mensaje CoAP

Todo mensaje CoAP empieza con una **cabecera fija de 4 bytes**. Después pueden venir un Token (que empareja petición y respuesta), una lista de opciones (como la ruta del recurso, Uri-Path) y, separado por un marcador de `0xFF`, el contenido o *payload*.

```
 Ver (2 bits) | T (2 bits) | TKL (4 bits) | Code (8 bits) | Message ID (16 bits) | Token (0-8 B)
 versión = 1  | CON / NON  | long. Token  | 0.01 = GET    | detecta duplicados   | une petición-respuesta
              | ACK / RST  |              | 2.05 = Content|                      |
 Opciones: Uri-Path = "sensor", Content-Format, etc. | 0xFF | Payload: "Temp: 26.2 C"
```

En el código, `packet.messageid` y `packet.payload` son exactamente estos dos campos.

### 3.3 Los cuatro tipos de mensaje: la fiabilidad, a la carta

| Tipo | Significado | Cuándo se usa |
| :--- | :--- | :--- |
| **CON** | Confirmable: exige un ACK | Comandos que no pueden perderse, p. ej. actuar sobre `/led` |
| **NON** | No confirmable: se envía y se olvida | Lecturas periódicas donde perder una no es grave |
| **ACK** | Confirmación de recepción de un CON | Puede viajar solo o llevar la respuesta "a cuestas" (*piggyback*) |
| **RST** | Reset: el mensaje no pudo procesarse | Errores de formato o contextos desconocidos |

El intercambio confirmable normal de esta práctica:

```
Cliente CoAP                              ESP32 (servidor)
    |  1. CON, GET /sensor, Message ID = 0x7A3F   |
    |-------------------------------------------->|
    |  2. ACK, 2.05 Content, mismo Message ID     |
    |     payload: Temp: 26.2 C                   |
    |<--------------------------------------------|
    |  3. Si el ACK no llega, el cliente reenvía  |
    |     el CON con el mismo Message ID. El      |
    |     servidor reconoce el duplicado y no     |
    |     procesa la petición dos veces.          |
```

### 3.4 Métodos y códigos de respuesta

Los métodos son los de REST: `GET` lee un recurso, `POST` crea o procesa, `PUT` actualiza y `DELETE` elimina. Las respuestas usan códigos con la misma lógica que HTTP pero en formato clase.detalle: **2.05 Content** equivale a un 200 con contenido, **2.04 Changed** a una actualización correcta, **4.04 Not Found** a una ruta inexistente y **4.05 Method Not Allowed** a un método no soportado en ese recurso. Si pides `/temperatura` en lugar de `/sensor`, recibirás un 4.04: pruébalo a propósito en la sección 8.

---

## 🔌 4. Esquema de funcionamiento y flujo de información

En el laboratorio hay tres protagonistas: el cliente (tu computadora), la red Wi-Fi (que solo transporta datagramas UDP) y la ESP32 (que escucha, decide qué recurso se pidió y responde). Dentro de la ESP32, la librería CoAPSimple hace de repartidor: mira la ruta de la petición y llama a la función *callback* que registraste para esa ruta.

```
CLIENTE CoAP                 ROUTER Wi-Fi              ESP32 - SERVIDOR CoAP
PC / navegador / Python      Red 2.4 GHz               WiFiUDP escucha en el puerto 5683
GET /sensor                  Entrega datagramas        coap.loop() recibe y despacha peticiones
                             UDP puerto 5683
coap://192.168.1.50                                     /sensor                  /led
/sensor                                                 callback_sensor()        callback_led()

                                                                               Monitor Serie a 115200 baudios
```

> Observa que el router no interpreta CoAP: solo entrega datagramas UDP entre dos direcciones IP.

### El viaje de una petición, en 6 pasos

1. **Arranque:** la ESP32 se conecta al Wi-Fi y el router le asigna una IP (p. ej. `192.168.1.50`). Esa IP es la dirección de tu servidor.
2. **Escucha:** `coap.start()` abre el puerto UDP 5683 y `coap.loop()` queda esperando datagramas.
3. **Petición:** el cliente envía un `GET` a `coap://192.168.1.50/sensor`. La opción Uri-Path vale `"sensor"`.
4. **Despacho:** CoAPSimple busca un recurso registrado con ese nombre y ejecuta su *callback*.
5. **Procesamiento:** el *callback* genera el dato (aquí, una temperatura simulada) y construye la respuesta.
6. **Respuesta:** `coap.sendResponse()` devuelve el *payload* al IP y puerto del cliente, reutilizando el Message ID de la petición para que el cliente sepa a qué petición corresponde.

**Idea clave para el reporte:** en CoAP no hay "páginas" ni "conexión con el servidor". Hay recursos con nombre, sobre los que un cliente ejecuta métodos, y cada intercambio es un par de datagramas independientes.

---

## ⚙️ 5. Configuración del entorno e instalación

1. Instala el soporte ESP32. En Arduino IDE ve a *Archivo → Preferencias* y, después, a *Herramientas → Placa → Administrador de Tarjetas*. Busca `"esp32"` (de Espressif Systems) e instálalo. Sin este paso, la placa no aparecerá en el menú.
2. Instala la librería CoAPSimple. *Herramientas → Administrar Bibliotecas* (Ctrl + Mayús + I). Busca **CoAPSimple, de hirotakaster**, e instálala con sus dependencias si las solicita.
3. Conecta la ESP32 por USB y selecciona la placa en *Herramientas → Placa → ESP32 Arduino → ESP32 Dev Module* (o el modelo exacto de tu tarjeta).
4. Elige el puerto serie correcto en *Herramientas → Puerto*. Si no aparece ninguno, cambia el cable antes de tocar cualquier otra cosa.
5. Prepara las credenciales: ten a la mano el nombre (SSID) y la contraseña de la red Wi-Fi de 2.4 GHz del laboratorio; los escribirás en el código de la sección 6.

### Lista de verificación antes de compilar

- [ ] La placa seleccionada es una ESP32, no una Arduino Uno u otra.
- [ ] El puerto serie corresponde a la ESP32 (desconecta y observa cuál desaparece).
- [ ] El Monitor Serie está configurado a 115200 baudios.
- [ ] La librería instalada es CoAPSimple de hirotakaster (hay otras con nombres parecidos).
- [ ] SSID y contraseña están escritos sin espacios extra y con mayúsculas correctas.

### ¿Cómo se organiza el programa? `setup()` y `loop()`

```
ENCENDIDO / RST
se ejecuta una vez
        |
        v
setup()
  1. Inicia el puerto serie
  2. Conecta el Wi-Fi
  3. Registra /sensor y /led
  4. Arranca el servidor (puerto 5683)
        |
        v
loop()  -- se repite para siempre --
  coap.loop() escucha:
  ¿llegó un datagrama?
  Sí: ejecuta el callback y responde.
  No: sigue.
```

**Error clásico:** poner un `delay()` largo o un ciclo pesado dentro de `loop()`. Mientras ese código corre, nadie llama a `coap.loop()` y las peticiones se pierden. En esta práctica, `loop()` debe quedar prácticamente vacío.

---

## 💻 6. Código del servidor CoAP, comentado línea por línea

El código completo está en **`Servidor_CoAP_ESP32.ino`**. Cópialo, reemplaza las credenciales Wi-Fi y cárgalo en tu ESP32. Lee los comentarios: cada uno explica qué hace la línea y por qué está ahí.

Resumen de lo que hace cada parte:

- **Librerías:** `WiFi.h` maneja la conexión, `WiFiUdp.h` los datagramas UDP y `coap-simple.h` interpreta CoAP y despacha cada petición a su *callback*.
- **`callback_led()`:** lee el *payload* del mensaje. Como `packet.payload` es un arreglo de bytes sin carácter de fin, se copian `payloadlen` bytes y se agrega `'\0'` para tratarlo como texto. Luego responde con `coap.sendResponse(ip, port, packet.messageid, ...)`, reutilizando el Message ID para emparejar petición y respuesta.
- **`callback_sensor()`:** genera una temperatura simulada (`25.4 + random(0, 50) / 10.0`, entre 25.4 y 30.3 °C) y la devuelve como texto plano, por ejemplo `Temp: 27.1 C`.
- **`setup()`:** inicia el Monitor Serie a 115200, conecta al Wi-Fi esperando con puntos cada 0.5 s, imprime la IP del servidor, registra los recursos con `coap.server(callback_led, "led")` y `coap.server(callback_sensor, "sensor")`, y arranca el servidor con `coap.start()` en el puerto UDP 5683.
- **`loop()`:** solo llama a `coap.loop()`, que revisa si llegó un datagrama y, si llegó, ejecuta el *callback* que corresponda.

> Para discutir en clase: ¿qué pasaría si dos clientes pidieran `/led` al mismo tiempo? El servidor procesa un *callback* a la vez; cada respuesta se dirige al IP y puerto de quien preguntó, y el Message ID evita que un cliente acepte la respuesta del otro.

---

## 🐍 7. Un cliente CoAP en Python, también comentado

La extensión de navegador sirve para una primera prueba, pero un script permite repetir la petición, medir tiempos y registrar resultados. El código completo está en **`cliente_coap.py`**.

```bash
pip install aiocoap
python cliente_coap.py
```

Sustituye la IP por la que mostró tu ESP32. El script hace dos peticiones: un `GET` a `/sensor` (esperado: código 2.05 y `Temp: .. C`) y un `POST` a `/led` con payload `ON` (el *callback* de la ESP32 lo imprimirá en el Monitor Serie).

**Lo que debes observar al ejecutarlo:**

- El script termina en menos de un segundo y nunca abrió una conexión: envió dos datagramas y esperó dos respuestas.
- Cada respuesta trae su propio código (2.05) y su propio *payload*; el emparejamiento petición-respuesta lo hace el Token y el Message ID, no una conexión.
- Si apagas la ESP32 y repites, la petición queda sin respuesta y aiocoap agota sus reintentos: es la fiabilidad "a la carta" de la sección 3.3 actuando en el cliente.

**Experimento rápido:** ejecuta el script 5 veces seguidas y anota las temperaturas. Deberían variar entre 25.4 y 30.3 °C. Si siempre recibes exactamente el mismo valor, sospecha que estás hablando con la placa de otra mesa: verifica la IP.

---

## ✅ 8. Procedimiento de pruebas y validación

### 8.1 Puesta en marcha

1. Carga el código de la sección 6 y abre el Monitor Serie a 115200 baudios.
2. Presiona el botón RST (Reset) de la ESP32 y observa el arranque completo.
3. Anota la dirección IP que muestra la consola al conectarse al Wi-Fi (ejemplo: `192.168.1.50`). Sin esa IP no hay cliente posible.

### 8.2 Pruebas obligatorias

| # | Petición | Resultado esperado | Evidencia a entregar |
| :--- | :--- | :--- | :--- |
| **P1** | `GET coap://<IP>/sensor` | Respuesta 2.05 con `Temp: XX.X C` en el puerto UDP 5683 | Captura del cliente con la respuesta |
| **P2** | `POST coap://<IP>/led` con payload `ON` | Respuesta `"Estado: OK..."` y en el Monitor Serie: `"Payload recibido: ON"` | Captura del cliente y del Monitor Serie |
| **P3** | `GET coap://<IP>/temperatura` (ruta inexistente, a propósito) | Error 4.04 Not Found | Captura y una frase explicando por qué ocurre |
| **P4** | Repetir P1 cinco veces | Cinco temperaturas distintas entre 25.4 y 30.3 °C | Tabla con los cinco valores |

**Criterio de práctica aprobada:** P1 y P2 funcionan y están documentadas; P3 se interpreta correctamente (no es un fallo de tu código, es el servidor diciendo que esa ruta no fue registrada en `setup()`); P4 muestra valores variables. Si P1 funciona pero P2 no, revisa el método elegido en el cliente: `/led` en esta práctica responde a la petición que tu cliente envíe, pero el experimento está diseñado como POST con payload.

### 8.3 Mirar el protocolo con tus propios ojos (Wireshark)

Abre Wireshark, captura sobre la interfaz Wi-Fi y usa el filtro `coap` (o `udp.port == 5683`). Repite la prueba P1 y localiza el par petición-respuesta. En el reporte, identifica y anota: el tipo de mensaje (CON), el Message ID de la petición, si la respuesta lleva el mismo Message ID, el código 2.05 y los bytes del *payload*. Es la Figura de la sección 3.2, pero con tráfico real generado por tu placa.

> Nota: algunos clientes y extensiones usan mensajes NON por defecto y otros CON. Si en tu captura ves NON en lugar de CON, no es un error: anótalo y explica con la tabla de la sección 3.3 qué garantía se pierde y qué se gana.

---

## 🛠️ 9. Solución de problemas frecuentes

| Síntoma | Causa más probable | Qué hacer |
| :--- | :--- | :--- |
| La ESP32 no aparece en el puerto serie | Cable USB solo de carga, o falta el controlador | Cambia el cable por uno de datos; prueba otro puerto USB |
| El Monitor Serie muestra puntos sin fin al conectar | SSID/contraseña incorrectos o red de 5 GHz | Revisa mayúsculas y conecta la placa a la red de 2.4 GHz |
| Monitor Serie con caracteres ilegibles | Velocidad distinta de la del código | Ajusta el Monitor Serie a 115200 baudios |
| La placa tiene IP, pero el cliente da *timeout* | Computadora y placa en redes distintas, o aislamiento entre clientes en el router | Conecta ambos a la misma red; prueba con el hotspot de un teléfono si el laboratorio lo permite |
| *Timeout* solo en el cliente, con todo en la misma red | Firewall de la computadora bloqueando UDP | Permite el tráfico UDP de la aplicación cliente o prueba el script de Python |
| Respuesta 4.04 Not Found | Ruta escrita distinto de la registrada (`/Sensor`, `/sensor/`) | CoAP distingue mayúsculas; la ruta debe coincidir con `coap.server(...)` |
| Error de compilación en `coap-simple.h` | Librería no instalada o una homónima incorrecta | Instala CoAPSimple de hirotakaster desde el Administrador de Librerías |
| El servidor deja de responder tras unos minutos | `loop()` bloqueado o reconexión Wi-Fi perdida | Quita retardos de `loop()`; reinicia con RST y observa el Monitor Serie |

---

## ❓ 10. Preguntas de análisis

Responde en el reporte con tus propias palabras y citando lo que observaste en tus pruebas.

1. ¿Por qué CoAP puede permitirse usar UDP, si UDP no garantiza la entrega? ¿Qué mecanismo compensa esa falta y en qué casos ni siquiera se usa?
2. En tu captura de Wireshark, ¿la respuesta de `/sensor` fue un ACK independiente o viajó "a cuestas" en el mismo mensaje que el *payload*? ¿Cómo lo sabes?
3. ¿Qué campo te permitió saber que la respuesta correspondía a tu petición y no a la de un compañero? Relaciónalo con la sección 3.2.
4. Una petición HTTP/1.1 mínima ocupa varios cientos de bytes con sus cabeceras. Calcula cuántos bytes ocupa tu mensaje CoAP de respuesta en la captura y compara.
5. Si este servidor controlara una bomba de riego con batería, ¿usarías mensajes CON o NON para reportar la humedad cada minuto? Justifica el costo en energía de tu elección.
6. ¿Qué riesgo de seguridad implica un servidor CoAP sin DTLS dentro de una red compartida, y quién podría explotarlo en el laboratorio?

### Retos opcionales (puntos extra)

**Reto 1: que `/led` encienda un LED de verdad.**
Conecta un LED con su resistencia al GPIO 2. En `callback_led()`, compara el *payload*: si es `"ON"` usa `digitalWrite(2, HIGH)` y si es `"OFF"`, `digitalWrite(2, LOW)`, y responde con el estado real. No olvides `pinMode(2, OUTPUT)` en `setup()`.

**Reto 2: sensor real.**
Sustituye la temperatura simulada por la lectura de un sensor disponible en tu mesa (por ejemplo, un DHT11/DHT22 o el sensor interno aproximado) y documenta qué líneas de `callback_sensor()` cambiaron y cuáles no. Observa que el cliente no nota ninguna diferencia: eso es la abstracción por recursos.

---

## 🏆 Rúbrica de evaluación (100 puntos)

| Criterio | Puntos | Evidencia |
| :--- | :---: | :--- |
| Servidor montado y conectado al Wi-Fi | 15 | Captura del Monitor Serie con la IP asignada |
| Prueba P1: GET `/sensor` funcional | 20 | Captura del cliente con respuesta 2.05 |
| Prueba P2: POST `/led` con payload | 20 | Capturas del cliente y del Monitor Serie |
| Pruebas P3 y P4 interpretadas | 15 | Explicación del 4.04 y tabla de 5 temperaturas |
| Captura Wireshark analizada | 15 | Campos de la sección 3.2 identificados en tráfico real |
| Preguntas de análisis | 15 | Respuestas fundamentadas en lo observado |

---

## 📝 11. Conclusiones

- El uso de CoAP sobre la ESP32 permite integrar dispositivos IoT en arquitecturas de red eficientes, consumiendo menos recursos que un servidor HTTP tradicional basado en TCP: no hay conexión que mantener y cada intercambio cabe en dos datagramas.
- La librería CoAPSimple facilita el mapeo rápido de rutas mediante *callbacks* orientados a eventos: registrar un recurso es asociar un nombre a una función, y todo el trabajo del protocolo (tipos de mensaje, Message ID, códigos) queda resuelto por la librería.
- La fiabilidad en CoAP es una decisión por mensaje, no una propiedad del transporte: CON cuando el dato no puede perderse, NON cuando la siguiente lectura llegará en segundos de todos modos.
- El límite también quedó visible: sin DTLS no hay cifrado ni autenticación, y el texto plano es solo el primer paso hacia formatos compactos como JSON o CBOR.

## Referencias

- Shelby, Z., Hartke, K. y Bormann, C. (2014). *The Constrained Application Protocol (CoAP)*, RFC 7252, IETF.
- Librería CoAPSimple para Arduino, de hirotakaster, disponible en el Administrador de Librerías del Arduino IDE.
