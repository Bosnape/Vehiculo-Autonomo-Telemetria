# RFC - Protocolo de Telemetría de Vehículo Autónomo (VATP)

**Solicitud de Comentarios:** VATP  
**Categoría:** Estándar  
**Autores:** Equipo de Telemetría de Vehículo Autónomo  
**Fecha:** Octubre 2025  

---

## Resumen

Este documento especifica el Protocolo de Telemetría de Vehículo Autónomo (VATP), un protocolo de capa de aplicación diseñado para la comunicación entre un sistema de vehículo autónomo simulado y múltiples clientes de monitoreo y control. El protocolo proporciona transmisión de telemetría en tiempo real, autenticación de usuarios con diferentes niveles de privilegios, y ejecución de comandos de control con validaciones de estado.

---

## 1. Visión General del Protocolo

### 1.1 Propósito y Alcance

El Protocolo de Telemetría de Vehículo Autónomo (VATP) facilita la comunicación bidireccional entre:

- **Servidor de Vehículo:** Entidad central que simula el comportamiento de un vehículo autónomo, mantiene su estado interno y gestiona múltiples conexiones concurrentes
- **Clientes de Monitoreo/Control:** Aplicaciones distribuidas que monitorizan telemetría en tiempo real y opcionalmente envían comandos de control

### 1.2 Modelo de Funcionamiento

VATP implementa un **modelo cliente-servidor centralizado** con las siguientes características:

- **Arquitectura:** Un servidor único acepta múltiples conexiones TCP concurrentes
- **Concurrencia:** Modelo de hilos (threads) con un hilo por cliente más un hilo dedicado de telemetría
- **Comunicación:** Bidireccional asíncrona con transmisión automática de telemetría
- **Persistencia:** Todas las interacciones son registradas con logging estructurado

### 1.3 Posición en el Stack TCP/IP

```
┌─────────────────────────────────────┐
│     VATP (Capa de Aplicación)       │
├─────────────────────────────────────┤
│          TCP (Transporte)           │
├─────────────────────────────────────┤
│           IP (Internet)             │
├─────────────────────────────────────┤
│        Ethernet (Acceso)            │
└─────────────────────────────────────┘
```

VATP opera en la **capa de aplicación** utilizando:
- **TCP (SOCK_STREAM)** como protocolo de transporte para garantizar entrega confiable
- **Berkeley Sockets API** para la implementación de red
- **Formato de texto plano** para facilitar debugging e interoperabilidad

### 1.4 Justificación del Protocolo de Transporte

**¿Por qué TCP en lugar de UDP?**

La decisión de utilizar TCP (SOCK_STREAM) se fundamenta en:

1. **Comandos críticos:** Los comandos de control del vehículo requieren entrega garantizada
2. **Gestión de conexiones:** TCP permite detectar automáticamente desconexiones de clientes
3. **Orden de mensajes:** La telemetría debe mantener secuencia temporal correcta
4. **Autenticación segura:** Las credenciales necesitan transmisión confiable
5. **Logging completo:** Todas las interacciones deben ser registrables sin pérdidas

---

## 2. Especificación del Servicio

### 2.1 Primitivas del Servicio

VATP define las siguientes operaciones que constituyen la interfaz del protocolo:

| Primitiva | Dirección | Propósito | Usuarios Autorizados | Frecuencia |
|-----------|-----------|-----------|---------------------|------------|
| `AUTH` | C→S | Autenticación e identificación de usuario | Todos | Una vez por sesión |
| `TELEM` | S→C | Transmisión de datos de telemetría | Autenticados | Cada 10 segundos |
| `CMD` | C→S | Envío de comandos de control | Solo ADMIN | Bajo demanda |
| `RESP` | S→C | Respuesta a comandos y autenticación | Todos | Reactiva |
| `LIST` | S→C | Lista de usuarios conectados | Solo ADMIN | Bajo demanda |

### 2.2 Modelo de Usuarios y Autorización

#### 2.2.1 Tipos de Usuario

```c
typedef enum {
    USER_ADMIN,    // Acceso completo: comandos + telemetría + consultas
    USER_OBSERVER  // Solo lectura: telemetría únicamente
} user_type_t;
```

#### 2.2.2 Matriz de Permisos

