#include <curses.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <string.h>
#include <getopt.h>
#include <string>
#include <deque>
#include <pthread.h>
#include <utility>
#include "dh.h"
#include <iostream>
#include <iomanip>

using namespace std;

unsigned char key[256];
unsigned char iv[256];
mpz_t A;
mpz_t B;
static pthread_t trecv;     /* wait for incoming messagess and post to queue */
void* recvMsg(void*);       /* for trecv */
static pthread_t tcurses;   /* setup curses and draw messages from queue */
void* cursesthread(void*);  /* for tcurses */
/* tcurses will get a queue full of these and redraw the appropriate windows */
struct redraw_data {
	bool resize;
	string msg;
	string sender;
	WINDOW* win;
};
static deque<redraw_data> mq; /* messages and resizes yet to be drawn */
/* manage access to message queue: */
static pthread_mutex_t qmx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t qcv = PTHREAD_COND_INITIALIZER;

/* XXX different colors for different senders */

/* record chat history as deque of strings: */
static deque<string> transcript;

#define max(a, b)         \
	({ typeof(a) _a = a;    \
	 typeof(b) _b = b;    \
	 _a > _b ? _a : _b; })

/* network stuff... */

int listensock, sockfd;

[[noreturn]] static void fail_exit(const char *msg);

[[noreturn]] static void error(const char *msg)
{
	perror(msg);
	fail_exit("");
}

