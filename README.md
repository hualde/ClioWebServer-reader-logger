# Flujo del Código CAN (Controller Area Network)

Este código implementa un sistema de comunicación CAN (Controller Area Network) utilizando el controlador TWAI (Two-Wire Automotive Interface) en un dispositivo ESP32. A continuación se describe el flujo principal del programa:

1. Inicialización:
   - Se configuran los parámetros del controlador TWAI.
   - Se inicializa el controlador TWAI con los pines GPIO especificados.
   - Se crean semáforos y colas para la sincronización de tareas.

2. Tarea de Transmisión:
   - Se ejecuta en un bucle continuo.
   - Cada segundo, se prepara y envía una trama CAN con un identificador específico.
   - El último byte de la trama se genera aleatoriamente.
   - Se implementa un sistema de "enfriamiento" después de enviar un número determinado de tramas.
   - Se manejan errores de transmisión con un sistema de reintentos y recuperación.

3. Tarea de Recepción:
   - Se ejecuta en paralelo con la tarea de transmisión.
   - Constantemente escucha el bus CAN para recibir mensajes.
   - Cuando se recibe un mensaje, se registra en el log del sistema.

4. Manejo de Errores y Recuperación:
   - Se implementa un sistema de detección y manejo de errores.
   - Si se producen errores consecutivos, se inicia un procedimiento de recuperación.
   - El procedimiento de recuperación incluye la reinicialización del controlador TWAI.

5. Monitoreo del Estado:
   - Periódicamente se verifica el estado del controlador TWAI.
   - Se registran estadísticas como mensajes enviados/recibidos y errores.
   - Si se detecta un estado de "bus-off", se inicia un proceso de recuperación.

6. Reinicio Automático:
   - El sistema se reinicia automáticamente cada 30 minutos para mantener la estabilidad.

7. Gestión de Buffers:
   - Se implementa un sistema para evitar el desbordamiento de los buffers de transmisión y recepción.
   - Si los buffers se acercan a su capacidad máxima, se limpian automáticamente.

Este flujo asegura una comunicación CAN robusta y estable, con mecanismos integrados para manejar errores y mantener el sistema en funcionamiento a largo plazo."# ClioWebServer_reader" 