| Operación | ADMIN | OBSERVER |
|-----------|-------|----------|
| Recibir telemetría | ✓ | ✓ |
| Enviar comandos de movimiento | ✓ | ✗ |
| Consultar lista de usuarios | ✓ | ✗ |
| Autenticarse | ✓ | ✓ |

#### 2.2.3 Usuarios Válidos (Hardcoded)

```c
valid_user_t VALID_USERS[] = {
    {"admin1", "adminpass", USER_ADMIN},
    {"admin2", "admin123", USER_ADMIN},
    {"observer1", "obspass", USER_OBSERVER},
    {"observer2", "obs123", USER_OBSERVER}
};
```

### 2.3 Comandos de Control Disponibles

| Comando | Función | Efecto en Estado | Restricciones |
|---------|---------|------------------|---------------|
| `SPEED UP` | Incrementar velocidad | +10 km/h, -2% batería | Batería ≥ 10%, Velocidad < 100 km/h |
| `SLOW DOWN` | Decrementar velocidad | -10 km/h | Velocidad > 0 km/h |
| `TURN LEFT` | Rotar hacia la izquierda | Cambio de dirección | Batería > 0% |
| `TURN RIGHT` | Rotar hacia la derecha | Cambio de dirección | Batería > 0% |
| `LIST USERS` | Consultar usuarios conectados | Ninguno | Solo ADMIN |

### 2.4 Estado del Vehículo

```c
typedef struct {
    float speed;        // 0.0 - 100.0 km/h (incrementos de 10)
    int battery;        // 0 - 100% (decrece con uso)
    float temperature;  // Variable (-10.0 a 50.0 °C)
    char direction[16]; // "NORTH", "EAST", "SOUTH", "WEST"
} vehicle_state_t;
```

**Lógica de Estado:**
- **Dirección:** Rotación cíclica horaria: NORTH → EAST → SOUTH → WEST → NORTH
- **Batería:** Consumo automático basado en velocidad (velocidad × 0.02% cada 10s)
- **Temperatura:** Variación aleatoria ±1.0°C cada ciclo de telemetría
- **Velocidad:** Si batería = 0%, velocidad forzada a 0.0 km/h

---

## 3. Formato de Mensajes Detallado

### 3.1 Estructura General del Protocolo

Todos los mensajes VATP siguen un formato uniforme basado en texto:

```
[TIPO]|[DATOS]\n
```

**Especificación de Campos:**
- `TIPO`: Identificador ASCII de 3-6 caracteres, case-sensitive
- `|`: Delimitador fijo (pipe character, ASCII 0x7C)
- `DATOS`: Payload variable según tipo de mensaje
- `\n`: Terminador obligatorio (newline, ASCII 0x0A)

**Características:**
- Codificación: ASCII/UTF-8
- Tamaño máximo por mensaje: 2048 bytes
- No hay escape de caracteres especiales
- Sensible a mayúsculas/minúsculas

### 3.2 Definición Detallada de Tipos de Mensaje

#### 3.2.1 Mensaje de Autenticación (AUTH)

**Propósito:** Identificar y autenticar cliente al servidor

**Formato:**
```
AUTH|[tipo_usuario]:[username]:[password]\n
```

**Especificación de Campos:**
- `tipo_usuario`: Enum string, valores: "ADMIN" | "OBSERVER"
- `username`: String alfanumérico, máximo 31 caracteres
- `password`: String ASCII, máximo 31 caracteres  
- Separador: ':' (colon, ASCII 0x3A)

**Ejemplos:**
```
AUTH|ADMIN:admin1:adminpass\n
AUTH|OBSERVER:observer1:obspass\n
```

**Validaciones del Servidor:**
1. Formato sintáctico correcto
2. Usuario existe en VALID_USERS[]
3. Password coincide exactamente
4. Tipo de usuario coincide

#### 3.2.2 Mensaje de Telemetría (TELEM)

**Propósito:** Transmitir estado actual del vehículo a todos los clientes autenticados

**Formato:**
```
TELEM|[velocidad]:[bateria]:[temperatura]:[direccion]\n
```

**Especificación de Campos:**
- `velocidad`: Float con 1 decimal (formato: "XX.X")
- `bateria`: Integer 0-100
- `temperatura`: Float con 1 decimal (formato: "XX.X")  
- `direccion`: Enum string: "NORTH"|"EAST"|"SOUTH"|"WEST"

**Ejemplo:**
```
TELEM|45.5:78:32.4:EAST\n
```