int initServerNet(int port)
{
	int reuse = 1;
	struct sockaddr_in serv_addr;
	listensock = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(listensock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	/* NOTE: might not need the above if you make sure the client closes first */
	if (listensock < 0)
		error("ERROR opening socket");
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);
	if (bind(listensock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR on binding");
	fprintf(stderr, "listening on port %i...\n",port);
	listen(listensock,1);
	socklen_t clilen;
	struct sockaddr_in  cli_addr;
	sockfd = accept(listensock, (struct sockaddr *) &cli_addr, &clilen);
	if (sockfd < 0)
		error("error on accept");
	
	
	/* three-way handshake */

	// First handshake: client sends SYN packet
	int rand_num;
	char syn_packet[1024];
	ssize_t n = recv(sockfd, syn_packet, sizeof(syn_packet), 0);
	if (n < 0) {
    		error("ERROR receiving SYN packet");
	} else if (n == 0) {
    		error("Connection closed by client");
	} else {
    		// Check if received packet is valid SYN
    		rand_num = atoi(syn_packet);
    		if (rand_num == 0) {
        		error("Invalid SYN packet received");
    		} else {
        		printf("Received SYN packet from client: %d\n", rand_num);
        	}
        }
        
        // Second handshake: server sends SYN and ACK packets
	char syn_ack_packet[1024];
	sprintf(syn_ack_packet, "SYN+ACK:%d", rand_num + 1);
	n = send(sockfd, syn_ack_packet, strlen(syn_ack_packet), 0);
	if (n < 0) {
    		error("ERROR sending SYN+ACK packet");
	} else {
    		printf("Sent SYN+ACK packet to client: %d\n", rand_num + 1);
    	}
    	
    	// Third handshake: client sends ACK packet
	char ack_packet[1024];
	n = recv(sockfd, ack_packet, sizeof(ack_packet), 0);
	if (n < 0) {
    		error("ERROR receiving ACK packet");
	} else if (n == 0) {
    		error("Connection closed by client");
	} else {
    		// Check if received packet is valid ACK
    		int ack_num = atoi(ack_packet);
    		if (ack_num != rand_num + 2) {
        		error("Invalid ACK packet received");
    		} else {
        		printf("Received ACK packet from client: %d\n", ack_num);
    		}
	}
			
    		
	/* Diffie–Hellman key exchange */
	
	init("params");
	NEWZ(a); // secret key (a random exponent)
	NEWZ(A); // public key: A = g^a mod p 
	dhGen(a,A);
	
	// Send public key A to the client
	char A_str[1024];	
	mpz_get_str(A_str, 16, A);
	send(sockfd, A_str, 1024, 0);

	// Receive the public key B sent by the client
	char B_str[1024];
	recv(sockfd, B_str, 1024, 0);
	mpz_set_str(B, B_str, 16);

        // Server's key derivation
        const size_t klen = 512;
	unsigned char kA[klen];
	dhFinal(a, A, B, kA, klen);
	
	printf("Server's key:\n");
	for (size_t i = 0; i < 256; i++) {
		printf("%02x ",kA[i]);
	}
	printf("\n");
	
	printf("Server's IV:\n");
	for (size_t i = 256; i < 512; i++) {
		printf("%02x ",kA[i]);
	}
	printf("\n");
	
	memcpy(key, kA, 256); // Assign first 256 bits to key
	memcpy(iv, kA + 256, 256); // Assign last 256 bits to iv
	
	close(listensock);
	fprintf(stderr, "connection made, starting session...\n");
	/* at this point, should be able to send/recv on sockfd */
	return 0;
}


static int initClientNet(char* hostname, int port)
{
	struct sockaddr_in serv_addr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct hostent *server;
	if (sockfd < 0)
		error("ERROR opening socket");
	server = gethostbyname(hostname);
	if (server == NULL) {
		fprintf(stderr,"ERROR, no such host\n");
		exit(0);
	}
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);
	serv_addr.sin_port = htons(port);
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
		error("ERROR connecting");
	
	
	/* three-way handshake */

	// First handshake: client sends SYN packet
	srand(time(NULL));
	int rand_num = rand() % 1000;
	char syn_packet[1024];
	sprintf(syn_packet, "%d", rand_num);
	ssize_t n = send(sockfd, syn_packet, strlen(syn_packet), 0);
	if (n < 0) {
    		error("ERROR sending SYN packet");
	} else {
    		printf("Sent SYN packet to server: %d\n", rand_num);
	}

	// Second handshake: server sends SYN and ACK packets
	int syn_ack_num;
	char syn_ack_packet[1024];
	n = recv(sockfd, syn_ack_packet, sizeof(syn_ack_packet), 0);
	if (n < 0) {
    		error("ERROR receiving SYN+ACK packet");
	} else if (n == 0) {
    		error("Connection closed by server");
	} else {
    		// Check if received packet is valid SYN+ACK
    		char syn_ack_str[1024];
    		sprintf(syn_ack_str, "SYN+ACK:%d", rand_num + 1);
    		if (strncmp(syn_ack_packet, syn_ack_str, strlen(syn_ack_str)) != 0) {
        		error("Invalid SYN+ACK packet received");
    		} else {
        		syn_ack_num = atoi(strchr(syn_ack_packet, ':') + 1);
        		printf("Received SYN+ACK packet from server: %d\n", syn_ack_num);
        	}
        }

        // Third handshake: client sends ACK packet
        char ack_packet[1024];
        sprintf(ack_packet, "%d", syn_ack_num + 1);
        n = send(sockfd, ack_packet, strlen(ack_packet), 0);
        if (n < 0) {
            	error("ERROR sending ACK packet");
        } else {
            	printf("Sent ACK packet to server: %d\n", syn_ack_num + 1);
        }

        
	/* Diffie–Hellman key exchange */

	init("params");
	NEWZ(b); // secret key (a random exponent)
	NEWZ(B); // public key: B = g^b mod p
	dhGen(b,B);
	
	// Receive the public key A from the server
	char A_str[1024];
	recv(sockfd, A_str, 1024, 0);
	mpz_set_str(A, A_str, 16);

	// Send public key B to the server
	char B_str[1024];
	mpz_get_str(B_str, 16, B);
	send(sockfd, B_str, 1024, 0);

	// Client's key derivation
	const size_t klen = 512;
	unsigned char kB[klen];
	dhFinal(b, B, A, kB, klen);
        
        printf("Client's key:\n");
	for (size_t i = 0; i < 256; i++) {
		printf("%02x ",kB[i]);
	}
	printf("\n");
        
        printf("Client's IV:\n");
	for (size_t i = 256; i < 512; i++) {
		printf("%02x ",kB[i]);
	}
	printf("\n");
	
	memcpy(key, kB, 256); // Assign first 256 bits to key
	memcpy(iv, kB + 256, 256); // Assign last 256 bits to iv

	/* at this point, should be able to send/recv on sockfd */
	return 0;
}

static int shutdownNetwork()
{
	shutdown(sockfd,2);
	unsigned char dummy[64];
	ssize_t r;
	do {
		r = recv(sockfd,dummy,64,0);
	} while (r != 0 && r != -1);
	close(sockfd);
	return 0;
}

/* end network stuff. */


[[noreturn]] static void fail_exit(const char *msg)
{
	// Make sure endwin() is only called in visual mode. As a note, calling it
	// twice does not seem to be supported and messed with the cursor position.
	if (!isendwin())
		endwin();
	fprintf(stderr, "%s\n", msg);
	exit(EXIT_FAILURE);
}

// Checks errors for (most) ncurses functions. CHECK(fn, x, y, z) is a checked
// version of fn(x, y, z).
#define CHECK(fn, ...) \
	do \
	if (fn(__VA_ARGS__) == ERR) \
	fail_exit(#fn"("#__VA_ARGS__") failed"); \
	while (false)

static bool should_exit = false;

// Message window
static WINDOW *msg_win;
// Separator line above the command (readline) window
static WINDOW *sep_win;
// Command (readline) window
static WINDOW *cmd_win;

// Input character for readline
static unsigned char input;

static int readline_getc(FILE *dummy)
{
	return input;
}

/* if batch is set, don't draw immediately to real screen (use wnoutrefresh
 * instead of wrefresh) */
static void msg_win_redisplay(bool batch, const string& newmsg="", const string& sender="")
{
	if (batch)
		wnoutrefresh(msg_win);
	else {
		wattron(msg_win,COLOR_PAIR(2));
		wprintw(msg_win,"%s:",sender.c_str());
		wattroff(msg_win,COLOR_PAIR(2));
		wprintw(msg_win," %s\n",newmsg.c_str());
		wrefresh(msg_win);
	}
}

// Encryption functions
int encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *key,
            unsigned char *iv, unsigned char *ciphertext) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int ciphertext_len;
    ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
    EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len);
    ciphertext_len = len;
    EVP_EncryptFinal_ex(ctx, ciphertext + len, &len);
    ciphertext_len += len;
    EVP_CIPHER_CTX_free(ctx);
    return ciphertext_len;
}

