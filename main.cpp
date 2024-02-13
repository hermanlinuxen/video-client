// Video Client Main

/* 
Resources
    - https://en.cppreference.com/w/cpp/language/ascii
    - https://en.wikipedia.org/wiki/Box-drawing_character
*/

// Libraries
#include <iostream>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <thread>
#include <fstream>
#include <signal.h>

// Global Variables
bool debug = false;
bool log_to_file = true;
bool collapse_threads = false;
bool interrupt = false;

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

void draw_box ( int top_w, int top_h, int bot_w, int bot_h, bool title, std::string title_str ) {
    std::string character;
    int x; int y;
    for ( x = top_h; x <= bot_h; ++x ) { // For each line downwords
        for ( y = top_w; y <= bot_w; ++y ) { // For each column rightward
            if ( x == top_h && y == top_w ) {
                // top left corner
                character = "╭";
            } else if ( x == top_h && y == bot_w ) {
                // top right corner
                character = "╮";
            } else if ( x == top_h ) {
                // top bar
                character = "─";
            } else if ( x == bot_h && y == top_w ) {
                // bot left corner
                character = "╰";
            } else if ( x == bot_h && y == bot_w ) {
                // bot right corner
                character = "╯";
            } else if ( x == bot_h ) {
                // bot bar
                character = "─";
            } else if ( y == top_w || y == bot_w ) {
                // left and right border
                character = "│";
            } else {
                continue;
            }
            printf("\033[%d;%dH%s", x, y, character.c_str());
        }
    }
    if ( title ) {
        int tb_w = top_w + bot_w;
        int subtract = title_str.size();
        int m = tb_w / 2;
        subtract = subtract / 2;
        subtract = subtract + 2;
        m = m - subtract;
        printf("\033[%d;%dH", top_h, m);
        std::cout << "┨ " << title_str << " ┠";
    }
}

void draw_ui ( int w, int h ) {
    //std::cout << "Width: " << w << " Height: " << h << "\n";
    int vertical_border = 0;
    int horizontal_border = 0;

    bool box_corner_overlap_vertical;
    if ( vertical_border == 0 ) { box_corner_overlap_vertical = true; }
    else { box_corner_overlap_vertical = false; }
    bool box_corner_overlap_horizontal;
    if ( horizontal_border == 0 ) { box_corner_overlap_horizontal = true; }
    else { box_corner_overlap_horizontal = false; }

    int box1_top_w = horizontal_border + 1;
    int box1_bot_w = w / 2 - horizontal_border;
    int box1_top_h = vertical_border + 1;
    int box1_bot_h = h - vertical_border;

    int box2_top_w = w / 2 + horizontal_border;
    int box2_bot_w = w - horizontal_border;
    int box2_top_h = vertical_border + 1;
    int box2_bot_h = h - vertical_border;


    draw_box( box1_top_w, box1_top_h, box1_bot_w, box1_bot_h, true, "Loooong ass Title here" );
    draw_box( box2_top_w, box2_top_h, box2_bot_w, box2_bot_h, true, "Title" );
}

void capture_interrupt(int signum){
    interrupt = true;
    collapse_threads = true;
}

int main ( int argc, char *argv[] ) {

    signal (SIGINT, capture_interrupt);

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
    int tmp_w, tmp_h, w, h; // Used to store previous window size to detect changes.
    struct winsize size;

    std::cout << "\033[2J"; // Clear screen.
    fputs("\e[?25l", stdout); // remove cursor

    std::thread input_thread(THREAD_input); // Start input thread

    while ( true ) { // Main loop

        if ( collapse_threads == true ) {
            log("Exiting main function");
            break;
        }

        /*
        std::string test_text;
        //printf("\033[%d;%dH%s", 10, 20, test_text.c_str());
        //printf("\033[%d;%dH", h, w);
        int x; int y;
        for ( x = 1; x <= h; ++x ) {
            for ( y = 1; y <= w; ++y ) {
                if ( x == 1 || y == 1 ) {
                    test_text = "A";
                } else if ( x == h || y == w ) {
                    test_text = "B";
                } else {
                    test_text = "C";
                }
                printf("\033[%d;%dH%s", x, y, test_text.c_str());
                //std::cout << "X: " << x << " Y: " << y << "\n";
                //usleep(1000);
            }
        }
        */

        ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
        w = size.ws_col; h = size.ws_row;
        if ( tmp_w != w || tmp_h != h ) {
            std::cout << "\033[2J";
            tmp_w = w;
            tmp_h = h;
            draw_ui(w, h);
            std::string teststring = "test123!";
            printf("\033[%d;%dH", 10, 10);
            std::cout << "Information\n";
            printf("\033[%d;%dH", 11, 10);
            std::cout << "More Information\n";
        }


        usleep(10000);
    }

    fputs("\e[?25h", stdout); // Show cursor again.

    collapse_threads = true;

    if ( interrupt ) {
        printf("\033[%d;%dH", h, 0);
        log("Interrupt signal received", 3);
        std::cout << "Interrupt!\n";
        return 1;
    }

    input_thread.join();

    return 0;
}