**Frecuencia:** Broadcast automático cada 10 segundos (TELEMETRY_INTERVAL)

#### 3.2.3 Mensaje de Comando (CMD)

**Propósito:** Enviar instrucciones de control del cliente al servidor

**Formato:**
```
CMD|[comando]\n
```

**Especificación de Campos:**
- `comando`: String literal, valores válidos:
  - "SPEED UP"
  - "SLOW DOWN" 
  - "TURN LEFT"
  - "TURN RIGHT"
  - "LIST USERS"

**Ejemplos:**
```
CMD|SPEED UP\n
CMD|TURN LEFT\n
CMD|LIST USERS\n
```

**Restricciones:**
- Solo usuarios tipo ADMIN pueden enviar comandos
- Comandos son case-sensitive
- Espacios internos son significativos

#### 3.2.4 Mensaje de Respuesta (RESP)

**Propósito:** Confirmar resultado de operaciones (autenticación, comandos)

**Formato:**
```
RESP|[status]:[mensaje]\n
```

**Especificación de Campos:**
- `status`: Enum string de resultado:
  - "OK": Operación exitosa
  - "ERROR": Error del sistema o formato
  - "DENIED": Operación rechazada por restricciones
- `mensaje`: String descriptivo libre

**Ejemplos:**
```
RESP|OK:Authenticated as ADMIN\n
RESP|DENIED:Low battery - cannot speed up\n
RESP|ERROR:Invalid command format\n
RESP|OK:Speed increased to 10.0 km/h\n
```

#### 3.2.5 Mensaje de Lista de Usuarios (LIST)

**Propósito:** Responder a comando LIST USERS con usuarios conectados actualmente

**Formato:**
```
LIST|[num_users]:[user1_info]:[user2_info]:...\n
```

**Especificación de Campos:**
- `num_users`: Integer, cantidad total de usuarios conectados
- `user_info`: Formato por usuario: "username-type-ip:port"
  - `username`: Nombre de usuario autenticado
  - `type`: "ADMIN" | "OBSERVER"  
  - `ip`: Dirección IP del cliente
  - `port`: Puerto TCP del cliente

**Ejemplo:**
```
LIST|2:admin1-ADMIN-127.0.0.1:45678:observer1-OBSERVER-192.168.1.100:54321\n
```

---

## 4. Reglas de Procedimiento y Máquinas de Estado

### 4.1 Máquina de Estados del Cliente

```
                    ┌─────────────────┐
                    │  DISCONNECTED   │
                    └────────┬────────┘
                             │ TCP Connect
                             ▼
                    ┌─────────────────┐
                    │   CONNECTED     │
                    └────────┬────────┘
                             │ Send AUTH
                             ▼
                    ┌─────────────────┐
                    │  AUTHENTICATING │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │   Receive RESP  │
                    └────────┬────────┘
                             │
                  ┌──────────┴──────────┐
                  │                     │
                  ▼ RESP|OK             ▼ RESP|ERROR
         ┌─────────────────┐    ┌─────────────────┐
         │  AUTHENTICATED  │    │  DISCONNECTED   │
         └────────┬────────┘    └─────────────────┘
                  │
                  ▼
         ┌─────────────────┐
         │     ACTIVE      │◄─┐
         │                 │  │
         │ • Receive TELEM │  │ Continuous
         │ • Send CMD      │  │ Operation
         │   (if ADMIN)    │  │
         └────────┬────────┘  │
                  │           │
                  └───────────┘
```

### 4.2 Máquina de Estados del Servidor

```
                    ┌─────────────────┐
                    │  INITIALIZING   │
                    └────────┬────────┘
                             │ socket(), bind(), listen()
                             ▼
                    ┌─────────────────┐
                    │   LISTENING     │◄─┐
                    └────────┬────────┘  │
                             │           │
                             ▼           │
                    ┌─────────────────┐  │
                    │ Accept Client   │  │
                    └────────┬────────┘  │
                             │           │
                             ▼           │
                    ┌─────────────────┐  │
                    │ Spawn Thread    │  │
                    └────────┬────────┘  │
                             │           │
                             └───────────┘
                             
            ┌─ Thread per Client ─┐    ┌─ Telemetry Thread ──┐
            │                     │    │                     │
            │ ┌─────────────────┐ │    │ ┌─────────────────┐ │
            │ │ Handle Client   │ │    │ │ Every 10 sec:   │ │
            │ │                 │ │    │ │                 │ │
            │ │ • Process AUTH  │ │    │ │ • Update state  │ │
            │ │ • Process CMD   │ │    │ │ • Send TELEM    │ │
            │ │ • Send RESP     │ │    │ │ • Check battery │ │
            │ │ • Detect disc.  │ │    │ │ • Update temp   │ │
            │ └─────────────────┘ │    │ └─────────────────┘ │
            └─────────────────────┘    └─────────────────────┘
```