// Decryption functions
int decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key,
            unsigned char *iv, unsigned char *plaintext) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;
    ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
    EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len);
    plaintext_len = len;
    EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
    plaintext_len += len;
    EVP_CIPHER_CTX_free(ctx);
    return plaintext_len;
}

static void msg_typed(char *line)
{   
        string mymsg;
        if (!line) {
                // Ctrl-D pressed on empty line
                should_exit = true;
                /* XXX send a "goodbye" message so other end doesn't
                 * have to wait for timeout on recv()? */
        } else {
                if (*line) {
                        /* Calculate the message's MAC and encrypt *
                         * the message and send it in combination  */
                         
                        // Calculate the MAC of a message
                        unsigned char mac[EVP_MAX_MD_SIZE];
                        unsigned int mac_len;
                        HMAC(EVP_sha512(), key, strlen((char*)key), (unsigned char*)line, strlen(line), mac, &mac_len);
                        
                        // Encrypt message
                        unsigned char ciphertext[strlen(line) + EVP_MAX_BLOCK_LENGTH];
                        int ciphertext_len = encrypt((unsigned char*)line, strlen(line), key, iv, ciphertext);
                        
                        // Combine the computed MAC and the encrypted message into a string
                        stringstream ss_mac;
                        for (int i = 0; i < mac_len; i++) {
                            ss_mac << std::hex << std::setw(2) << std::setfill('0') << (int)mac[i];
                        }
                        string mac_hex = ss_mac.str();
                        
                        stringstream ss_ciphertext;
                        for (int i = 0; i < ciphertext_len; i++) {
                            ss_ciphertext << std::hex << std::setw(2) << std::setfill('0') << (int)ciphertext[i];
                        }
                        string ciphertext_hex = ss_ciphertext.str();
                        
                        // Combine the MAC and encrypted message into a string
                        string combined = mac_hex + ciphertext_hex;
                        
                        add_history(line);      
                        mymsg = string(line);
                        transcript.push_back("me: " + mymsg);
                        ssize_t nbytes;
                        
                        // Send out the combined string
                        if ((nbytes = send(sockfd, combined.c_str(), combined.length(), 0)) == -1)
                                error("send failed");
                }
                pthread_mutex_lock(&qmx);
                mq.push_back({false,mymsg,"me",msg_win});
                pthread_cond_signal(&qcv);
                pthread_mutex_unlock(&qmx);
        }
}


