ifconfig tap1 create
sysctl net.link.tap.up_on_open=1

sudo ifconfig tap1 10.0.0.1 up
sudo arp -s 10.0.0.2 00:a0:98:5a:4b:1b
