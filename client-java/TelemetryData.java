public class TelemetryData {
    private double speed;
    private int battery;
    private double temperature;
    private String direction;
    
    public TelemetryData() {
        this.speed = 0.0;
        this.battery = 0;
        this.temperature = 0.0;
        this.direction = "NORTH";
    }
    
    // Getters
    public double getSpeed() { return speed; }
    public int getBattery() { return battery; }
    public double getTemperature() { return temperature; }
    public String getDirection() { return direction; }
    
    // Setters
    public void setSpeed(double speed) { this.speed = speed; }
    public void setBattery(int battery) { this.battery = battery; }
    public void setTemperature(double temperature) { this.temperature = temperature; }
    public void setDirection(String direction) { this.direction = direction; }
}