### 4.3 Flujo de Establecimiento de Conexión

```
Cliente                           Servidor
   │                                 │
   │ 1. socket(AF_INET, SOCK_STREAM) │
   │ 2. connect(server_addr)         │
   ├─────────── TCP SYN ─────────────┤
   │                                 │ 3. accept()
   │◄────────── TCP ACK ─────────────┤
   │                                 │
   │ 4. send(AUTH message)           │
   ├─────── AUTH|type:user:pass ─────┤
   │                                 │ 5. validate_credentials()
   │                                 │ 6. add_client() if valid
   │◄────── RESP|OK:Authenticated ───┤
   │                                 │
   │ [Cliente registrado para TELEM] │
   │                                 │
   │◄────── TELEM|data (cada 10s) ───┤
   │                                 │
```

### 4.4 Flujo de Procesamiento de Comandos

```
Cliente ADMIN                     Servidor
   │                                 │
   │ send(CMD message)               │
   ├────────── CMD|SPEED UP ─────────┤
   │                                 │ 1. verify_admin_permissions()
   │                                 │ 2. parse_command()
   │                                 │ 3. validate_vehicle_state()
   │                                 │    ├─ check_battery_level()
   │                                 │    ├─ check_speed_limits()
   │                                 │    └─ check_movement_constraints()
   │                                 │ 4. execute_command()
   │                                 │    ├─ update_vehicle_state()
   │                                 │    └─ log_action()
   │◄───── RESP|OK:Speed increased ──┤ 5. send_response()
   │                                 │
   │ [Próximo ciclo de telemetría reflejará cambio]
   │◄────── TELEM|new_speed:... ─────┤
   │                                 │
```

### 4.5 Flujo de Consulta de Usuarios

```
Cliente ADMIN                     Servidor
   │                                 │
   │ send(LIST USERS command)        │
   ├───────── CMD|LIST USERS ────────┤
   │                                 │ 1. verify_admin_permissions()
   │                                 │ 2. pthread_mutex_lock(clients_mutex)
   │                                 │ 3. iterate_client_list()
   │                                 │    ├─ count_active_clients()
   │                                 │    └─ collect_client_info()
   │                                 │ 4. pthread_mutex_unlock(clients_mutex)
   │                                 │ 5. format_user_list()
   │◄─── LIST|2:admin1-ADMIN... ─────┤ 6. send_list_response()
   │                                 │
```

### 4.6 Manejo de Desconexiones y Errores

```
                     ┌──────────────────┐
                     │ Client Operation │
                     └────────┬─────────┘
                              │
                    ┌─────────▼─────────┐
                    │ recv() / send()   │
                    └────────┬──────────┘
                             │
                ┌────────────▼────────────┐
                │ Return Value Check      │
                └────┬──────────────┬─────┘
                     │              │
                ▼ == 0           ▼ < 0
        ┌─────────────────┐  ┌─────────────────┐
        │ Clean Disconnect│  │ Error / Timeout │
        └────────┬────────┘  └────────┬────────┘
                 │                    │
                 ▼                    ▼
        ┌─────────────────────────────────────┐
        │        Cleanup Procedure:           │
        │ 1. log_message(DISC)                │
        │ 2. remove_client(client_list)       │
        │ 3. close(socket_fd)                 │
        │ 4. free(client_memory)              │
        │ 5. pthread_exit()                   │
        └─────────────────────────────────────┘
```

---

## 5. Ejemplos de Implementación y Diagramas de Secuencia

### 5.1 Secuencia Completa: Autenticación + Comando + Telemetría