/* if batch is set, don't draw immediately to real screen (use wnoutrefresh
 * instead of wrefresh) */
static void cmd_win_redisplay(bool batch)
{
	int prompt_width = strnlen(rl_display_prompt, 128);
	int cursor_col = prompt_width + strnlen(rl_line_buffer,rl_point);

	werase(cmd_win);
	mvwprintw(cmd_win, 0, 0, "%s%s", rl_display_prompt, rl_line_buffer);
	/* XXX deal with a longer message than the terminal window can show */
	if (cursor_col >= COLS) {
		// Hide the cursor if it lies outside the window. Otherwise it'll
		// appear on the very right.
		curs_set(0);
	} else {
		wmove(cmd_win,0,cursor_col);
		curs_set(1);
	}
	if (batch)
		wnoutrefresh(cmd_win);
	else
		wrefresh(cmd_win);
}

static void readline_redisplay(void)
{
	pthread_mutex_lock(&qmx);
	mq.push_back({false,"","",cmd_win});
	pthread_cond_signal(&qcv);
	pthread_mutex_unlock(&qmx);
}

static void resize(void)
{
	if (LINES >= 3) {
		wresize(msg_win,LINES-2,COLS);
		wresize(sep_win,1,COLS);
		wresize(cmd_win,1,COLS);
		/* now move bottom two to last lines: */
		mvwin(sep_win,LINES-2,0);
		mvwin(cmd_win,LINES-1,0);
	}

	/* Batch refreshes and commit them with doupdate() */
	msg_win_redisplay(true);
	wnoutrefresh(sep_win);
	cmd_win_redisplay(true);
	doupdate();
}

static void init_ncurses(void)
{
	if (!initscr())
		fail_exit("Failed to initialize ncurses");

	if (has_colors()) {
		CHECK(start_color);
		CHECK(use_default_colors);
	}
	CHECK(cbreak);
	CHECK(noecho);
	CHECK(nonl);
	CHECK(intrflush, NULL, FALSE);

	curs_set(1);

	if (LINES >= 3) {
		msg_win = newwin(LINES - 2, COLS, 0, 0);
		sep_win = newwin(1, COLS, LINES - 2, 0);
		cmd_win = newwin(1, COLS, LINES - 1, 0);
	} else {
		// Degenerate case. Give the windows the minimum workable size to
		// prevent errors from e.g. wmove().
		msg_win = newwin(1, COLS, 0, 0);
		sep_win = newwin(1, COLS, 0, 0);
		cmd_win = newwin(1, COLS, 0, 0);
	}
	if (!msg_win || !sep_win || !cmd_win)
		fail_exit("Failed to allocate windows");

	scrollok(msg_win,true);

	if (has_colors()) {
		// Use white-on-blue cells for the separator window...
		CHECK(init_pair, 1, COLOR_WHITE, COLOR_BLUE);
		CHECK(wbkgd, sep_win, COLOR_PAIR(1));
		/* NOTE: -1 is the default background color, which for me does
		 * not appear to be any of the normal colors curses defines. */
		CHECK(init_pair, 2, COLOR_MAGENTA, -1);
	}
	else {
		wbkgd(sep_win,A_STANDOUT); /* c.f. man curs_attr */
	}
	wrefresh(sep_win);
}

