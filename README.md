# Visor CAN ESP32

## Descripción general

El Visor CAN ESP32 es un proyecto que te permite monitorear mensajes del bus CAN utilizando un microcontrolador ESP32. Crea un punto de acceso Wi-Fi y aloja un servidor web, permitiendo a los usuarios ver mensajes CAN en tiempo real a través de una interfaz de navegador web.

## Características

- Crea un punto de acceso Wi-Fi para una conexión fácil
- Aloja un servidor web con una interfaz amigable para el usuario
- Muestra mensajes CAN en tiempo real
- Implementa un buffer circular para almacenar mensajes CAN recientes
- Proporciona una ventana desplazable en la interfaz web para una mejor visualización de mensajes
- Actualiza y se desplaza automáticamente para mostrar los últimos mensajes
- Evita la visualización de mensajes duplicados

## Requisitos de hardware

- Placa de desarrollo ESP32
- Transceptor CAN (por ejemplo, SN65HVD230)
- Conexión al bus CAN para monitorear

## Requisitos de software

- ESP-IDF (Espressif IoT Development Framework)
- Navegador web (para ver los mensajes CAN)

## Configuración e instalación

1. Clona este repositorio en tu máquina local.
2. Abre el proyecto en tu entorno de desarrollo ESP-IDF.
3. Configura tu proyecto si es necesario (por ejemplo, ajustando los pines GPIO para la conexión CAN).
4. Compila y flashea el proyecto en tu placa ESP32.

## Uso

1. Enciende tu placa ESP32.
2. Conéctate al punto de acceso Wi-Fi creado por el ESP32. Por defecto, el SSID es "ESP32_CAN_Viewer" y no hay contraseña.
3. Abre un navegador web y navega a `http://192.168.4.1` (dirección IP predeterminada del AP ESP32).
4. Verás la interfaz del Visor CAN mostrando los mensajes CAN entrantes en tiempo real.

## Componentes principales

- `main.c`: Contiene el código principal de la aplicación, incluyendo la configuración Wi-Fi, la inicialización del servidor web y el manejo de mensajes CAN.
- `CAN.h` y `CAN.c` (no mostrados en el código proporcionado): Implementan las funciones de inicialización del bus CAN y recuperación de mensajes.

## Funciones clave

- `wifi_init_softap()`: Inicializa el ESP32 como un punto de acceso Wi-Fi.
- `start_webserver()`: Inicia el servidor HTTP y el servidor WebSocket.
- `http_server_handler()`: Maneja las solicitudes HTTP y sirve la página HTML principal.
- `websocket_handler()`: Gestiona las conexiones WebSocket para actualizaciones en tiempo real.
- `add_can_message()`: Añade nuevos mensajes CAN al buffer circular.
- `get_all_can_messages()`: Recupera todos los mensajes CAN almacenados para su visualización.
- `can_message_task()`: Tarea en segundo plano que verifica continuamente nuevos mensajes CAN.

## Personalización

Puedes personalizar el proyecto:
- Modificando el SSID y la contraseña Wi-Fi en la función `wifi_init_softap()`.
- Ajustando el número máximo de mensajes CAN almacenados cambiando la definición de `MAX_CAN_MESSAGES`.
- Personalizando el diseño y estilo de la interfaz web en la función `http_server_handler()`.

## Solución de problemas

Si encuentras algún problema:
- Asegúrate de que tu ESP32 esté correctamente conectado al bus CAN.
- Verifica que estés conectado a la red Wi-Fi correcta.
- Comprueba que tu navegador web soporte WebSockets.
- Limpia la caché de tu navegador si no ves actualizaciones.

## Contribuciones

Las contribuciones para mejorar el Visor CAN ESP32 son bienvenidas. Por favor, siéntete libre de enviar pull requests o abrir issues para bugs y solicitudes de características.

## Licencia

Este proyecto es de código abierto y está disponible bajo la [Licencia MIT](https://opensource.org/licenses/MIT).