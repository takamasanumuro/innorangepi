To build this, install cmake and libcurl on your system

sudo apt install cmake
sudo apt install libcurl4-openssl-dev

Then cd into build folder and run:
cmake .. && make ./instrumentation-app && sudo ./instrumentation-app [I2C_ADDRESS] [CONFIG_FILE.txt] [I2C-BUS]

Choose the config file based on the board being used.

Examples:

cmake .. && make ./instrumentation-app && sudo ./instrumentation-app 0x48 configA.txt /dev/i2c-1
cmake .. && make ./instrumentation-app && sudo ./instrumentation-app 0x49 configB.txt /dev/i2c-3


Troubleshooting:

1) Can't find anything using sudo i2cdetect -y [I2C-BUS-NUMBER]

    Make sure to enable the I2C overlays  on /boot/orangePiEnv.txt and /boot/firmware/ubuntuEnv.txt

2) Devices still not showing up, even with the overlays enabled.

    Try using a different overlay, check the physical connections between the board and the microcomputer,
    make sure they share a common GND. Try switching to another ADS1115 to make sure it is not a faulty one.
    Check continuity on the board for GND, SCL, SDA.

3) Measurements stuck at 0 or 32768. 
    Check current sensor orientation. Positive current should flow in direction of arrow.
    See if output voltage at measurement pin is higher than the gain that was set (saturation condition).

4) Weird values showing up.
    
    Make sure sensors are connected correctly before initializing the program.
    Make sure you have passed the correct I2C address to the program

5) Can't run the program.
    
    Make sure you have CMake and Make installed.  
    Make sure you are prossing the proper command line arguments as well.

6) Can't get a handle on I2C device

    Make sure to run the program as sudo

7) Getting different readings after switching ADS1115

    Recalibration might be required to gain new correction values for angular and linear coefficients.
    It is advisable not to switch ADS1115 between boards unless a fault occurs.