import javax.swing.*;
import java.awt.*;

public class TelemetryGUI extends JFrame {
    private VehicleClient client;
    private JLabel speedLabel;
    private JLabel batteryLabel;
    private JLabel temperatureLabel;
    private JLabel directionLabel;
    private JLabel statusLabel;
    
    public TelemetryGUI(VehicleClient client) {
        this.client = client;
        this.client.setGUI(this);
        
        setTitle("Vehicle Telemetry Monitor");
        setSize(400, 500);
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLayout(new BorderLayout());
        
        createGUI();
        setVisible(true);
    }
    
    private void createGUI() {
        // Panel de telemetría
        JPanel telemPanel = new JPanel(new GridLayout(4, 2, 10, 10));
        telemPanel.setBorder(BorderFactory.createTitledBorder("Telemetry Data"));
        
        telemPanel.add(new JLabel("Speed:", SwingConstants.RIGHT));
        speedLabel = new JLabel("0.0 km/h");
        speedLabel.setFont(new Font("Arial", Font.BOLD, 14));
        telemPanel.add(speedLabel);
        
        telemPanel.add(new JLabel("Battery:", SwingConstants.RIGHT));
        batteryLabel = new JLabel("0%");
        batteryLabel.setFont(new Font("Arial", Font.BOLD, 14));
        telemPanel.add(batteryLabel);
        
        telemPanel.add(new JLabel("Temperature:", SwingConstants.RIGHT));
        temperatureLabel = new JLabel("0.0°C");
        temperatureLabel.setFont(new Font("Arial", Font.BOLD, 14));
        telemPanel.add(temperatureLabel);
        
        telemPanel.add(new JLabel("Direction:", SwingConstants.RIGHT));
        directionLabel = new JLabel("NORTH");
        directionLabel.setFont(new Font("Arial", Font.BOLD, 14));
        telemPanel.add(directionLabel);
        
        add(telemPanel, BorderLayout.NORTH);
        
        // Panel de control (solo para admin)
        if (client.getUserType() == UserType.ADMIN) {
            JPanel controlPanel = new JPanel(new GridLayout(5, 1, 5, 5));
            controlPanel.setBorder(BorderFactory.createTitledBorder("Control Commands"));
            
            JButton speedUpBtn = new JButton("SPEED UP");
            speedUpBtn.addActionListener(e -> client.sendCommand("SPEED UP"));
            controlPanel.add(speedUpBtn);
            
            JButton slowDownBtn = new JButton("SLOW DOWN");
            slowDownBtn.addActionListener(e -> client.sendCommand("SLOW DOWN"));
            controlPanel.add(slowDownBtn);
            
            JButton turnLeftBtn = new JButton("TURN LEFT");
            turnLeftBtn.addActionListener(e -> client.sendCommand("TURN LEFT"));
            controlPanel.add(turnLeftBtn);
            
            JButton turnRightBtn = new JButton("TURN RIGHT");
            turnRightBtn.addActionListener(e -> client.sendCommand("TURN RIGHT"));
            controlPanel.add(turnRightBtn);
            
            JButton listUsersBtn = new JButton("LIST USERS");
            listUsersBtn.addActionListener(e -> client.sendCommand("LIST USERS"));
            controlPanel.add(listUsersBtn);
            
            add(controlPanel, BorderLayout.CENTER);
        }
        
        // Status bar
        statusLabel = new JLabel(String.format("Connected as %s (%s)", 
                                              client.getUsername(), 
                                              client.getUserType()));
        statusLabel.setBorder(BorderFactory.createEtchedBorder());
        add(statusLabel, BorderLayout.SOUTH);
    }
    
    public void updateTelemetry(TelemetryData data) {
        SwingUtilities.invokeLater(() -> {
            speedLabel.setText(String.format("%.1f km/h", data.getSpeed()));
            batteryLabel.setText(String.format("%d%%", data.getBattery()));
            temperatureLabel.setText(String.format("%.1f°C", data.getTemperature()));
            directionLabel.setText(data.getDirection());
        });
    }
    
    public void showMessage(String status, String message) {
        SwingUtilities.invokeLater(() -> {
            if (status.equals("OK")) {
                JOptionPane.showMessageDialog(this, message, "Success", 
                                            JOptionPane.INFORMATION_MESSAGE);
            } else if (status.equals("DENIED")) {
                JOptionPane.showMessageDialog(this, message, "Command Denied", 
                                            JOptionPane.WARNING_MESSAGE);
            } else {
                JOptionPane.showMessageDialog(this, message, "Error", 
                                            JOptionPane.ERROR_MESSAGE);
            }
        });
    }
    
    public void showUserList(String userList) {
        SwingUtilities.invokeLater(() -> {
            JOptionPane.showMessageDialog(this, userList, "Connected Users", 
                                        JOptionPane.INFORMATION_MESSAGE);
        });
    }
    
    public void showError(String message) {
        SwingUtilities.invokeLater(() -> {
            JOptionPane.showMessageDialog(this, message, "Error", 
                                        JOptionPane.ERROR_MESSAGE);
        });
    }
    
    public void onDisconnected() {
        SwingUtilities.invokeLater(() -> {
            statusLabel.setText("Disconnected from server");
            JOptionPane.showMessageDialog(this, "Lost connection to server", 
                                        "Connection Lost", 
                                        JOptionPane.ERROR_MESSAGE);
        });
    }
}
