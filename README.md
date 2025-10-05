# Sistema de Telemetría de Vehículo Autónomo

Sistema de telemetría para vehículo autónomo desarrollado en C (servidor) con clientes en Python y Java.

## Requisitos y Configuración de Terminales

### Servidor (C)
- GCC con soporte para C99
- POSIX threads (pthread)
- Make
- **Terminal requerido: WSL (Windows Subsystem for Linux)**

### Cliente Python
- Python 3.6+
- tkinter (generalmente incluido)
- **Terminal requerido: PowerShell o CMD**

### Cliente Java
- Java JDK 8+
- Swing (incluido en JDK)
- **Terminal requerido: PowerShell o CMD**

### ¿Por qué diferentes terminales?
- **Servidor:** Compilado como ejecutable Linux (ELF) → Necesita WSL
- **Clientes:** Aplicaciones Windows nativas → Usan PowerShell/CMD

## Compilación

### 1. Compilar Servidor (usar WSL)
```bash
cd server
make
```

### 2. Compilar Cliente Java (usar PowerShell/CMD)
```bash
cd client-java
javac *.java
```

### 3. Verificar Cliente Python (usar PowerShell/CMD)
```bash
cd client-python
python -m compileall .
```
> **Nota:** Python no requiere compilación. Este comando solo verifica la sintaxis.

## Ejecución

### 1. Iniciar Servidor (usar WSL)
```bash
cd server
./server <puerto> <archivo_log>
```

**Ejemplo:**
```bash
./server 8080 vehicle_logs.txt
```

### 2. Iniciar Cliente Python (usar PowerShell)
```bash
cd client-python
python client.py
```

### 3. Iniciar Cliente Java (usar PowerShell)
```bash
cd client-java
java LoginGUI
```

## Usuarios de Prueba

### Administradores (pueden enviar comandos)
- **Username:** `admin1` - **Password:** `adminpass`
- **Username:** `admin2` - **Password:** `admin123`

### Observadores (solo reciben telemetría)
- **Username:** `observer1` - **Password:** `obspass`
- **Username:** `observer2` - **Password:** `obs123`

## Comandos Disponibles (Solo Admin)

- **SPEED UP:** Incrementa velocidad (+10 km/h, consume batería)
- **SLOW DOWN:** Reduce velocidad (-10 km/h)
- **TURN LEFT:** Gira a la izquierda (cambia dirección)
- **TURN RIGHT:** Gira a la derecha (cambia dirección)
- **LIST USERS:** Muestra usuarios conectados

## Telemetría

El servidor envía cada **10 segundos** a todos los clientes conectados:

- **Velocidad:** 0-100 km/h
- **Batería:** 0-100% (se consume con velocidad y comandos)
- **Temperatura:** Variable (-10°C a 50°C)
- **Dirección:** NORTH, EAST, SOUTH, WEST

## Características Técnicas

### Protocolo
- **Formato:** `[TIPO]|[DATOS]\n`
- **Transporte:** TCP (SOCK_STREAM) para toda la comunicación
- **Tipos:** AUTH, CMD, RESP, TELEM, LIST

### Concurrencia
- **Servidor:** 1 hilo por cliente + 1 hilo de telemetría
- **Sincronización:** Mutexes para datos compartidos

### Logging
- **Formato:** `[TIMESTAMP] [IP:PORT] [USERNAME] [TYPE] [MESSAGE]`
- **Tipos:** CONN (conexión), AUTH (autenticación), REQ (comando), RESP (respuesta), DISC (desconexión)
- **Destinos:** Consola + archivo especificado

## Estructura del Proyecto

```
vehiculo-autonomo/
├── server/
│   ├── server.c        # Servidor principal en C
│   └── Makefile        # Compilación
├── client-python/
│   └── client.py       # Cliente Python con GUI
├── client-java/
│   ├── LoginGUI.java       # Interfaz de autenticación
│   ├── TelemetryGUI.java   # Interfaz de telemetría
│   ├── VehicleClient.java  # Cliente principal
│   ├── TelemetryData.java  # Datos de telemetría
│   └── UserType.java       # Tipos de usuario
├── docs/
│   ├── protocol-specification.md   # RFC del protocolo
│   └── tcp-justification.md        # Justificación TCP vs UDP
├── .gitignore          # Archivos excluidos de Git
└── README.md           # Este archivo
```

## Documentación del Protocolo

Para información detallada sobre el protocolo de comunicación:

- **Especificación del Protocolo:** Ver `docs/protocol-specification.md` para la especificación RFC del protocolo VATP
- **Decisión de la Capa de Transporte:** Ver `docs/tcp-justification.md` para la justificación técnica de usar TCP sobre UDP

## Pruebas

### Escenario Básico
1. Iniciar servidor en puerto 8080
2. Conectar admin1 desde cliente Python
3. Conectar observer1 desde cliente Java
4. Admin envía comandos SPEED UP
5. Ambos clientes reciben telemetría actualizada
6. Admin consulta LIST USERS

### Verificación de Restricciones
1. Observer intenta enviar comando → Error de permisos
2. SPEED UP con batería baja → Comando denegado
3. SPEED UP superando 100 km/h → Velocidad máxima alcanzada

## Solución de Problemas

### Servidor no inicia
- Verificar que el puerto no esté en uso: `netstat -an | grep :8080`
- Usar otro puerto: `./server 8081 logs.txt`
- **Usar WSL:** El servidor requiere terminal Linux (WSL)

### Cliente Java no compila
- **Compilar primero:** `javac *.java` antes de `java LoginGUI`
- **No usar .java:** Ejecutar `java LoginGUI` (sin extensión)
- **Usar PowerShell:** Terminal Windows para clientes

### Cliente no conecta
- Verificar que el servidor esté ejecutándose
- Confirmar host y puerto correctos
- Revisar firewall/antivirus

### Autenticación falla
- Verificar username/password exactos (case-sensitive)
- Usar credenciales de prueba listadas arriba