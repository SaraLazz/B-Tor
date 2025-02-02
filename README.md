# B-Tor
NS3 simulation of a branched version of Tor to provide anonymity against a global adversary.

This project is an **adaptation of [nstor](https://github.com/tschorsch/nstor)**, modified to implement a branched Tor model.

## Installation  

To run the simulation, you need to install **NS-3.30** on your Linux system. Follow these steps for a Debian-based distribution (e.g., Ubuntu).   

### 1. Update the system  
```bash
sudo apt update && sudo apt upgrade -y
```

### 2. Install required dependencies
```bash
sudo apt install -y gcc g++ python3 python3-pip git mercurial qtbase5-dev cmake \
    libc6-dev libc6-dev-i386 gdb valgrind uncrustify doxygen graphviz imagemagick \
    texlive texlive-latex-extra texlive-fonts-recommended texlive-lang-english \
    python3-sphinx dia tcpdump sqlite sqlite3 libsqlite3-dev
```

### 3. Clone the NS-3 repository
```bash
wget https://www.nsnam.org/releases/ns-allinone-3.30.tar.bz2
tar xjf ns-allinone-3.30.tar.bz2
cd ns-allinone-3.30
```

### 4. Configure and build NS-3
```bash
./build.py --enable-examples --enable-tests
./build.py
```

## Running B-Tor

To run the **B-Tor** simulation, follow these steps:

In `examples/btor-example.cc`:
1. Set branch length (`branch_len`) at line 31 and chain length (`chain_len`) at line 32 (available configurations can be found in `doc2/EXP`).
2. Ensure line 33 is commented out
3. Uncomment line 63
4. Comment out line 64
<br>     
In `helper/branched-tor-dumbbell-helper.cc`:
1. Set node bandwidth at lines 278 and 318
2. Uncomment line 432 and comment out line 433
<br>
In `model/tor.cc`:
1. Comment out lines 384-389
<br>
In `model/pseudo-socket.cc`:
1. Set the amount of data the client should receive at line 405
<br>
After making these modifications, compile and run:

```bash
cd ns-3.30
./waf --run  btor-example
```

