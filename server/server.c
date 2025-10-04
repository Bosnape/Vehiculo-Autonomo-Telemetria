#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>

#define MAX_USERNAME 32
#define MAX_PASSWORD 32
#define TELEMETRY_INTERVAL 10

typedef enum {
    USER_ADMIN,
    USER_OBSERVER
} user_type_t;

typedef struct client {
    int socket_fd;
    char ip[INET_ADDRSTRLEN];
    int port;
    char username[MAX_USERNAME];
    user_type_t type;
    int active;  // 1 si está conectado, 0 si no
    struct client* next;
} client_t;

typedef struct {
    float speed;
    int battery;
    float temperature;
    char direction[16];
} vehicle_state_t;

typedef struct {
    int tcp_socket;
    int port;
    char* log_file;
    client_t* clients_head;
    vehicle_state_t vehicle;
    pthread_mutex_t clients_mutex;
    pthread_mutex_t vehicle_mutex;
    int running;
} server_t;

// Usuarios válidos hardcoded
typedef struct {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    user_type_t type;
} valid_user_t;

valid_user_t VALID_USERS[] = {
    {"admin1", "adminpass", USER_ADMIN},
    {"admin2", "admin123", USER_ADMIN},
    {"observer1", "obspass", USER_OBSERVER},
    {"observer2", "obs123", USER_OBSERVER}
};
#define NUM_VALID_USERS 4

// Variables globales
server_t server;

