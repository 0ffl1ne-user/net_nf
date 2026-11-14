# Wifi notification daemon.
This is a lightweight wifi notification daemon utilizing epoll to wait on a netlink socket which is subscribed to nl80211's mlme multicast group and then send a notification which says that the wifi has been disconnected or connected along with the SSID shown besides the latter.

## Dependencies
**OS**: Linux <br>
**Libraries**: [libsystemd](https://github.com/systemd/systemd)

## Installation
First create a folder to clone the repo in:
```bash
mkdir ~/net_nf
cd ~/net_nf
git clone https://github.com/0ffl1ne-user/net_nf.git
```
Then run `build.sh` which creates the binary `net_nf` and then copies it to `/usr/local/bin/`:
```bash
sudo ./build.sh net_nf
```
Then in order for the program to start automatically at startup we need to copy the systemd service file `net_nf.service` inside `~/.config/systemd/user/` (create the folders if they dont exist with: `mkdir -p ~/.config/systemd/user`):
```bash
cp net_nf.service ~/.config/systemd/user
systemctl --user daemon-reload
systemctl --user enable --now net_nf
```

## Removal
To completely remove the application run:
```bash
cd ~
systemctl --user stop net_nf
sudo rm -r net_nf .config/systemd/user/net_nf.service /usr/local/bin/net_nf
systemctl --user daemon-reload
```

## License
This project is licensed under the [MIT License](LICENSE).
