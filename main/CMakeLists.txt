idf_component_register(
    SRCS main.c CAN.c  # Reemplazamos log_reader.c por CAN.c
    INCLUDE_DIRS "."   # Esto ya incluye el directorio actual, donde estará CAN.h
    PRIV_INCLUDE_DIRS  # opcional, añade aquí directorios de inclusión privados
    REQUIRES           # opcional, lista los requisitos públicos (nombres de componentes)
    PRIV_REQUIRES      # opcional, lista los requisitos privados
)