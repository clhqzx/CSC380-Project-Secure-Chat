<!-- ABOUT THE PROJECT -->
## About The Project
This is an end-to-end chat program. It provides correspondent authentication, message integrity verification, and encrypted message delivery.

In this program, the server and the client first perform the Three-way Handshake Process. 

![image text](https://www.coengoedegebure.com/content/images/2018/09/handshake.png)

Then use the Diffie–Hellman key exchange to generate the shared key and iv. The shared key and iv will be used to calculate the MAC value of the message and encrypt and decrypt the message. 
![image text](https://i.imgur.com/BqymNdp.png)

In order to ensure the integrity and security of the message, this program will calculate the MAC value of the original message and encrypt the original message. Then combine the MAC value and the encrypted message into a message and send it to the other party. After receiving the message, the other party will split the message to obtain the MAC value and the encrypted message. Then decrypt the encrypted message, and calculate the MAC value of the decrypted message. This program will verify whether the message has been tampered with by a third party by comparing the two MAC values.

![image text](https://i.imgur.com/HLp6q9B.jpeg)

<!-- GETTING STARTED -->
## Getting Started

This project was running successful on Ubuntu.

### Prerequisites

* ```ncurses``` and its header files. If you are on linux/BSD, you might have to get a package like ncurses-devel or similar, although some distributions (e.g. Arch Linux) will include header files in the normal package (no -devel needed).

  ```
  sudo apt-get install libncurses5-dev libncursesw5-dev
  ```

* ```readline``` and the header files (readline-devel).

  ```
  sudo apt-get install libreadline-dev
  ```

* ```openssl``` and headers (openssl-devel).

  ```
  sudo apt-get install libssl-dev
  ```

* ```gmp``` and its header files (gmp-devel).

  ```
  sudo apt-get install  libgmp-dev
  ```

### Installation

1. Clone this repo
   ```
   git clone https://github.com/clhqzx/CSC380-Project-Secure-Chat.git
   ```
3. Open terminal on this project file, and run makefile
   ```
   make
   ```
4. Begin to start chat, open two terminal windows on this project file

   one run
   ```
   ./chat -l
   ```
   the other one run
   ```
   ./chat -c YOUR SERVER'S IP ADDRESS
   ```
5. Start chat!

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

<!-- ABOUT THE TEAM -->
## About The Team
Team members:
* Zhi Gao
* Wendian Jiang
* Xinwei Wu
