// Video Client Main

/* 
Resources
    - https://en.cppreference.com/w/cpp/language/ascii
    - https://en.wikipedia.org/wiki/Box-drawing_character
    - https://docs.invidious.io/api/
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
#include <ctime>
#include <bits/stdc++.h>

// Curl
#include <curl/curl.h>

// Global Variables
bool debug = false;
bool log_to_file = true;
bool collapse_threads = false;
bool interrupt = false;
int epoch_time;

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
bool update_ui = true;

// colors:
const std::string color_reset  = "\033[0m";
const std::string color_red    = "\033[31m";
const std::string color_green  = "\033[32m";
const std::string color_yellow = "\033[33m";
const std::string color_blue   = "\033[34m";
const std::string color_cyan   = "\033[36m";

const std::string frame_title_color = color_green;
const std::string frame_color = color_yellow;

// Input Parameter switches
bool arg_verbose = false;
bool arg_help    = false;

// Videos vector for storing video details.
struct inv_videos{ // invidious videos
    std::string URL;
    bool used = false;
    int published;
    int lengthseconds;
    int viewCount;
    std::string title;
    std::string publishedtext;
    std::string author;
    std::string author_id;
    std::string description;
};
std::vector<inv_videos> video;

struct inv_instances{
    std::string name;
    bool used = false;
    int last_get = 1;
    int health;
};

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

// Curl
std::pair<std::string, CURLcode> fetch (const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string response;
    CURLcode res = CURLE_FAILED_INIT;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    return std::make_pair(response, res);
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

void draw_box ( int top_w, int top_h, int bot_w, int bot_h, bool title, int type, std::string title_str ) {
    /*
    Drawbox types: Why do i do this to myself.
        0) Default, no special corners / walls.
        1) Left most box with right side facing another box.
        2) Right most box with left side facing another box.
        3) Top box.
        4) bottom box.
        5) Top left.
        6) Top Right.
        7) Bottom Left.
        8) Bottom Right.
    */
    std::string character;
    int x; int y;
    bool skip = false;
    for ( x = top_h; x <= bot_h; ++x ) { // For each line downwords
        for ( y = top_w; y <= bot_w; ++y ) { // For each column rightward
            if ( x == top_h && y == top_w ) {
                // top left corner
                if ( type == 2 ) { character = "┬"; }
                else { character = "╭"; }

            } else if ( x == top_h && y == bot_w ) {
                // top right corner
                if ( type == 1 ) { skip = true; }
                else { character = "╮"; }

            } else if ( x == top_h ) {
                // top bar
                //if (  ) { skip = true; }
                //else { character = "─"; }
                character = "─";

            } else if ( x == bot_h && y == top_w ) {
                // bot left corner
                if ( type == 2 ) { character = "┴"; }
                else { character = "╰"; }

            } else if ( x == bot_h && y == bot_w ) {
                // bot right corner
                if ( type == 1 ) { skip = true; }
                else { character = "╯"; }

            } else if ( x == bot_h ) {
                // bot bar
                if ( type == 3 || type == 5 || type == 6 ) { skip = true; }
                else { character = "─"; }

            } else if ( y == top_w ) {
                // left border
                if ( type == 2 || type == 6 || type == 8 ) { skip = true; }
                else { character = "│"; }

            } else if ( y == bot_w ) {
                // right border
                //if (  ) { skip = true; }
                //else { character = "│"; }
                character = "│";

            } else {
                continue;
            }
            if ( skip ) {
                skip = false;
                continue;
            }
            printf("\033[%d;%dH", x, y);
            std::cout << frame_color << character.c_str() << color_reset;
        }
    }
    if ( title ) {
        int tb_w = top_w + bot_w;
        int subtract = title_str.size();
        int m = tb_w / 2;
        subtract = subtract / 2;
        subtract = subtract + 1;
        m = m - subtract;
        printf("\033[%d;%dH", top_h, m);
        std::cout << frame_color << "┨ " << frame_title_color << title_str << frame_color << " ┠" << color_reset;
    }
}

void draw_ui ( int w, int h ) {
    //std::cout << "Width: " << w << " Height: " << h << "\n";
    int vertical_border = 0;
    int horizontal_border = 0;

    int box1_top_w = horizontal_border + 1;
    int box1_bot_w = w / 2 - horizontal_border;
    int box1_top_h = vertical_border + 1;
    int box1_bot_h = h - vertical_border;

    int box2_top_w = w / 2 + horizontal_border;
    int box2_bot_w = w - horizontal_border;
    int box2_top_h = vertical_border + 1;
    int box2_bot_h = h - vertical_border;


    draw_box( box1_top_w, box1_top_h, box1_bot_w, box1_bot_h, true, 1, "Loooong ass Title here" );
    draw_box( box2_top_w, box2_top_h, box2_bot_w, box2_bot_h, true, 2, "Title" );
}

void capture_interrupt (int signum) {
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
                log("Double dash parameter");
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

    // Test Stuff:
    //std::vector<inv_videos> video;
    video.push_back(inv_videos()); // Created initial element at index 0.



    video[0].URL = "longstring1";
    video[0].title = "Title Name Here";
    video.push_back(inv_videos());
    video[1].URL = "Anotherlongstring1";
    video[1].title = "Another Title Name Here";

    log("Vector 0: " + video[0].title + " " + video[0].URL);
    log("Vector 1: " + video[1].title + " " + video[1].URL);

    std::string url = "https://inv.tux.pizza/api/v1/videos/SpeSpA3e56A?fields=title,videoId";
    auto result = fetch(url);

    if ( result.second == CURLE_OK ) {
        log("JSON data fetched successfully: " + result.first);
    } else {
        log("Failed to fetch JSON data from URL. Error: " + std::to_string(curl_easy_strerror(result.second)));
    }

    // Local UI Elements
    int tmp_w, tmp_h, w, h; // Used to store previous window size to detect changes.
    struct winsize size;

    std::cout << "\033c"; // Clear screen.
    std::cout << "\e[?25l"; // remove cursor

    std::thread input_thread(THREAD_input); // Start input thread

    setvbuf(stdout, NULL, _IONBF, 0); // Disables buffering

    while ( true ) { // Main loop

        epoch_time = std::time(0); // Update epoch time variable

        if ( collapse_threads == true ) {
            log("Exiting main function");
            break;
        }

        ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
        w = size.ws_col; h = size.ws_row;
        if ( tmp_w != w || tmp_h != h || update_ui == true ) {
            std::cout << "\033c"; // Clear screen. Also reset.
            tmp_w = w;
            tmp_h = h;
            update_ui = false;

            // This is where everything is drawn to STDOut.
            draw_ui(w, h);

            std::cout << "\e[?25l"; // remove cursor
        }


        usleep(1000);
    }

    fputs("\e[?25h", stdout); // Show cursor again.

    collapse_threads = true;

    if ( interrupt ) {
        printf("\033[%d;%dH", h, 0);
        log("Interrupt signal received", 3);
        std::cout << "\nInterrupt!\n";
        return 1;
    }

    input_thread.join();

    return 0;
}