```
Cliente(ADMIN)    Servidor         Otros Clientes
     │               │                    │
     │ Conectar TCP  │                    │
     ├──────────────►│                    │
     │               │                    │
     │ AUTH|ADMIN:.. │                    │
     ├──────────────►│                    │
     │               │ validate()         │
     │ RESP|OK:...   │                    │
     │◄──────────────┤                    │
     │               │                    │
     │               │ ── TELEM ─────────►│ (broadcast)
     │◄── TELEM ─────┤                    │
     │               │                    │
     │ CMD|SPEED UP  │                    │
     ├──────────────►│                    │
     │               │ check_battery()    │
     │               │ update_speed()     │
     │ RESP|OK:...   │                    │
     │◄──────────────┤                    │
     │               │                    │
     │               │ ── TELEM(nuevo) ──►│ (broadcast refleja cambio)
     │◄── TELEM ─────┤                    │
     │               │                    │
```

### 5.2 Secuencia de Error: Comando con Restricciones

```
Cliente(ADMIN)         Servidor
     │                    │
     │ CMD|SPEED UP       │
     ├───────────────────►│
     │                    │ check_battery_level()
     │                    │ battery = 5% < 10%
     │                    │
     │ RESP|DENIED:...    │
     │◄───────────────────┤
     │                    │
     │ [Sin cambio de estado del vehículo]
     │                    │
```

### 5.3 Secuencia de Múltiples Clientes

```
Cliente_Admin    Cliente_Observer    Servidor
     │               │                │
     │ AUTH|ADMIN:.. │                │
     ├───────────────────────────────►│
     │               │ AUTH|OBSERVER..│
     │               ├───────────────►│
     │ RESP|OK       │                │
     │◄───────────────────────────────┤
     │               │ RESP|OK        │
     │               │◄───────────────┤
     │               │                │
     │◄── TELEM ─────┼────────────────┤ (broadcast a todos)
     │               │◄───────────────┤
     │               │                │
     │ CMD|TURN LEFT │                │
     ├───────────────────────────────►│
     │               │                │ execute_turn()
     │ RESP|OK:..    │                │
     │◄───────────────────────────────┤
     │               │                │
     │◄── TELEM ─────┼────────────────┤ (broadcast con nueva dirección)
     │   (nueva_dir) │◄───────────────┤
     │               │                │
     │ CMD|LIST USERS│                │
     ├───────────────────────────────►│
     │               │                │ build_user_list()
     │ LIST|2:admin..│                │
     │◄───────────────────────────────┤
     │               │                │
```

### 5.4 Implementación de Referencia: Fragmentos de Código

#### 5.4.1 Servidor - Autenticación

```c
int authenticate_user(char* message, client_t* client) {
    if (strncmp(message, "AUTH|", 5) != 0) {
        return -1;
    }
    
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char type_str[10];
    user_type_t type;
    
    // Parsear: AUTH|ADMIN:username:password\n
    char* data = message + 5;  // Saltar "AUTH|"
    
    if (sscanf(data, "%9[^:]:%31[^:]:%31[^\n]", type_str, username, password) != 3) {
        return -1;
    }
    
    // Determinar tipo
    if (strcmp(type_str, "ADMIN") == 0) {
        type = USER_ADMIN;
    } else if (strcmp(type_str, "OBSERVER") == 0) {
        type = USER_OBSERVER;
    } else {
        return -1;
    }
    
    // Validar contra lista hardcoded
    for (int i = 0; i < NUM_VALID_USERS; i++) {
        if (strcmp(VALID_USERS[i].username, username) == 0 && 
            strcmp(VALID_USERS[i].password, password) == 0 && 
            VALID_USERS[i].type == type) {

            // Autenticación exitosa
            strcpy(client->username, username);
            client->type = type;
            return 0;
        }
    }
    
    return -1;  // Credenciales inválidas
}
```

#### 5.4.2 Servidor - Telemetría

