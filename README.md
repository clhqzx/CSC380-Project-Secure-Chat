<!-- ABOUT THE PROJECT -->
## About The Project



Write a chat program in C/C++ that provides:
* Authentication of correspondents
* Message secrecy (encryption)
* Message integrity (MACs)

Goals：
* Gain familiarity using cryptographic libraries (openssl).
* Experience in protocol design.
* Understanding various issues in network programming.
* How to avoid common software security issues.


<!-- ABOUT THE TEAM -->
## About The Team
Team members:
* Zhi Gao
* Wendian Jiang
* Xinwei Wu

<!-- Project Progress -->
## Project Progress
* Authentication of correspondents ✔️
* Message secrecy (encryption) ✔️
* Message integrity (MACs) ✔️

<!-- GETTING STARTED -->
## Getting Started

This project was running successful on Ubuntu.

### Prerequisites

* ncurses and its header files. If you are on linux/BSD, you might have to get a package like ncurses-devel or similar, although some distributions (e.g. Arch Linux) will include header files in the normal package (no -devel needed).

* readline and the header files (readline-devel).

* openssl and headers (openssl-devel).

* gmp and its header files (gmp-devel).

### Installation

1. Clone the repo
   ```sh
   $ git clone https://github.com/clhqzx/CSC380Project1.git
   ```
3. Open terminal on the file, and run makefile
   ```sh
   $ make
   ```
4. Begin to start chat, open two terminal windows

   one run
   ```js
   $ ./chat -l
   ```
   the other one run
   ```js
   $ ./chat -c YOUR SERVER'S IP ADDRESS
   ```
5. Chat!

<!-- ACKNOWLEDGMENTS -->
## Acknowledgments
* [Forward secrecy](https://en.wikipedia.org/wiki/Forward_secrecy)
* [Three-Way Handshake](https://www.techopedia.com/definition/10339/three-way-handshake)
* [Diffie–Hellman key exchange](https://en.wikipedia.org/wiki/Diffie%E2%80%93Hellman_key_exchange)
* [How to Send Secret Messages](https://www.freecodecamp.org/news/how-to-send-secret-messages/)
* [Message authentication code](https://en.wikipedia.org/wiki/Message_authentication_code)
* [Network programming guide](https://beej.us/guide/bgnet/)
* [Ncurses examples](https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/)
* [Info gmp](https://gmplib.org/manual/)
