import java.io.*;
import java.net.*;

public class VehicleClient {
    private Socket socket;
    private BufferedReader in;
    private PrintWriter out;
    private String host;
    private int port;
    private String username;
    private UserType userType;
    private boolean isConnected;
    private TelemetryGUI gui;
    
    private TelemetryData currentTelemetry;
    
    public VehicleClient(String host, int port) {
        this.host = host;
        this.port = port;
        this.isConnected = false;
        this.currentTelemetry = new TelemetryData();
    }
    
    public boolean connect(String username, String password, UserType type) {
        try {
            socket = new Socket(host, port);
            in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            out = new PrintWriter(socket.getOutputStream(), true);
            
            // Enviar autenticación
            String authMsg = String.format("AUTH|%s:%s:%s\n", 
                                          type.toString(), username, password);
            out.print(authMsg);
            out.flush();
            
            // Recibir respuesta
            String response = in.readLine();
            
            if (response.startsWith("RESP|OK")) {
                this.username = username;
                this.userType = type;
                this.isConnected = true;
                
                // Iniciar thread de recepción
                new Thread(this::receiveMessages).start();
                
                return true;
            } else {
                socket.close();
                return false;
            }
            
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }
    }
    
    public void sendCommand(String command) {
        if (!isConnected) return;
        
        if (userType != UserType.ADMIN) {
            gui.showError("Only administrators can send commands");
            return;
        }
        
        String cmdMsg = String.format("CMD|%s\n", command);
        out.print(cmdMsg);
        out.flush();
    }
    
    private void receiveMessages() {
        try {
            String message;
            while (isConnected && (message = in.readLine()) != null) {
                processMessage(message);
            }
        } catch (IOException e) {
            isConnected = false;
            if (gui != null) {
                gui.onDisconnected();
            }
        }
    }
    
    private void processMessage(String message) {
        if (message.startsWith("TELEM|")) {
            parseTelemetry(message);
        } else if (message.startsWith("RESP|")) {
            parseResponse(message);
        } else if (message.startsWith("LIST|")) {
            parseUserList(message);
        }
    }
    
    private void parseTelemetry(String message) {
        try {
            String data = message.split("\\|")[1];
            String[] parts = data.split(":");
            
            currentTelemetry.setSpeed(Double.parseDouble(parts[0]));
            currentTelemetry.setBattery(Integer.parseInt(parts[1]));
            currentTelemetry.setTemperature(Double.parseDouble(parts[2]));
            currentTelemetry.setDirection(parts[3]);
            
            if (gui != null) {
                gui.updateTelemetry(currentTelemetry);
            }
            
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    
    private void parseResponse(String message) {
        try {
            String data = message.split("\\|")[1];
            String[] parts = data.split(":", 2);
            String status = parts[0];
            String msg = parts[1];
            
            if (gui != null) {
                gui.showMessage(status, msg);
            }
            
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    
    private void parseUserList(String message) {
        try {
            String data = message.split("\\|")[1];
            String[] parts = data.split(":");
            
            StringBuilder userList = new StringBuilder("Connected Users:\n\n");
            
            for (int i = 1; i < parts.length; i++) {
                String[] userInfo = parts[i].split("-");
                if (userInfo.length == 3) {
                    userList.append(String.format("%s (%s) - %s\n", 
                                                 userInfo[0], userInfo[1], userInfo[2]));
                }
            }
            
            if (gui != null) {
                gui.showUserList(userList.toString());
            }
            
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    
    public void disconnect() {
        isConnected = false;
        try {
            if (socket != null) socket.close();
            if (in != null) in.close();
            if (out != null) out.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
    
    public void setGUI(TelemetryGUI gui) {
        this.gui = gui;
    }
    
    public String getUsername() { return username; }
    public UserType getUserType() { return userType; }
    public boolean isConnected() { return isConnected; }
}
