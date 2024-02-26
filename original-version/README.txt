To build this, cd into build folder and run:
cmake .. && make ./instrumentation-app && ./instrumentation-app [I2C_ADDRESS] [CONFIG_FILE.txt]   

Choose the config file based on the board being used.

Examples:

cmake .. && make ./instrumentation-app && ./instrumentation-app 0x48 configA.txt
cmake .. && make ./instrumentation-app && ./instrumentation-app 0x49 configB.txt