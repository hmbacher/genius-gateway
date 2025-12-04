# General Setup

## Hardware Setup

Choose either the [experimental setup](../hardware.md#experimental-setup) to test the functions of Genius Gateway, or the [Genius Gateway PCB](../hardware.md#final-hardware) to put Genius Gateway into operation.

## Build and Install Software

### IDE und Plugins

The project is based on the [PlatformIO :material-open-in-new:](https://platformio.org/){ target=_blank } build system. Unfortunately, PlatformIO does not support [Espressif Arduino 3.x :material-open-in-new:](https://github.com/espressif/arduino-esp32){ target=_blank }. 

Fortunately, there is [piorduino :material-open-in-new:](https://github.com/pioarduino){ target=_blank }: a community-maintained fork of the PlatformIO IDE for Visual Studio Code that provides support for Espressif microcontrollers, especially the newer ESP32 boards and the latest Arduino Core for them.

1. Download and install [Microsoft VSCode :material-open-in-new:](https://code.visualstudio.com/){ target=_blank }.
2. Install [Pioarduino IDE (pioarduino) :material-open-in-new:](https://marketplace.visualstudio.com/items?itemName=pioarduino.pioarduino-ide&ssr=false#review-details){ target=_blank } extension from within VSCode.

### Clone GitHub Project

1. Clone the project to your local environment  
    ```bash
    git clone https://github.com/hmbacher/genius-gateway.git
    ```

2. Open the project in VSCode  
    ```bash
    cd genius-gateway
    vscode .
    ```

### Configuration

#### Target board 

Choose the board you are working with in `platformio.ini` file by (un)commenting the appropriate `default_envs` options:  
```ini
[platformio]
description = Genius Gateway - A gateway for Hekatron Genius Plus X smoke detectors
data_dir = data
extra_configs = 
	factory_settings.ini
	features.ini
    config.ini
;default_envs = esp32-s3-devkitc-1
;default_envs = esp32-s3-devkitc-1-n8r2
default_envs = seeed-xiao-esp32s3
```

!!! note "No PSRAM used"
    Since PSRAM is not necessary or not used by Genius Gateway, it is not necessary to create variants of `esp32-s3-devkitc-1` (e.g., `esp32-s3-devkitc-1-n8r2`).

#### SPI bus wiring

Depending on the technical setup, the SPI bus-related signal lines may need to be adjusted. This is done via the `config.ini` file:  
```ini
[config]
build_flags = 
  ; --- Espressif ESP32S3 DevKit-C 1 ---
  ; -D CONFIG_CSN_GPIO=45   ; blue
  ; -D CONFIG_MISO_GPIO=11  ; yellow
  ; -D CONFIG_GDO0_GPIO=47  ; gray
  ; -D CONFIG_MOSI_GPIO=13  ; brown
  ; -D CONFIG_SCK_GPIO=12   ; white
  ; -D GPIO_TEST1=21
  ; -D GPIO_TEST2=14
  ; --- XIAO ESP32S3 & Genius Gateway PCB 1.0 ---
  -D CONFIG_CSN_GPIO=5
  -D CONFIG_MISO_GPIO=8
  -D CONFIG_GDO0_GPIO=6
  -D CONFIG_MOSI_GPIO=9
  -D CONFIG_SCK_GPIO=7
  -D GPIO_TEST1=1
  -D GPIO_TEST2=2
  -D GPIO_TEST3=3
  -D GPIO_TEST4=4
  ; Common
  -D HOST_ID=1
```

#### More build options

The platformio.ini file is the main file for controlling the software build. It references three additional files whose content is evaluated by the build system:
- `features.ini`
- `factory-settings.ini`
- `config.ini`

!!! critical
    Settings can be made in the listed files that affect the build and the later functionality of the Genius Gateway.

    For regular functionality, no adjustments beyond those described above need to be made.

### Build

#### Build Software

Start the *Build* task for the corresponding project environment (target device):  

![Execute Build](../assets/images/doc/vscode-tasks-build.png "Executing pioarduino Build in VSCode"){ .off-glb }

### Flash Software

Make sure you have properly connected the board to your computer via USB cable. A serial device must appear in the systems devices.

Start the *Upload and Monitor* Task and pick the right serial interface if asked:  

![Execute Upload and Monitor](../assets/images/doc/vscode-tasks-upload-monitor.png "Executing pioarduino Upload and Monitor in VSCode"){ .off-glb }

After the device has been successfully flashed, it will restart and you will see its serial output in VSCode's terminal window.

## Initial Configuration of Genius Gateway

After the first startup, the device has the settings stored in [factory_settings.ini :material-open-in-new:](https://github.com/hmbacher/genius-gateway/blob/main/factory_settings.ini){ target=_blank }.

The device creates its own WiFi network with the SSID `Genius-Gateway-XXX` and the password `genius-gateway`. The Genius Gateway web interface can be accessed via the IP address [192.168.4.1 :material-open-in-new:](http://192.168.4.1).

The default password for the `admin` account is `admin`.

### WiFi Settings

Genius Gateway can be [integrated into an existing WiFi network](wifi.md).

### Users

By default, two users are created: an `admin` account with elevated privileges and a `guest` account with limited privileges.

Additional users can be created with their own username and password. See the [Users](users.md#adding-a-user) page for details on managing user accounts.

### Change JWT Secret

For security reasons, it is strongly recommended to change the default JWT (JSON Web Token) secret after initial setup. The JWT secret is used to sign authentication tokens and should be unique for your gateway.

To change the JWT token, navigate to the [Security Settings](users.md#security-settings) section on the Users page and enter a new JWT secret value. Note that changing the JWT secret will sign out all users immediately.

!!! tip "How to get a secure JWT Secret"
    You can generate a cryptographically strong JWT secret using an online tool such as [RandomKeygen :material-open-in-new:](https://randomkeygen.com/){ target=_blank }. Use a Fort Knox Password (256-bit or higher) for maximum security.

### MQTT

For integration into a smart home system, the local MQTT broker must be configured. See the [MQTT Settings](../features/gateway-settings.md) page for details on configuring the MQTT connection.
