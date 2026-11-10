# cliente_coap.py - Cliente CoAP mínimo con aiocoap
# Uso: python cliente_coap.py
# Instalación: pip install aiocoap
# Sustituye la IP por la que mostró tu ESP32 en el Monitor Serie.
import asyncio
from aiocoap import Context, Message, GET, POST

IP_ESP32 = "192.168.1.50"        # <-- la IP de TU placa

async def main():
    # Se crea el contexto del cliente: aiocoap abrirá un
    # socket UDP efímero (un puerto libre cualquiera).
    contexto = await Context.create_client_context()

    # --- Petición 1: GET al recurso /sensor ----------------
    # Equivale a escribir coap://IP/sensor en la extensión.
    peticion = Message(code=GET, uri=f"coap://{IP_ESP32}/sensor")
    respuesta = await contexto.request(peticion).response
    print("Código de respuesta:", respuesta.code)      # esperado: 2.05
    print("Payload:", respuesta.payload.decode())      # esperado: Temp: .. C

    # --- Petición 2: POST al recurso /led ------------------
    # El payload viaja en el cuerpo del mensaje; el callback
    # callback_led() de la ESP32 lo imprimirá en el Monitor Serie.
    peticion2 = Message(code=POST, uri=f"coap://{IP_ESP32}/led",
                        payload=b"ON")
    respuesta2 = await contexto.request(peticion2).response
    print("Respuesta /led:", respuesta2.payload.decode())

asyncio.run(main())
