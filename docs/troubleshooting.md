# Troubleshooting

Common issues and solutions for the ESP32 Genius Gateway.

## Quick Diagnostics

Before diving into specific issues, run through this quick diagnostic checklist:

### Power & Connectivity
- [ ] Gateway is powered (LED indicators active)
- [ ] WiFi connection established (check router's client list)
- [ ] Web interface accessible at gateway IP address
- [ ] System status shows all services running

### RF Communication  
- [ ] CC1101 radio module detected in system status
- [ ] Antenna properly connected to CC1101
- [ ] Packet visualizer shows RF activity
- [ ] At least one Genius device within range

### MQTT Integration (if configured)
- [ ] MQTT broker reachable from gateway network
- [ ] Credentials and connection settings correct
- [ ] MQTT connection status shows "Connected"
- [ ] Test messages successfully published

## Common Issues

### Gateway Won't Start / No WiFi Access Point

**Symptoms**
- No status LEDs active
- No "Genius Gateway" WiFi network appears
- Device appears completely unresponsive

**Possible Causes & Solutions**

!!! failure "Power Supply Issues"
    **Check**: Verify 5V power supply can deliver at least 500mA  
    **Solution**: Use quality USB-C cable and power adapter, avoid USB hubs

!!! failure "Firmware Not Loaded"  
    **Check**: Device may not have firmware flashed properly  
    **Solution**: Reflash firmware using ESP32 programming tools

!!! failure "Hardware Damage"
    **Check**: Visual inspection for damaged components  
    **Solution**: Check continuity of power rails, replace if necessary

### Can't Access Web Interface

**Symptoms**
- WiFi connection established but web interface unreachable
- Browser shows "connection timeout" or "refused" errors
- Can ping gateway but HTTP not responding

**Solutions**

=== "Network Issues"
    1. **Check IP Address**: Use router admin panel or network scanner
    2. **Verify Port**: Try `http://[ip]:80` explicitly  
    3. **Clear DNS Cache**: `ipconfig /flushdns` (Windows) or `sudo dscacheutil -flushcache` (macOS)
    4. **Try Different Browser**: Rule out browser-specific issues

=== "Gateway Issues"
    1. **Restart Gateway**: Power cycle to restart web server
    2. **Factory Reset**: Hold boot button during power-on for 10 seconds
    3. **Check System Status**: Use serial console to verify web server startup

### No Devices Discovered

**Symptoms**
- Gateway operational but no smoke detectors appear in device list
- Packet visualizer shows no RF activity
- Known devices not detected despite being nearby

**Troubleshooting Steps**

1. **Verify RF Setup**
   ```bash
   # Check CC1101 status in system logs
   # Should show "CC1101 set up successfully"
   ```

2. **Test RF Reception**
   - Enable packet visualizer in web interface
   - Trigger test on nearby smoke detector
   - Check if any RF packets are received

3. **Check Device Compatibility**
   - Ensure detectors have FM Basis Radio Module
   - Verify devices are Hekatron Genius Plus X series
   - Check device serial numbers and model information

**Solutions**

!!! tip "Improve RF Reception"
    - **Antenna**: Ensure 868 MHz antenna properly connected
    - **Range**: Move gateway closer to devices (within 10-20m initially)  
    - **Interference**: Check for 868 MHz interference sources
    - **Position**: Elevate gateway and ensure line-of-sight when possible

!!! tip "Force Device Activity"
    - **Test Button**: Press test button on smoke detector
    - **Alarm Test**: Carefully trigger test alarm (follow safety procedures)
    - **Battery Replacement**: Devices often transmit during battery changes

### MQTT Connection Failures

**Symptoms**
- Gateway shows "MQTT Disconnected" status
- Home Assistant devices not updating
- MQTT broker logs show connection attempts failing

**Common Solutions**

=== "Connection Issues"
    **Check Network Connectivity**
    ```bash
    # Test if MQTT broker is reachable
    ping [mqtt-broker-ip]
    telnet [mqtt-broker-ip] 1883
    ```
    
    **Verify Credentials**
    - Double-check username and password
    - Ensure MQTT user has publish/subscribe permissions
    - Test with MQTT client tool (mosquitto_pub/mosquitto_sub)

=== "Configuration Issues"
    **MQTT Settings**
    - Verify broker hostname/IP address
    - Check port (default 1883, SSL usually 8883)
    - Ensure client ID is unique if specified
    - Verify QoS levels are supported by broker

=== "Firewall/Network Issues"
    **Network Configuration**
    - Check firewall rules on MQTT broker
    - Verify network routing between gateway and broker
    - Test from another device on same network

### Performance Issues

**Symptoms**
- Web interface slow to load or respond
- Delayed alarm notifications
- High memory usage or frequent restarts

**Solutions**

!!! warning "Memory Management"
    **Reduce Device Count**: Maximum 50 devices recommended  
    **Clear Alarm History**: Delete old alarm records periodically  
    **Limit Packet Logging**: Disable packet visualizer when not needed

!!! warning "Network Optimization"
    **WiFi Signal**: Ensure strong WiFi signal (-70 dBm or better)  
    **Bandwidth**: Avoid saturating network connection with other traffic  
    **DNS**: Use fast, reliable DNS servers (8.8.8.8, 1.1.1.1)

## Advanced Diagnostics

### Serial Console Access

For detailed troubleshooting, connect to the serial console:

1. **Connect Serial**: USB-C connection provides serial interface
2. **Terminal Settings**: 115200 baud, 8N1, no flow control  
3. **Monitor Output**: Boot messages and runtime logs

**Useful Serial Commands**
```bash
# System information
ESP.getFreeHeap()
ESP.getChipModel()
WiFi.status()

# CC1101 status  
cc1101_get_status()
cc1101_get_rssi()

# Network diagnostics
ping [hostname]
nslookup [hostname]
```

### Log Analysis

The gateway provides detailed logging:

**Log Levels**
- `ERROR`: Critical issues requiring attention
- `WARN`: Potential problems or degraded functionality  
- `INFO`: Normal operational messages
- `DEBUG`: Detailed troubleshooting information
- `VERBOSE`: Maximum detail for development

**Important Log Tags**
- `GeniusGateway`: Main gateway service messages
- `CC1101Controller`: RF hardware communication  
- `GatewayDevices`: Device management and status
- `WSLogger`: WebSocket packet logging
- `WiFiService`: Network connectivity

### Hardware Testing

For suspected hardware issues:

**Power Supply Testing**
```bash
# Measure voltages at test points
# 3.3V rail should be stable ±5%
# Check for voltage drops under load
```

**RF Testing**  
```bash
# CC1101 register dump
# RSSI measurements
# Frequency calibration check
```

**Component Testing**
- Visual inspection for damaged components
- Continuity testing of critical connections
- Oscilloscope verification of clock signals

## Getting Help

### Before Requesting Support

1. **Gather Information**
   - Gateway firmware version
   - Hardware revision (if known)
   - Complete error messages or logs
   - Network configuration details
   - Device models and quantities

2. **Document Issue**
   - Detailed problem description
   - Steps to reproduce  
   - Expected vs actual behavior
   - Screenshots if applicable

3. **Test Basic Functionality**
   - Try with minimal configuration
   - Test with different devices/networks
   - Verify issue persistence across restarts

### Community Support

- **GitHub Issues**: [Report bugs and request features](https://github.com/hmbacher/genius-gw-svelte/issues)
- **Discussions**: [Community help and sharing](https://github.com/hmbacher/genius-gw-svelte/discussions)  
- **Documentation**: Check this documentation for updates and additional guides

### Professional Support

For commercial deployments or complex issues:

- Detailed system analysis and recommendations
- Custom configuration and integration assistance  
- Hardware modification and optimization services
- Training and implementation support

Contact through GitHub or project maintainer for professional support options.

## Preventive Maintenance

### Regular Checks

**Monthly**
- [ ] Verify all devices still detected and responsive
- [ ] Check MQTT connection stability and message delivery
- [ ] Review alarm history for patterns or issues
- [ ] Test web interface performance and accessibility

**Quarterly**  
- [ ] Update firmware if new versions available
- [ ] Check antenna connections and RF performance
- [ ] Review system logs for errors or warnings
- [ ] Backup configuration settings

**Annually**
- [ ] Complete system testing including alarm scenarios
- [ ] Review and update device configurations
- [ ] Check hardware for signs of wear or damage
- [ ] Update documentation and procedures

### Best Practices

**Installation**
- Use quality power supplies and cables
- Ensure adequate ventilation and temperature control
- Position for optimal RF reception and network connectivity
- Document installation details and configuration

**Operation**
- Monitor system status regularly
- Keep firmware updated
- Backup configurations before changes
- Maintain current documentation

**Maintenance**
- Schedule regular testing during low-risk periods
- Test alarm scenarios safely and according to procedures
- Keep spare hardware components for critical deployments
- Train operators on basic troubleshooting procedures