```c
void* telemetry_sender(void* arg) {
    server_t* server = (server_t*)arg;
    
    while (server->running) {
        sleep(TELEMETRY_INTERVAL); // 10 segundos
        
        // Actualizar estado del vehículo
        pthread_mutex_lock(&server->vehicle_mutex);
        
        // Consumir batería basado en velocidad
        if (server->vehicle.speed > 0) {
            float battery_consumption = server->vehicle.speed * 0.02;
            server->vehicle.battery -= (int)battery_consumption;
            if (server->vehicle.battery < 0) server->vehicle.battery = 0;
        }

        if (server->vehicle.battery == 0) {
            server->vehicle.speed = 0.0; // Detener vehículo si no hay batería
        }
        
        // Variar temperatura ligeramente
        server->vehicle.temperature += ((rand() % 21) - 10) * 0.1;
        if (server->vehicle.temperature < -10.0) server->vehicle.temperature = -10.0;
        if (server->vehicle.temperature > 50.0) server->vehicle.temperature = 50.0;
        
        // Construir mensaje de telemetría
        char telem_msg[256];
        snprintf(telem_msg, sizeof(telem_msg),
                "TELEM|%.1f:%d:%.1f:%s\n",
                server->vehicle.speed,
                server->vehicle.battery,
                server->vehicle.temperature,
                server->vehicle.direction);
        pthread_mutex_unlock(&server->vehicle_mutex);
        
        // Enviar a todos los clientes conectados
        pthread_mutex_lock(&server->clients_mutex);
        client_t* curr = server->clients_head;
        while (curr != NULL) {
            if (curr->active) {
                int sent = send(curr->socket_fd, telem_msg, strlen(telem_msg), MSG_NOSIGNAL);
                if (sent < 0) {
                    curr->active = 0; // Marcar como inactivo si falla envío
                }
            }
            curr = curr->next;
        }
        pthread_mutex_unlock(&server->clients_mutex);
    }
    
    return NULL;
}
```

#### 5.4.3 Cliente Python - Recepción de Telemetría

```python
def receive_messages(self):
    """Thread que escucha mensajes del servidor"""
    buffer = ""
    
    while self.is_connected:
        try:
            data = self.socket.recv(1024).decode()
            if not data:
                break
                
            buffer += data
            
            # Procesar mensajes completos (terminados en \n)
            while '\n' in buffer:
                message, buffer = buffer.split('\n', 1)
                self.process_message(message)
                
        except Exception as e:
            print(f"Error receiving: {e}")
            break
    
    self.is_connected = False

def parse_telemetry(self, message):
    """Parsea mensaje de telemetría: TELEM|speed:battery:temp:direction"""
    try:
        data = message.split('|')[1]
        parts = data.split(':')
        
        self.telemetry_data['speed'] = float(parts[0])
        self.telemetry_data['battery'] = int(parts[1])
        self.telemetry_data['temperature'] = float(parts[2])
        self.telemetry_data['direction'] = parts[3]
        
        # Actualizar GUI si existe
        if self.gui:
            self.gui.update_telemetry(self.telemetry_data)
            
    except Exception as e:
        print(f"Error parsing telemetry: {e}")
```

---

## 6. Aspectos de Implementación y Configuración

### 6.1 Consideraciones de Concurrencia

#### 6.1.1 Modelo de Threading

```c
// Estructura del servidor con sincronización
typedef struct {
    int tcp_socket;
    int port;
    char* log_file;
    client_t* clients_head;
    vehicle_state_t vehicle;
    pthread_mutex_t clients_mutex;  // Protege lista de clientes
    pthread_mutex_t vehicle_mutex;  // Protege estado del vehículo
    int running;
} server_t;
```

**Estrategia de Hilos:**
- **Hilo principal:** Acepta conexiones y crea hilos de cliente
- **Hilos de cliente:** Uno por cada conexión TCP activa
- **Hilo de telemetría:** Único, broadcast cada 10 segundos
- **Sincronización:** Mutexes para evitar condiciones de carrera

#### 6.1.2 Gestión de Recursos

```c
// Cleanup al desconectar cliente
void cleanup_client(client_t* client) {
    if (client->active) {
        log_message(&server, client, "DISC", "Client disconnected");
    }
    
    remove_client(&server, client);
    close(client->socket_fd);
    free(client);
}
```

### 6.2 Sistema de Logging Estructurado

#### 6.2.1 Formato de Log

```
[TIMESTAMP] [IP:PORT] [USERNAME] [TYPE] [MESSAGE]
```

#### 6.2.2 Tipos de Eventos