// Declaraciones de funciones
void* handle_client(void* arg);
void* telemetry_sender(void* arg);
int authenticate_user(char* message, client_t* client);
int process_command(char* message, client_t* client, server_t* server);
void add_client(server_t* server, client_t* client);
void remove_client(server_t* server, client_t* client);
void log_message(server_t* server, client_t* client, char* type, char* message);
void update_vehicle_direction(vehicle_state_t* vehicle, char* turn_direction);
void signal_handler(int sig);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <LogsFile>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // Configurar manejadores de señales
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Inicializar generador de números aleatorios
    srand(time(NULL));
    
    server.port = atoi(argv[1]);
    server.log_file = argv[2];
    server.clients_head = NULL;
    server.running = 1;
    
    // Inicializar estado del vehículo
    server.vehicle.speed = 0.0;
    server.vehicle.battery = 100;
    server.vehicle.temperature = 25.0;
    strcpy(server.vehicle.direction, "NORTH");
    
    // Inicializar mutexes
    pthread_mutex_init(&server.clients_mutex, NULL);
    pthread_mutex_init(&server.vehicle_mutex, NULL);
    
    // Crear socket TCP (SOCK_STREAM) con IPv4 (AF_INET)
    server.tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server.tcp_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Permitir reutilizar dirección
    int opt = 1;
    setsockopt(server.tcp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Configurar dirección
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server.port);
    
    // Bind
    if (bind(server.tcp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen
    if (listen(server.tcp_socket, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port %d\n", server.port);
    printf("Logging to file: %s\n", server.log_file);
    
    // Crear hilo de telemetría
    pthread_t telemetry_thread;
    pthread_create(&telemetry_thread, NULL, telemetry_sender, &server);
    
    // Loop principal: aceptar clientes
    while (server.running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_socket = accept(server.tcp_socket, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }
        
        // Crear estructura para el cliente
        client_t* new_client = malloc(sizeof(client_t));
        new_client->socket_fd = client_socket;
        inet_ntop(AF_INET, &client_addr.sin_addr, new_client->ip, INET_ADDRSTRLEN);
        new_client->port = ntohs(client_addr.sin_port);
        new_client->active = 0;  // No autenticado aún
        new_client->next = NULL;
        
        // Crear hilo para manejar este cliente
        pthread_t client_thread;
        pthread_create(&client_thread, NULL, handle_client, new_client);
        pthread_detach(client_thread);
    }
    
    // Cleanup
    close(server.tcp_socket);
    pthread_mutex_destroy(&server.clients_mutex);
    pthread_mutex_destroy(&server.vehicle_mutex);
    
    return 0;
}

void* handle_client(void* arg) {
    client_t* client = (client_t*)arg;
    char buffer[1024];
    
    printf("New connection from %s:%d\n", client->ip, client->port);
    
    // Log conexión inicial
    log_message(&server, client, "CONN", "New connection established");
    
    // Esperar autenticación
    int bytes = recv(client->socket_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        log_message(&server, client, "DISC", "Connection closed before authentication");
        close(client->socket_fd);
        free(client);
        return NULL;
    }
    
    buffer[bytes] = '\0';
    
    // Procesar autenticación
    if (authenticate_user(buffer, client) == 0) {
        client->active = 1;
        add_client(&server, client);
        
        char response[128];
        snprintf(response, sizeof(response), "RESP|OK:Authenticated as %s\n",
                 client->type == USER_ADMIN ? "ADMIN" : "OBSERVER");
        send(client->socket_fd, response, strlen(response), 0);
        log_message(&server, client, "AUTH", "Authentication successful");
        
        // Loop de recepción de comandos
        while (client->active) {
            bytes = recv(client->socket_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) {
                break;  // Cliente desconectado
            }
            
            buffer[bytes] = '\0';
            
            // Procesar comando
            if (strncmp(buffer, "CMD|", 4) == 0) {
                // Limpiar \n al final del buffer para el log
                char clean_buffer[1024];
                strcpy(clean_buffer, buffer);
                int len = strlen(clean_buffer);
                if (len > 0 && clean_buffer[len - 1] == '\n') {
                    clean_buffer[len - 1] = '\0';
                }
                log_message(&server, client, "REQ", clean_buffer);
                process_command(buffer, client, &server);
            } else {
                char error[] = "RESP|ERROR:Invalid message format\n";
                send(client->socket_fd, error, strlen(error), 0);
                log_message(&server, client, "RESP", "ERROR:Invalid message format");
            }
        }
    } else {
        char response[] = "RESP|ERROR:Authentication failed\n";
        send(client->socket_fd, response, strlen(response), 0);
        log_message(&server, client, "AUTH", "Authentication failed");
    }
    
    // Log desconexión antes del cleanup
    if (client->active) {
        log_message(&server, client, "DISC", "Client disconnected");
    }
    
    // Cleanup
    remove_client(&server, client);
    close(client->socket_fd);
    free(client);
    
    return NULL;
}

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

int process_command(char* message, client_t* client, server_t* server) {
    // Verificar permisos
    if (client->type != USER_ADMIN) {
        char response[] = "RESP|ERROR:Permission denied\n";
        send(client->socket_fd, response, strlen(response), 0);
        log_message(server, client, "RESP", "ERROR:Permission denied");
        return -1;
    }
    
    // Parsear comando: CMD|SPEED UP\n
    char command[32];
    if (sscanf(message, "CMD|%31[^\n]", command) != 1) {
        char response[] = "RESP|ERROR:Invalid command format\n";
        send(client->socket_fd, response, strlen(response), 0);
        log_message(server, client, "RESP", "ERROR:Invalid command format");
        return -1;
    }
    
    // Comando especial: LIST USERS
    if (strcmp(command, "LIST USERS") == 0) {
        char response[2048]; // Se vuelve largo y puede haber problemas si hay muchos usuarios
        int offset = snprintf(response, sizeof(response), "LIST|");
        
        pthread_mutex_lock(&server->clients_mutex);
        
        int count = 0;
        client_t* curr = server->clients_head;
        while (curr != NULL) {
            count++;
            curr = curr->next;
        }
        
        offset += snprintf(response + offset, sizeof(response) - offset, "%d", count);
        
        curr = server->clients_head;
        while (curr != NULL) {
            offset += snprintf(response + offset, sizeof(response) - offset,
                             ":%s-%s-%s:%d",
                             curr->username,
                             curr->type == USER_ADMIN ? "ADMIN" : "OBSERVER",
                             curr->ip,
                             curr->port);
            curr = curr->next;
        }
        
        strcat(response, "\n");
        send(client->socket_fd, response, strlen(response), 0);
        
        // Crear mensaje de log más legible
        char log_msg[2048];
        int log_offset = snprintf(log_msg, sizeof(log_msg), "%d users connected", count);
        
        curr = server->clients_head;
        while (curr != NULL) {
            log_offset += snprintf(log_msg + log_offset, sizeof(log_msg) - log_offset,
                                 " | %s (%s) at %s:%d",
                                 curr->username,
                                 curr->type == USER_ADMIN ? "ADMIN" : "OBSERVER",
                                 curr->ip,
                                 curr->port);
            curr = curr->next;
        }
        pthread_mutex_unlock(&server->clients_mutex);

        log_message(server, client, "RESP", log_msg);
        return 0;
    }
    
    // Comandos de movimiento
    pthread_mutex_lock(&server->vehicle_mutex);
    
    char response[128];
    
    if (strcmp(command, "SPEED UP") == 0) {
        if (server->vehicle.battery < 10) {
            snprintf(response, sizeof(response),
                    "RESP|DENIED:Low battery - cannot speed up\n");
        } else if (server->vehicle.speed >= 100.0) {
            snprintf(response, sizeof(response),
                    "RESP|DENIED:Maximum speed reached\n");
        } else {
            server->vehicle.speed += 10.0;
            server->vehicle.battery -= 2;  // Consumir batería
            if (server->vehicle.battery < 0) server->vehicle.battery = 0;
            snprintf(response, sizeof(response),
                    "RESP|OK:Speed increased to %.1f km/h\n",
                    server->vehicle.speed);
        }
    }
    else if (strcmp(command, "SLOW DOWN") == 0) {
        if (server->vehicle.speed <= 0.0) {
            snprintf(response, sizeof(response),
                    "RESP|DENIED:Vehicle already stopped\n");
        } else {
            server->vehicle.speed -= 10.0;
            if (server->vehicle.speed < 0.0) server->vehicle.speed = 0.0;
            snprintf(response, sizeof(response),
                    "RESP|OK:Speed decreased to %.1f km/h\n",
                    server->vehicle.speed);
        }
    }
    else if (strcmp(command, "TURN LEFT") == 0) {
        if (server->vehicle.battery <= 0) {
            snprintf(response, sizeof(response),
                    "RESP|DENIED:No battery - cannot turn\n");
        } else {
            update_vehicle_direction(&server->vehicle, "LEFT");
            snprintf(response, sizeof(response),
                    "RESP|OK:Turned left, now facing %s\n", 
                    server->vehicle.direction);
        }
    }
    else if (strcmp(command, "TURN RIGHT") == 0) {
        if (server->vehicle.battery <= 0) {
            snprintf(response, sizeof(response),
                    "RESP|DENIED:No battery - cannot turn\n");
        } else {
            update_vehicle_direction(&server->vehicle, "RIGHT");
            snprintf(response, sizeof(response),
                    "RESP|OK:Turned right, now facing %s\n", 
                    server->vehicle.direction);
        }
    }
    else {
        snprintf(response, sizeof(response),
                "RESP|ERROR:Unknown command\n");
    }
    
    pthread_mutex_unlock(&server->vehicle_mutex);
    
    send(client->socket_fd, response, strlen(response), 0);
    
    char log_msg[128];
    strcpy(log_msg, response + 5);
    log_msg[strlen(log_msg) - 1] = '\0';
    log_message(server, client, "RESP", log_msg);
    return 0;
}

void* telemetry_sender(void* arg) {
    server_t* server = (server_t*)arg;
    
    while (server->running) {
        sleep(TELEMETRY_INTERVAL); // 10 segundos
        
        // Actualizar estado del vehículo
        pthread_mutex_lock(&server->vehicle_mutex);
        
        // Consumir batería basado en velocidad
        if (server->vehicle.speed > 0) {
            float battery_consumption = server->vehicle.speed * 0.02; // Más velocidad = más consumo
            server->vehicle.battery -= (int)battery_consumption;
            if (server->vehicle.battery < 0) server->vehicle.battery = 0;
        }

        if (server->vehicle.battery == 0) {
            server->vehicle.speed = 0.0; // Detener vehículo si no hay batería
        }
        
        // Variar temperatura ligeramente
        server->vehicle.temperature += ((rand() % 21) - 10) * 0.1; // Variación de -1.0 a +1.0
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
            client_t* next = curr->next; // Guardar antes de operar
            if (curr->active) {
                int sent = send(curr->socket_fd, telem_msg, strlen(telem_msg), MSG_NOSIGNAL);
                if (sent < 0) {
                    // Cliente desconectado, marcar como inactivo
                    curr->active = 0;
                }
            }
            curr = next; // Usar la copia guardada
        }
        
        pthread_mutex_unlock(&server->clients_mutex);
        
        printf("Telemetry broadcast sent\n");
    }
    
    return NULL;
}

void log_message(server_t* server, client_t* client, char* type, char* message) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    
    char log_entry[512];
    snprintf(log_entry, sizeof(log_entry),
             "[%s] [%s:%d] [%s] [%s] %s\n",
             timestamp,
             client->ip,
             client->port,
             client->username[0] ? client->username : "UNKNOWN",
             type,
             message);
    
    // Imprimir en consola
    printf("%s", log_entry);
    
    // Escribir en archivo (logging síncrono)
    FILE* log_file = fopen(server->log_file, "a");
    if (log_file) {
        fprintf(log_file, "%s", log_entry);
        fclose(log_file);
    } else {
        perror("Failed to write to log file");
    }
}

void add_client(server_t* server, client_t* client) {
    pthread_mutex_lock(&server->clients_mutex);
    client->next = server->clients_head;
    server->clients_head = client;
    pthread_mutex_unlock(&server->clients_mutex);
}

void remove_client(server_t* server, client_t* client) {
    pthread_mutex_lock(&server->clients_mutex);
    client->active = 0;
    client_t** curr = &server->clients_head;
    while (*curr != NULL) {
        if (*curr == client) {
            *curr = client->next;
            break;
        }
        curr = &((*curr)->next);
    }
    pthread_mutex_unlock(&server->clients_mutex);
}

void update_vehicle_direction(vehicle_state_t* vehicle, char* turn_direction) {
    // Mapa de direcciones: N->0, E->1, S->2, W->3
    char* directions[] = {"NORTH", "EAST", "SOUTH", "WEST"};
    int current_dir = 0;
    
    // Encontrar dirección actual
    for (int i = 0; i < 4; i++) {
        if (strcmp(vehicle->direction, directions[i]) == 0) {
            current_dir = i;
            break;
        }
    }
    
    // Calcular nueva dirección
    if (strcmp(turn_direction, "LEFT") == 0) {
        current_dir = (current_dir + 3) % 4;  // Girar a la izquierda (-1, pero en módulo 4)
    } else if (strcmp(turn_direction, "RIGHT") == 0) {
        current_dir = (current_dir + 1) % 4;  // Girar a la derecha (+1)
    }
    
    // Actualizar dirección del vehículo
    strcpy(vehicle->direction, directions[current_dir]);
}

void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down server...\n", sig);
    server.running = 0;
    
    // Cerrar socket principal para interrumpir accept()
    if (server.tcp_socket > 0) {
        close(server.tcp_socket);
    }
}
