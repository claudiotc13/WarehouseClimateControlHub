install paho

sudo apt-get install libssl-dev
git clone https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c
make
sudo make install

compilar
gcc main.c -l paho-mqtt3cs -o main