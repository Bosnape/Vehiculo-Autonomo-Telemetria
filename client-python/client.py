import socket
import threading
import tkinter as tk
from tkinter import ttk, messagebox

class VehicleClient:
    def __init__(self, host, port):
        self.socket = None
        self.host = host
        self.port = port
        self.username = None
        self.user_type = None
        self.is_connected = False
        self.telemetry_data = {
            'speed': 0.0,
            'battery': 0,
            'temperature': 0.0,
            'direction': 'NORTH'
        }
        self.gui = None
        
    def connect(self, username, password, user_type):
        """Conecta al servidor y autentica"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.host, self.port))
            
            # Enviar autenticación
            auth_msg = f"AUTH|{user_type}:{username}:{password}\n"
            self.socket.send(auth_msg.encode())
            
            # Recibir respuesta (manejar múltiples chunks)
            response_buffer = ""
            while '\n' not in response_buffer:
                data = self.socket.recv(1024).decode()
                if not data:
                    break
                response_buffer += data
            
            response = response_buffer.split('\n')[0]  # Tomar primer mensaje completo
            
            if response.startswith("RESP|OK"):
                self.username = username
                self.user_type = user_type
                self.is_connected = True
                
                # Iniciar thread de recepción
                recv_thread = threading.Thread(target=self.receive_messages, daemon=True)
                recv_thread.start()
                
                return True
            else:
                self.socket.close()
                return False
                
        except Exception as e:
            print(f"Connection error: {e}")
            return False
    
    def send_command(self, command):
        """Envía comando (solo admins)"""
        if not self.is_connected:
            return False
            
        if self.user_type != "ADMIN":
            messagebox.showerror("Error", "Only administrators can send commands")
            return False
            
        try:
            cmd_msg = f"CMD|{command}\n"
            self.socket.send(cmd_msg.encode())
            return True
        except Exception as e:
            print(f"Error sending command: {e}")
            return False
    
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
        if self.gui:
            self.gui.on_disconnected()
    
    def process_message(self, message):
        """Procesa mensajes del servidor"""
        if message.startswith("TELEM|"):
            self.parse_telemetry(message)
        elif message.startswith("RESP|"):
            self.parse_response(message)
        elif message.startswith("LIST|"):
            self.parse_user_list(message)
    
    def parse_telemetry(self, message):
        """Parsea mensaje de telemetría: TELEM|speed:battery:temp:direction"""
        try:
            data = message.split('|')[1]
            parts = data.split(':')
            
            self.telemetry_data['speed'] = float(parts[0])
            self.telemetry_data['battery'] = int(parts[1])
            self.telemetry_data['temperature'] = float(parts[2])
            self.telemetry_data['direction'] = parts[3]
            
            if self.gui:
                self.gui.update_telemetry(self.telemetry_data)
                
        except Exception as e:
            print(f"Error parsing telemetry: {e}")
    
    def parse_response(self, message):
        """Parsea respuesta del servidor"""
        try:
            data = message.split('|')[1]
            status, msg = data.split(':', 1)
            
            if self.gui:
                if status == "OK":
                    self.gui.show_message("Success", msg, "info")
                elif status == "DENIED":
                    self.gui.show_message("Command Denied", msg, "warning")
                else:
                    self.gui.show_message("Error", msg, "error")
                    
        except Exception as e:
            print(f"Error parsing response: {e}")
    
    def parse_user_list(self, message):
        """Parsea lista de usuarios"""
        try:
            data = message.split('|')[1]
            parts = data.split(':')
            num_users = int(parts[0])
            
            users = []
            for i in range(1, len(parts)):
                user_info = parts[i].split('-')
                if len(user_info) == 3:
                    username = user_info[0]
                    user_type = user_info[1]
                    address = user_info[2]
                    users.append(f"{username} ({user_type}) - {address}")
            
            if self.gui:
                self.gui.show_user_list(users)
                
        except Exception as e:
            print(f"Error parsing user list: {e}")
    
    def disconnect(self):
        """Cierra conexión"""
        self.is_connected = False
        if self.socket:
            self.socket.close()


class TelemetryGUI:
    def __init__(self, client):
        self.client = client
        self.client.gui = self
        self.root = tk.Tk()
        self.root.title("Vehicle Telemetry Monitor")
        self.root.geometry("400x500")
        
        self.create_widgets()
        
    def create_widgets(self):
        """Crea interfaz gráfica"""
        # Frame de telemetría
        telem_frame = ttk.LabelFrame(self.root, text="Telemetry Data", padding=10)
        telem_frame.pack(fill="both", expand=True, padx=10, pady=5)
        
        # Speed
        ttk.Label(telem_frame, text="Speed:", font=("Arial", 10, "bold")).grid(row=0, column=0, sticky="w", pady=5)
        self.speed_label = ttk.Label(telem_frame, text="0.0 km/h", font=("Arial", 12))
        self.speed_label.grid(row=0, column=1, sticky="w", pady=5)
        
        # Battery
        ttk.Label(telem_frame, text="Battery:", font=("Arial", 10, "bold")).grid(row=1, column=0, sticky="w", pady=5)
        self.battery_label = ttk.Label(telem_frame, text="0%", font=("Arial", 12))
        self.battery_label.grid(row=1, column=1, sticky="w", pady=5)
        
        # Temperature
        ttk.Label(telem_frame, text="Temperature:", font=("Arial", 10, "bold")).grid(row=2, column=0, sticky="w", pady=5)
        self.temp_label = ttk.Label(telem_frame, text="0.0°C", font=("Arial", 12))
        self.temp_label.grid(row=2, column=1, sticky="w", pady=5)
        
        # Direction
        ttk.Label(telem_frame, text="Direction:", font=("Arial", 10, "bold")).grid(row=3, column=0, sticky="w", pady=5)
        self.direction_label = ttk.Label(telem_frame, text="NORTH", font=("Arial", 12))
        self.direction_label.grid(row=3, column=1, sticky="w", pady=5)
        
        # Frame de control (solo para admin)
        if self.client.user_type == "ADMIN":
            control_frame = ttk.LabelFrame(self.root, text="Control Commands", padding=10)
            control_frame.pack(fill="both", expand=True, padx=10, pady=5)
            
            ttk.Button(control_frame, text="SPEED UP", 
                      command=lambda: self.client.send_command("SPEED UP")).pack(fill="x", pady=2)
            ttk.Button(control_frame, text="SLOW DOWN",
                      command=lambda: self.client.send_command("SLOW DOWN")).pack(fill="x", pady=2)
            ttk.Button(control_frame, text="TURN LEFT",
                      command=lambda: self.client.send_command("TURN LEFT")).pack(fill="x", pady=2)
            ttk.Button(control_frame, text="TURN RIGHT",
                      command=lambda: self.client.send_command("TURN RIGHT")).pack(fill="x", pady=2)
            ttk.Button(control_frame, text="LIST USERS",
                      command=lambda: self.client.send_command("LIST USERS")).pack(fill="x", pady=2)
        
        # Status
        self.status_label = ttk.Label(self.root, text=f"Connected as {self.client.username} ({self.client.user_type})",
                                     relief="sunken")
        self.status_label.pack(side="bottom", fill="x")
        
    def update_telemetry(self, data):
        """Actualiza valores mostrados"""
        self.speed_label.config(text=f"{data['speed']:.1f} km/h")
        self.battery_label.config(text=f"{data['battery']}%")
        self.temp_label.config(text=f"{data['temperature']:.1f}°C")
        self.direction_label.config(text=data['direction'])
    
    def show_message(self, title, message, msg_type):
        """Muestra mensaje al usuario"""
        if msg_type == "info":
            messagebox.showinfo(title, message)
        elif msg_type == "warning":
            messagebox.showwarning(title, message)
        else:
            messagebox.showerror(title, message)
    
    def show_user_list(self, users):
        """Muestra lista de usuarios en ventana emergente"""
        list_window = tk.Toplevel(self.root)
        list_window.title("Connected Users")
        list_window.geometry("400x300")
        
        listbox = tk.Listbox(list_window)
        listbox.pack(fill="both", expand=True, padx=10, pady=10)
        
        for user in users:
            listbox.insert(tk.END, user)
    
    def on_disconnected(self):
        """Maneja desconexión"""
        self.status_label.config(text="Disconnected from server")
        messagebox.showerror("Connection Lost", "Lost connection to server")
    
    def run(self):
        """Inicia el loop de la GUI"""
        self.root.mainloop()


# Login window
class LoginWindow:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Vehicle Client - Login")
        self.root.geometry("300x400")
        
        ttk.Label(self.root, text="Server Host:").pack(pady=5)
        self.host_entry = ttk.Entry(self.root)
        self.host_entry.insert(0, "localhost")
        self.host_entry.pack(pady=5)
        
        ttk.Label(self.root, text="Server Port:").pack(pady=5)
        self.port_entry = ttk.Entry(self.root)
        self.port_entry.insert(0, "8080")
        self.port_entry.pack(pady=5)
        
        ttk.Label(self.root, text="Username:").pack(pady=5)
        self.username_entry = ttk.Entry(self.root)
        self.username_entry.pack(pady=5)
        
        ttk.Label(self.root, text="Password:").pack(pady=5)
        self.password_entry = ttk.Entry(self.root, show="*")
        self.password_entry.pack(pady=5)
        
        ttk.Label(self.root, text="User Type:").pack(pady=5)
        self.type_var = tk.StringVar(value="OBSERVER")
        ttk.Radiobutton(self.root, text="Admin", variable=self.type_var, value="ADMIN").pack()
        ttk.Radiobutton(self.root, text="Observer", variable=self.type_var, value="OBSERVER").pack()
        
        ttk.Button(self.root, text="Connect", command=self.connect).pack(pady=10)
        
    def connect(self):
        host = self.host_entry.get().strip()
        port_str = self.port_entry.get().strip()
        username = self.username_entry.get().strip()
        password = self.password_entry.get()
        user_type = self.type_var.get()
        
        # Validaciones de entrada
        if not host:
            messagebox.showerror("Error", "Server host cannot be empty")
            return
            
        if not port_str:
            messagebox.showerror("Error", "Server port cannot be empty")
            return
            
        try:
            port = int(port_str)
            if port < 1 or port > 65535:
                messagebox.showerror("Error", "Port must be between 1 and 65535")
                return
        except ValueError:
            messagebox.showerror("Error", "Port must be a valid number")
            return
            
        if not username:
            messagebox.showerror("Error", "Username cannot be empty")
            return
            
        if not password:
            messagebox.showerror("Error", "Password cannot be empty")
            return
        
        client = VehicleClient(host, port)
        
        if client.connect(username, password, user_type):
            self.root.destroy()
            gui = TelemetryGUI(client)
            gui.run()
        else:
            messagebox.showerror("Error", "Authentication failed")
    
    def run(self):
        self.root.mainloop()


if __name__ == "__main__":
    login = LoginWindow()
    login.run()
