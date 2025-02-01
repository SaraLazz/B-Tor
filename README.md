# B-Tor
NS3 simulation of a branched version of Tor to provide anonymity against a global adversary

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

### 5. Set up environment variables
```bash
export NS3_DIR=$(pwd)/ns-3.30
echo "export NS3_DIR=$(pwd)/ns-3.30" >> ~/.bashrc
echo "export PATH=$NS3_DIR/build:$PATH" >> ~/.bashrc
echo "export LD_LIBRARY_PATH=$NS3_DIR/build:$LD_LIBRARY_PATH" >> ~/.bashrc
echo "export PYTHONPATH=$NS3_DIR/build:$PY
```