static void deinit_ncurses(void)
{
	delwin(msg_win);
	delwin(sep_win);
	delwin(cmd_win);
	endwin();
}

static void init_readline(void)
{
	// Let ncurses do all terminal and signal handling
	rl_catch_signals = 0;
	rl_catch_sigwinch = 0;
	rl_deprep_term_function = NULL;
	rl_prep_term_function = NULL;

	// Prevent readline from setting the LINES and COLUMNS environment
	// variables, which override dynamic size adjustments in ncurses. When
	// using the alternate readline interface (as we do here), LINES and
	// COLUMNS are not updated if the terminal is resized between two calls to
	// rl_callback_read_char() (which is almost always the case).
	rl_change_environment = 0;

	// Handle input by manually feeding characters to readline
	rl_getc_function = readline_getc;
	rl_redisplay_function = readline_redisplay;

	rl_callback_handler_install("> ", msg_typed);
}

static void deinit_readline(void)
{
	rl_callback_handler_remove();
}

static const char* usage =
"Usage: %s [OPTIONS]...\n"
"Secure chat for CSc380.\n\n"
"   -c, --connect HOST  Attempt a connection to HOST.\n"
"   -l, --listen        Listen for new connections.\n"
"   -p, --port    PORT  Listen or connect on PORT (defaults to 1337).\n"
"   -h, --help          show this message and exit.\n";

int main(int argc, char *argv[])
{
	// define long options
	static struct option long_opts[] = {
		{"connect",  required_argument, 0, 'c'},
		{"listen",   no_argument,       0, 'l'},
		{"port",     required_argument, 0, 'p'},
		{"help",     no_argument,       0, 'h'},
		{0,0,0,0}
	};
	// process options:
	char c;
	int opt_index = 0;
	int port = 1337;
	char hostname[HOST_NAME_MAX+1] = "localhost";
	hostname[HOST_NAME_MAX] = 0;
	bool isclient = true;


	while ((c = getopt_long(argc, argv, "c:lp:h", long_opts, &opt_index)) != -1) {
		switch (c) {
			case 'c':
				if (strnlen(optarg,HOST_NAME_MAX))
					strncpy(hostname,optarg,HOST_NAME_MAX);
				break;
			case 'l':
				isclient = false;
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 'h':
				printf(usage,argv[0]);
				return 0;
			case '?':
				printf(usage,argv[0]);
				return 1;
		}
	}
	
	if (isclient) {
		initClientNet(hostname,port);
	} else {
		initServerNet(port);
	}

	/* NOTE: these don't work if called from cursesthread */
	init_ncurses();
	init_readline();
	/* start curses thread */
	if (pthread_create(&tcurses,0,cursesthread,0)) {
		fprintf(stderr, "Failed to create curses thread.\n");
	}
	/* start receiver thread: */
	if (pthread_create(&trecv,0,recvMsg,0)) {
		fprintf(stderr, "Failed to create update thread.\n");
	}

	/* put this in the queue to signal need for resize: */
	redraw_data rd = {false,"","",NULL};
	do {
		int c = wgetch(cmd_win);
		switch (c) {
			case KEY_RESIZE:
				pthread_mutex_lock(&qmx);
				mq.push_back(rd);
				pthread_cond_signal(&qcv);
				pthread_mutex_unlock(&qmx);
				break;
				// Ctrl-L -- redraw screen
			// case '\f':
			// 	// Makes the next refresh repaint the screen from scratch
			// 	/* XXX this needs to be done in the curses thread as well. */
			// 	clearok(curscr,true);
			// 	resize();
			// 	break;
			default:
				input = c;
				rl_callback_read_char();
		}
	} while (!should_exit);

	shutdownNetwork();
	deinit_ncurses();
	deinit_readline();
	return 0;
}

/* Let's have one thread responsible for all things curses.  It should
 * 1. Initialize the library
 * 2. Wait for messages (we'll need a mutex-protected queue)
 * 3. Restore terminal / end curses mode? */