| Tipo | Descripción | Ejemplo |
|------|-------------|---------|
| CONN | Conexión establecida | `[2025-10-04 10:30:15] [127.0.0.1:45678] [UNKNOWN] [CONN] New connection established` |
| AUTH | Resultado de autenticación | `[2025-10-04 10:30:16] [127.0.0.1:45678] [admin1] [AUTH] Authentication successful` |
| REQ | Comando recibido | `[2025-10-04 10:30:17] [127.0.0.1:45678] [admin1] [REQ] CMD\|SPEED UP` |
| RESP | Respuesta enviada | `[2025-10-04 10:30:18] [127.0.0.1:45678] [admin1] [RESP] OK:Speed increased to 10.0 km/h` |
| DISC | Desconexión | `[2025-10-04 10:30:30] [127.0.0.1:45678] [admin1] [DISC] Client disconnected` |

### 6.3 Manejo de Errores y Excepciones

#### 6.3.1 Códigos de Error del Protocolo

| Código | Categoría | Acción del Servidor |
|--------|-----------|-------------------|
| `RESP\|ERROR:Authentication failed` | AUTH_FAIL | Cerrar conexión |
| `RESP\|ERROR:Permission denied` | PERM_DENIED | Mantener conexión, rechazar comando |
| `RESP\|ERROR:Invalid command format` | CMD_INVALID | Mantener conexión, rechazar comando |
| `RESP\|DENIED:Low battery - cannot speed up` | STATE_CONFLICT | Mantener conexión, comando no ejecutado |
| `RESP\|DENIED:Maximum speed reached` | LIMIT_REACHED | Mantener conexión, comando no ejecutado |

#### 6.3.2 Recuperación de Errores

```c
// Ejemplo de manejo robusto de envío
int safe_send(int socket_fd, const char* message, client_t* client) {
    int result = send(socket_fd, message, strlen(message), MSG_NOSIGNAL);
    if (result < 0) {
        log_message(&server, client, "DISC", "Send failed - client disconnected");
        client->active = 0;
        return -1;
    }
    return result;
}
```

### 6.4 Configuración y Parámetros

#### 6.4.1 Parámetros de Compilación

```c
#define MAX_USERNAME 32
#define MAX_PASSWORD 32
#define TELEMETRY_INTERVAL 10  // segundos
```

#### 6.4.2 Parámetros de Ejecución

```bash
./server <port> <LogsFile>
```

**Donde:**
- `port`: Puerto TCP de escucha (recomendado: 8080-9999)
- `LogsFile`: Archivo de logging (ejemplo: vehicle_logs.txt)

---

## 7. Consideraciones de Seguridad

### 7.1 Autenticación y Autorización

#### 7.1.1 Modelo de Seguridad

- **Autenticación:** Basada en credenciales hardcoded
- **Autorización:** Control de acceso por tipo de usuario
- **Sesión:** Una autenticación por conexión TCP
- **Timeout:** Implícito via timeout de TCP

#### 7.1.2 Limitaciones de Seguridad

⚠️ **ADVERTENCIAS DE SEGURIDAD:**
- Credenciales transmitidas en texto plano
- No hay cifrado de comunicaciones
- Autenticación no renovable durante la sesión
- No hay protección contra ataques de fuerza bruta

### 7.2 Validación de Entrada

```c
// Ejemplo de validación estricta
int validate_command_format(char* message) {
    if (strncmp(message, "CMD|", 4) != 0) {
        return -1; // Formato inválido
    }
    
    char command[32];
    if (sscanf(message, "CMD|%31[^\n]", command) != 1) {
        return -1; // Error de parsing
    }
    
    // Validar comandos conocidos
    if (strcmp(command, "SPEED UP") != 0 &&
        strcmp(command, "SLOW DOWN") != 0 &&
        strcmp(command, "TURN LEFT") != 0 &&
        strcmp(command, "TURN RIGHT") != 0 &&
        strcmp(command, "LIST USERS") != 0) {
        return -1; // Comando desconocido
    }
    
    return 0;
}
```

---

## 8. Conformidad y Interoperabilidad

### 8.1 Requisitos de Conformidad

Una implementación conforme a VATP DEBE:

1. **Soportar todos los tipos de mensaje** definidos en la sección 3.2
2. **Implementar autenticación completa** según flujo de la sección 4.3
3. **Manejar desconexiones gracefully** según procedimiento 4.6
4. **Proporcionar logging estructurado** con formato de la sección 6.2
5. **Usar TCP como transporte** exclusivamente
6. **Respetar restricciones de comandos** definidas en sección 2.3

### 8.2 Niveles de Conformidad

#### 8.2.1 Conformidad Básica
- Implementa todos los tipos de mensaje
- Autenticación funcional
- Telemetría broadcast cada 10 segundos

