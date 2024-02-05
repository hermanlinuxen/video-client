// Video Client Main

/*
https://en.cppreference.com/w/cpp/language/ascii

*/

// Libraries
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <thread>

// Namespace
//using namespace std;

// Global Variables
bool logging = false;
bool logfile = false;
bool collapse_threads = false;

// input validation variables
int input_character;
bool input_character_exit = false;
bool exit_thread_started = false;
bool typing_mode = false;
unsigned char char_input_character;

// colors:
const std::string color_reset  = "\033[0m";
const std::string color_red    = "\033[31m";
const std::string color_green  = "\033[32m";
const std::string color_yellow = "\033[33m";
const std::string color_blue   = "\033[34m";
const std::string color_cyan   = "\033[36m";

// Parameter switches
bool arg_verbose = false;
bool arg_help    = false;

// Log functions
void log_main(std::string logmsg, int severity = 0, bool file = false) {
    std::string severity_prefix;
    if ( severity == 0 ) {
        severity_prefix = "DEBUG  ";
    } else if ( severity == 1 ) {
        severity_prefix = "WARNING";
    } else if ( severity == 2 ) {
        severity_prefix = "ERROR! ";
    } else if ( severity == 3 ) {
        severity_prefix = "FATAL! ";
    }
    std::string fullmsg;
    if ( file == false ) {
        if ( severity == 0 ) {
            fullmsg = "Video-Client - " + color_green + severity_prefix + color_reset + " - " + logmsg + "\n";
        } else if ( severity == 1 ) {
            fullmsg = "Video-Client - " + color_yellow + severity_prefix + color_reset + " - " + logmsg + "\n";
        } else if ( severity > 1 ) {
            fullmsg = "Video-Client - " + color_red + severity_prefix + color_reset + " - " + logmsg + "\n";
        }
        if ( severity == 0 ) {
            std::cout << fullmsg;
        } else {
            std::cerr << fullmsg;
        }
    } else {
        fullmsg = severity_prefix + " - " + logmsg + "\n";
        std::cout << "ToFile: " << fullmsg;
    }
}
void log(std::string message, int severity = 0) {
    if (logging = true){
        log_main(message, severity, logfile);
    }
}

// Exit check thread, multiple keys return 27 including escape and arrow keys.
void THREAD_input_verify_exit () {
    usleep(150); // 0.15MS
    if ( input_character == 27 ) {
        log("Received exit signal.");
        collapse_threads = true;
    } else {
        if ( input_character == 91 ) {
            usleep(500); // 0.5MS
            if ( input_character == 65 ) {
                log("Arrow Up");
            } else if ( input_character == 66 ) {
                log("Arrow Down");
            } else if ( input_character == 68 ) {
                log("Arrow Left");
            } else if ( input_character == 67 ) {
                log("Arrow Right");
            } else {
                log("Unknown key!", 2);
            }
        }
    }
    if ( collapse_threads == true ) {
        std::cout << "Press any key to exit...\n";
    }
}

//
void determine_input ( int input = 0 ) {
    if ( input == 0 ) {
        return;
    }
    log("Determining input for: " + std::to_string(input));
    if ( input == 27 ) {
        std::thread input_thread_verify_exit(THREAD_input_verify_exit);
        input_thread_verify_exit.detach();
    }
}

// Input thread keeping track of inputs, and adding to queue.
void THREAD_input () {
    log("Started input thread.");
    struct termios old_tattr, new_tattr;
    tcgetattr(STDIN_FILENO,&old_tattr);
    new_tattr=old_tattr;
    new_tattr.c_lflag &=(~ICANON & ~ECHO);
    tcsetattr(STDIN_FILENO,TCSANOW,&new_tattr);
    // Continous loop
    while ( true ) {
        usleep(100); // 0.1MS
        if ( collapse_threads == true ) {
            break;
        }
        if ( typing_mode == true ) {
            char_input_character=getchar();
            input_character = (int)char_input_character;
        } else {
            input_character=getchar();
        }
        determine_input(input_character);
    }
    tcsetattr(STDIN_FILENO,TCSANOW,&old_tattr);
}

int main() {

    std::thread input_thread(THREAD_input);

    while ( true ) { // Main loop

        usleep(10000); // 10MS
        if ( collapse_threads == true ) {
            log("Exiting main function");
            break;
        }
    }

    collapse_threads = true;

    input_thread.join();
    usleep(1000);

    return 0;
}