/* We'll need yet another thread to listen for incoming messages and
 * post them to the queue. */

void* cursesthread(void* pData)
{
	/* NOTE: these calls only worked from the main thread... */
	// init_ncurses();
	// init_readline();
	while (true) {
		pthread_mutex_lock(&qmx);
		while (mq.empty()) {
			pthread_cond_wait(&qcv,&qmx);
			/* NOTE: pthread_cond_wait will release the mutex and block, then
			 * reaquire it before returning.  Given that only one thread (this
			 * one) consumes elements of the queue, we probably don't have to
			 * check in a loop like this, but in general this is the recommended
			 * way to do it.  See the man page for details. */
		}
		/* at this point, we have control of the queue, which is not empty,
		 * so write all the messages and then let go of the mutex. */
		while (!mq.empty()) {
			redraw_data m = mq.front();
			mq.pop_front();
			if (m.win == cmd_win) {
				cmd_win_redisplay(m.resize);
			} else if (m.resize) {
				resize();
			} else {
				msg_win_redisplay(false,m.msg,m.sender);
				/* Redraw input window to "focus" it (otherwise the cursor
				 * will appear in the transcript which is confusing). */
				cmd_win_redisplay(false);
			}
		}
		pthread_mutex_unlock(&qmx);
	}
	return 0;
}

void* recvMsg(void*)
{
    size_t maxlen = 512;
    char msg[maxlen+1];
    ssize_t nbytes;
    while (1) {            
        if ((nbytes = recv(sockfd,msg,maxlen,0)) == -1)
            error("recv failed");
        msg[nbytes] = 0; /* make sure it is null-terminated */
        
        if (nbytes == 0) {
            // Signal to the main loop that we should quit
            should_exit = true;
            return 0;
        }
        
        /* Split the received combined message  * 
         * compare MACs and decrypt the message */
         
        // Split the received combination message
        string combined(msg);
        string mac_str = combined.substr(0, 128);  // MAC is the first 128 characters
        string ciphertext_hex = combined.substr(128);
        
        // Convert the ciphertext to unsigned char array
        size_t ciphertext_len = ciphertext_hex.length() / 2;
        unsigned char ciphertext[ciphertext_len];
        for (int i = 0; i < ciphertext_len; i++) {
            sscanf(ciphertext_hex.substr(i*2, 2).c_str(), "%02x", &ciphertext[i]);
        }

        // Decrypt the message
        unsigned char plaintext[maxlen];
        int plaintext_len = decrypt(ciphertext, ciphertext_len, key, iv, plaintext);
        string plaintext_str(reinterpret_cast<const char*>(plaintext), plaintext_len);

        // Calculate the MAC of the decrypted message
        unsigned char decrypted_mac[EVP_MAX_MD_SIZE];
        unsigned int decrypted_mac_len;
        HMAC(EVP_sha512(), key, strlen((char*)key), plaintext, plaintext_len, decrypted_mac, &decrypted_mac_len);

        stringstream stream;
        for (int i = 0; i < decrypted_mac_len; i++) {
            stream << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(decrypted_mac[i]);
        }
        string mac_calc = stream.str();
        
        /* A known bug                                     */
        /* The first message received may indicate that    */
        /* Message has been tampered with!                 */
        
        /* Frequency: Occasionally, not every time.        */
        /* Solution: Send a message to the other party.    */
        /* If this warning continues prompt, it means that */
        /* both parties are not using the same shared key. */
        
        /* TODO: fix this known bug */
        
        // Compare MACs
        if (mac_calc == mac_str) {
            pthread_mutex_lock(&qmx);
            mq.push_back({false, plaintext_str, "Mr Thread", msg_win});
            pthread_cond_signal(&qcv);
            pthread_mutex_unlock(&qmx);
        } else {
            string msg = "Message has been tampered with!";
            pthread_mutex_lock(&qmx);
            mq.push_back({false, msg, "Warning", msg_win});
            pthread_cond_signal(&qcv);
            pthread_mutex_unlock(&qmx);
        }
    }
    return 0;
}