#### 8.2.2 Conformidad Completa
- Conformidad básica +
- Logging completo
- Manejo robusto de errores
- Concurrencia sin race conditions

### 8.3 Testing de Interoperabilidad

#### 8.3.1 Casos de Test Mínimos

1. **Test de Autenticación:**
   - Credenciales válidas (ADMIN y OBSERVER)
   - Credenciales inválidas
   - Formato incorrecto de AUTH

2. **Test de Telemetría:**
   - Recepción automática cada 10s
   - Múltiples clientes reciben simultáneamente
   - Formato correcto de TELEM

3. **Test de Comandos:**
   - Todos los comandos con ADMIN
   - Rechazo de comandos con OBSERVER
   - Restricciones de estado del vehículo

4. **Test de Concurrencia:**
   - Múltiples clientes simultáneos
   - Comandos concurrentes
   - Desconexiones durante operación

---

## 9. Extensibilidad y Evolución del Protocolo

### 9.1 Compatibilidad hacia Adelante

El protocolo permite extensiones futuras manteniendo compatibilidad:

#### 9.1.1 Nuevos Tipos de Mensaje
```
FUTURE_TYPE|new_data_format\n
```

#### 9.1.2 Nuevos Comandos
```
CMD|NEW_COMMAND_NAME\n
```

#### 9.1.3 Campos Adicionales de Telemetría
```
TELEM|speed:battery:temp:direction:new_field1:new_field2\n
```

### 9.2 Versionado del Protocolo

- **Versión actual:** VATP
- **Identificación:** No hay field de versión en mensajes actuales
- **Evolución futura:** Posible adición de campo VERSION en AUTH

---

## 10. Referencias

1. **RFC 793** - Transmission Control Protocol Specification
2. **RFC 2616** - Hypertext Transfer Protocol HTTP/1.1  
3. **POSIX.1-2017** - Portable Operating System Interface
4. **Berkeley Sockets API Documentation** - UNIX Network Programming
5. **Stevens, W. Richard** - TCP/IP Illustrated, Volume 1

---

## Apéndices

### Apéndice A: Códigos de Estado Completos

| Código | Tipo | Descripción | Acción del Cliente |
|--------|------|-------------|-------------------|
| `OK` | Success | Operación completada exitosamente | Continuar |
| `ERROR` | System Error | Error interno del servidor o formato | Revisar formato |
| `DENIED` | Business Logic | Operación válida pero rechazada por estado | Reintentar más tarde |

### Apéndice B: Matriz de Transiciones de Estado

| Estado Actual | Evento | Estado Siguiente | Acción |
|---------------|--------|------------------|--------|
| DISCONNECTED | TCP Connect | CONNECTED | Abrir socket |
| CONNECTED | Send AUTH | AUTHENTICATING | Validar credenciales |
| AUTHENTICATING | RESP\|OK | AUTHENTICATED | Registrar cliente |
| AUTHENTICATING | RESP\|ERROR | DISCONNECTED | Cerrar conexión |
| AUTHENTICATED | Receive TELEM | AUTHENTICATED | Actualizar GUI |
| AUTHENTICATED | Send CMD | AUTHENTICATED | Procesar comando |

### Apéndice C: Configuración de Usuarios por Defecto

```c
// Usuarios hardcoded en servidor
valid_user_t VALID_USERS[] = {
    {"admin1", "adminpass", USER_ADMIN},
    {"admin2", "admin123", USER_ADMIN},
    {"observer1", "obspass", USER_OBSERVER},
    {"observer2", "obs123", USER_OBSERVER}
};
#define NUM_VALID_USERS 4
```

### Apéndice D: Puertos y Configuración de Red

- **Puerto por defecto:** Configurable (recomendado: 8080)
- **Protocolo de transporte:** TCP (SOCK_STREAM)
- **Familia de direcciones:** AF_INET (IPv4)
- **Dirección de bind:** INADDR_ANY (0.0.0.0)
- **Backlog de listen:** 10 conexiones pendientes

---

**Fin del Documento RFC VATP**

*Este documento especifica completamente el Protocolo de Telemetría de Vehículo Autónomo, incluyendo todas las primitivas de servicio, formatos de mensaje, reglas de procedimiento, máquinas de estado, ejemplos de implementación y diagramas de secuencia requeridos para una implementación conforme y interoperable.*