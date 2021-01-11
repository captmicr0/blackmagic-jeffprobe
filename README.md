Jeff Probe + support for nrf52810
=================

## Prereqs (tested under WSL (ubuntu) in Windows 10)

### ARM toolchain setup
Run these under a WSL prompt (Run `wsl` in a command prompt):
```
sudo apt install make python libncurses-dev
sudo ln -s /usr/lib/x86_64-linux-gnu/libncurses.so.6 /usr/lib/x86_64-linux-gnu/libncurses.so.5
sudo ln -s /usr/lib/x86_64-linux-gnu/libtinfo.so.6 /usr/lib/x86_64-linux-gnu/libtinfo.so.5

cd /usr/share
sudo apt-get https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz
sudo tar xjf gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2 -C /usr/share/
```

Add to ~/.profile:
```
export PATH=$PATH:/usr/share/gcc-arm-none-eabi-10-2020-q4-major/bin
```

Restart your terminal to get the PATH environment variables updated, and check if it works:
```
arm-none-eabi-gcc --version
arm-none-eabi-g++ --version
arm-none-eabi-gdb --version
arm-none-eabi-size --version
```

### Source code and prereqs
```
sudo apt install make python
```

GitHub setup (make sure to change your email below):
```
ssh-keygen -t ed25519 -C "your_email_here"
cat ~/.ssh/id_ed25519.pub
```
The above should output your public key. Add this key at github's ssh key page: [SSH and GPG keys (github.com)](https://github.com/settings/keys)	

Clone this repo:
```
git clone -b v1.6.2+nrf52810 https://github.com/sureshsitaula/blackmagic-jeffprobe
```

Building
----
From within the cloned repo folder `blackmagic-jeffprobe`, run
```
make PROBE_HOST=jeff
```

Flashing
----
Get `dfu-util` from http://dfu-util.sourceforge.net/

Install and run [Zadig](https://zadig.akeo.ie/). Select the WinUSB driver for the `Black Magic Firmware Update (SAMD2) (Interface *)` device in normal mode or `DFU Demo` device in DFU mode (see below) if they are not already active.

The commands below assume a `cmd` prompt, not `WSL`.

Hold the button on the jeff-probe while plugging it in to get it into DFU mode. Alternatively run this from the blackmagic-jeffprobe folder:
```
dfu-util.exe -d 1d50:6018,:6017 -s 0x2000:leave -D src\blackmagic.bin
```

(If you run the above command instead of holding the button, you'll get an error about a file id mismatch. That is ok.)

You'll see just the green LED turn on instead of green + orange LEDS when in DFU mode.

Now upload the new firmware:
```
dfu-util.exe -d 0000:0000 -s 0x2000:leave -D src\blackmagic.bin
```
Note the use of `0x2000` as the start address used for the jeff probe, which differs from `0x08002000` in the original blackmagic probe.

### References
https://github.com/blacksphere/blackmagic/wiki/Hacking

https://askubuntu.com/questions/1243252/how-to-install-arm-none-eabi-gdb-on-ubuntu-20-04-lts-focal-fossa

# Original README follows below

Black Magic Probe
=================

[![Build Status](https://travis-ci.org/blacksphere/blackmagic.svg?branch=master)](https://travis-ci.org/blacksphere/blackmagic)
[![Gitter](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/blacksphere/blackmagic?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)
[![Donate](https://img.shields.io/badge/paypal-donate-blue.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=N84QYNAM2JJQG)
[![Kickstarter](https://img.shields.io/badge/kickstarter-back%20us-14e16e.svg)](https://www.kickstarter.com/projects/esden/1bitsy-and-black-magic-probe-demystifying-arm-prog)

Firmware for the Black Magic Debug Probe.

The Black Magic Probe is a modern, in-application debugging tool for
embedded microprocessors. It allows you see what is going on 'inside' an
application running on an embedded microprocessor while it executes. It is
able to control and examine the state of the target microprocessor using a
JTAG or Serial Wire Debugging (SWD) port and on-chip debug logic provided
by the microprocessor. The probe connects to a host computer using a
standard USB interface. The user is able to control exactly what happens
using the GNU source level debugging software, GDB.

See online documentation at https://github.com/blacksphere/blackmagic/wiki

Binaries from the latest automated build are at http://builds.blacksphere.co.nz/blackmagic
