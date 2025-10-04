import javax.swing.*;
import java.awt.*;

public class LoginGUI extends JFrame {
    private JTextField hostField;
    private JTextField portField;
    private JTextField usernameField;
    private JPasswordField passwordField;
    private JRadioButton adminRadio;
    private JRadioButton observerRadio;
    
    public LoginGUI() {
        setTitle("Vehicle Client - Login");
        setSize(320, 300);
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLayout(new GridLayout(7, 2, 5, 5));
        
        add(new JLabel("Server Host:"));
        hostField = new JTextField("localhost");
        add(hostField);
        
        add(new JLabel("Server Port:"));
        portField = new JTextField("8080");
        add(portField);
        
        add(new JLabel("Username:"));
        usernameField = new JTextField();
        add(usernameField);
        
        add(new JLabel("Password:"));
        passwordField = new JPasswordField();
        add(passwordField);
        
        add(new JLabel("User Type:"));
        JPanel radioPanel = new JPanel();
        adminRadio = new JRadioButton("Admin");
        observerRadio = new JRadioButton("Observer", true);
        ButtonGroup group = new ButtonGroup();
        group.add(adminRadio);
        group.add(observerRadio);
        radioPanel.add(adminRadio);
        radioPanel.add(observerRadio);
        add(radioPanel);
        
        JButton connectBtn = new JButton("Connect");
        connectBtn.addActionListener(e -> connect());
        add(new JLabel());
        add(connectBtn);
        
        setVisible(true);
    }
    
    private void connect() {
        String host = hostField.getText().trim();
        String portStr = portField.getText().trim();
        String username = usernameField.getText().trim();
        String password = new String(passwordField.getPassword());
        UserType type = adminRadio.isSelected() ? UserType.ADMIN : UserType.OBSERVER;
        
        // Validaciones de entrada
        if (host.isEmpty()) {
            JOptionPane.showMessageDialog(this, "Server host cannot be empty", 
                                        "Error", JOptionPane.ERROR_MESSAGE);
            return;
        }
        
        if (portStr.isEmpty()) {
            JOptionPane.showMessageDialog(this, "Server port cannot be empty", 
                                        "Error", JOptionPane.ERROR_MESSAGE);
            return;
        }
        
        int port;
        try {
            port = Integer.parseInt(portStr);
            if (port < 1 || port > 65535) {
                JOptionPane.showMessageDialog(this, "Port must be between 1 and 65535", 
                                            "Error", JOptionPane.ERROR_MESSAGE);
                return;
            }
        } catch (NumberFormatException e) {
            JOptionPane.showMessageDialog(this, "Port must be a valid number", 
                                        "Error", JOptionPane.ERROR_MESSAGE);
            return;
        }
        
        if (username.isEmpty()) {
            JOptionPane.showMessageDialog(this, "Username cannot be empty", 
                                        "Error", JOptionPane.ERROR_MESSAGE);
            return;
        }
        
        if (password.isEmpty()) {
            JOptionPane.showMessageDialog(this, "Password cannot be empty", 
                                        "Error", JOptionPane.ERROR_MESSAGE);
            return;
        }
        
        VehicleClient client = new VehicleClient(host, port);
        
        if (client.connect(username, password, type)) {
            this.dispose();
            new TelemetryGUI(client);
        } else {
            JOptionPane.showMessageDialog(this, "Authentication failed", 
                                        "Error", JOptionPane.ERROR_MESSAGE);
        }
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(LoginGUI::new);
    }
}