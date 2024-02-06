// Video Client Main

/* 
Resources
    - https://en.cppreference.com/w/cpp/language/ascii
*/

// Libraries
#include <iostream>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <thread>
#include <fstream>

// Global Variables
bool debug = false;
bool log_to_file = true;
bool collapse_threads = false;

// Global Constants
const std::string homedir = getenv("HOME");
const std::string userconfigdir = homedir + "/.config";
const std::string configdir = userconfigdir + "/video-client";
const std::string logfile = configdir + "/video-client.log";

// input validation variables
int input_character;
bool input_character_exit = false;
bool exit_thread_started = false;
bool typing_mode = false;
unsigned char char_input_character;

// UI elements


// colors:
const std::string color_reset  = "\033[0m";
const std::string color_red    = "\033[31m";
const std::string color_green  = "\033[32m";
const std::string color_yellow = "\033[33m";
const std::string color_blue   = "\033[34m";
const std::string color_cyan   = "\033[36m";

// Input Parameter switches
bool arg_verbose = false;
bool arg_help    = false;

void usage () {
    const char *usage_text = R""""(usage: video-client

Arguments:
    -h, --help                      Show this page
    -v, --verbose, --debug          Show debug logs)"""";

    std::cout << usage_text << "\n";
}

// Append file
void append_file (std::string file, std::string content) {
    std::fstream f;
    f.open(file, std::ios::app);
    if (!f)
        std::cout << "File does not exist: " << file << "\n";
    else {
        f << content;
        f.close();
    }
}

// Log functions
void log_main ( std::string logmsg, int severity = 0, bool file = false ) {
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
    } else if ( file == true ) {
        fullmsg = severity_prefix + " - " + logmsg + "\n";
        append_file(logfile, fullmsg);
    }
}
void log ( std::string message, int severity = 0 ) {
    if ( debug == false ){
        if ( severity == 0 ) {
            return;
        }
    }
    log_main(message, severity, log_to_file);
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
    while ( true ) { // Continous loop
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

void draw_ui ( int w, int h ) {
    std::cout << "Width: " << w << " Height: " << h << "\n";
}

int main ( int argc, char *argv[] ) {


    // Parse arguments
    int argument_iteration = 0;
    if ( argc > 100 ) { std::cout << "Too many arguments!\n"; usage(); return 1; }
    while ( true ) {
        ++argument_iteration;
        if ( argument_iteration >= argc ) {
            break;
        }
        std::string argument_as_string( argv[argument_iteration] );
        int argument_char_0_int = argument_as_string[0];
        int argument_char_1_int = argument_as_string[1];
        if ( argument_char_0_int == 45 ) {
            if ( argument_char_1_int == 45 ) { // Double dash parameter, still WIP
                std::cout << "Double Dash!\n";
            } else { // Single dash parameter
                for ( int argument_char_i = 1; argument_char_i < argument_as_string.length() ; ++argument_char_i ) {
                    int argument_char_current = argument_as_string[argument_char_i];
                    if ( argument_char_current == 118 ) { debug = true; log("Debugging Enabled!"); }
                    else if ( argument_char_current == 104 ) { usage(); return 0; }
                    else { std::cout << "Unknown parameter: -" << argument_as_string[argument_char_i] << "\n"; usage(); return 1; }
                }
            }
        } else {
            std::cout << "Unknown parameter: " << argv[argument_iteration] << "\n";
            usage();
            return 1;
        }
        //if ( argument_char_0 == "-" ) { std::cout << "Matching Single Dash!\n"; }
        log("Success on parsing parameter: " + argument_as_string);
    }

    // Local UI Elements
    int tmp_w, tmp_h; // Used to store previous window size to detect changes.
    struct winsize size;

    std::thread input_thread(THREAD_input); // Start input thread

    while ( true ) { // Main loop

        usleep(10000); // 10MS

        if ( collapse_threads == true ) {
            log("Exiting main function");
            break;
        }

        ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
        if ( tmp_w != size.ws_col || tmp_h != size.ws_row ) {
            tmp_w = size.ws_col;
            tmp_h = size.ws_row;
            draw_ui(size.ws_col, size.ws_row);
        }
    }

    log("testlog debug");
    log("testlog error", 2);

    collapse_threads = true;

    input_thread.join();
    usleep(1000);

    return 0;
}
