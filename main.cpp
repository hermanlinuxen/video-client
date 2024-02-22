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

// Json
#include "include/nlohmann/json.hpp"

// Namespaces
using json = nlohmann::json;

// Global Variables
bool debug = false;
bool log_to_file = true;
bool collapse_threads = false;
bool interrupt = false;
bool local_instances_updated = false;
int epoch_time;

// Global Constants
const std::string homedir = getenv("HOME");
const std::string userconfigdir = homedir + "/.config";
const std::string configdir = userconfigdir + "/video-client";
const std::string logfile = configdir + "/video-client.log";

// URL Variables
const std::string URL_instances = "https://api.invidious.io/instances.json?&sort_by=type,users";

// input validation variables
int input_character;
bool input_character_exit = false;
bool exit_thread_started = false;
bool typing_mode = false;
unsigned char char_input_character;
std::vector <int> input_list;

// UI elements
bool update_ui = true;
bool quit = false;

/*
    Menu items:
    0 - Main Menu
*/
int current_menu = 0;

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
std::vector<inv_videos> inv_videos_vector;

struct inv_instances{
    bool enabled; // if the program is going to use this instance
    bool api_enabled; // if the API is enabled for this instance
    std::string name; // instance name, got from top of array
    std::string URL; // URL
    std::string type; // https or other
    int last_get = 0; // ignored and 0 if enabled, if unreachable, retry after 10 minutes. Apply current epoch to this int.
    int health; // instance health 90d as int
    std::string region; // instance region
};
std::vector<inv_instances> inv_instances_vector;

void usage () {
    const char *usage_text = R""""(usage: video-client

Arguments:
    -h, --help                      Show this page
    -v, --verbose, --debug          Show debug logs)"""";

    std::cout << usage_text << "\n";
}

int epoch () {
    return std::time(0);
}

std::string to_string_bool ( bool boolean_in ) {
    std::string bool_str = boolean_in ? "true" : "false";
    return bool_str;
}

std::string to_string_int ( int integer_in ) {
    return std::to_string(integer_in);
}

std::string to_string_float ( float float_in ) {
    std::stringstream float_string;
    float_string << float_in;
    return float_string.str();
}

std::string to_string_char ( char char_in[] ) {
    std::string char_str( char_in );
    return char_str;
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
        severity_prefix = "INFO   ";
    } else if ( severity == 2 ) {
        severity_prefix = "WARNING";
    } else if ( severity == 3 ) {
        severity_prefix = "ERROR! ";
    } else if ( severity == 4 ) {
        severity_prefix = "FATAL! ";
    }
    std::string fullmsg;
    if ( ! file ) {
        if ( severity == 0 || severity == 1 ) {
            fullmsg = "Video-Client - " + color_green + severity_prefix + color_reset + " - " + logmsg + "\n";
        } else if ( severity == 2 ) {
            fullmsg = "Video-Client - " + color_yellow + severity_prefix + color_reset + " - " + logmsg + "\n";
        } else if ( severity > 2 ) {
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
    // Example: log("Message here: " + std::to_string(integer_variable), 1);
    if ( debug == false ){
        if ( severity == 0 ) {
            return;
        }
    }
    log_main(message, severity, log_to_file);
}

// Write Callback
size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *data) {
    size_t total_size = size * nmemb;
    data->append((char*)contents, total_size);
    return total_size;
}

// Curl
std::pair<bool, std::string> fetch (const std::string& url) {
    CURL *curl;
    CURLcode res;
    std::string output;
    bool success = false;

    // init curl
    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        // Set callback
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
        // run request
        res = curl_easy_perform(curl);
        // Check for errors
        std::string curl_error = curl_easy_strerror(res);
        if (res != CURLE_OK) {
            log("CURL failed: " + curl_error + ", URL: " + url, 3);
            success = false;
        } else {
            log("CURL successfully requested " + url);
            success = true;
        }
        // Clean up
        curl_easy_cleanup(curl);
    } else {
        log("Request failed! " + url, 3);
        success = false;
    }
    return std::make_pair(success, output);
}

void parse_instances(const json& data) { // receives instances json output, and refreshes list of local instances.
    inv_instances_vector.clear(); // clear instances vector
    int inv_instances_vector_iteration = 0; // Instance vector iteration counter
    for (const auto& entry : data) {
        std::string headname;
        std::string type;
        bool enabled = true;

        bool tmp_ratio_enabled = false;
        std::string tmp_ratio;
        int tmp_ratio_int;

        headname = entry[0]; // Name
        if (entry[1]["api"].is_null()) {
            enabled = false;
            log("Skipping non-API instance: " + headname);
            continue;
        }
        type = entry[1]["type"]; // Type, "HTTPS"

        // Extracting variables
        bool apiEnabled = entry[1]["api"];
        if (!entry[1]["monitor"].is_null()) {
            tmp_ratio = entry[1]["monitor"]["90dRatio"]["ratio"]; // Ratio, uptime last 90 Days as integer.
            tmp_ratio_int = std::stoi(tmp_ratio);
            tmp_ratio_enabled = true;
        }
        std::string uri = entry[1]["uri"]; // Instance URL 
        std::string region = entry[1]["region"]; // Instance Region, (US, DE, NO, etc...)

        if ( enabled ) { // add entry to instance list
            log("Adding instance: " + headname);
            inv_instances_vector.push_back(inv_instances());
            inv_instances_vector[inv_instances_vector_iteration].enabled = enabled;
            inv_instances_vector[inv_instances_vector_iteration].api_enabled = apiEnabled;
            inv_instances_vector[inv_instances_vector_iteration].name = headname;
            inv_instances_vector[inv_instances_vector_iteration].URL = uri;
            inv_instances_vector[inv_instances_vector_iteration].type = type;
            inv_instances_vector[inv_instances_vector_iteration].region = region;
            inv_instances_vector[inv_instances_vector_iteration].last_get = epoch();
            if ( tmp_ratio_enabled ) {
                inv_instances_vector[inv_instances_vector_iteration].health = tmp_ratio_int;
            } else {
                inv_instances_vector[inv_instances_vector_iteration].health = 0;
            }

            ++inv_instances_vector_iteration;
        }
    }
    /*
    std::cout << "Loop over vectors:\n";
    for (unsigned i=0; i<inv_instances_vector.size(); i++) {
        std::string vector_output_test = inv_instances_vector[i].name;
        std::cout << vector_output_test << "\n";
    }
    */
    if ( inv_instances_vector.size() >= 1 ) {
        log("Successfully added " + to_string_int(inv_instances_vector.size()) + " Instances.");
        local_instances_updated = true;
    } else {
        log("Unable to update local instance list!", 4);
    }
}

// Update local instance list
void THREAD_update_instances () {
    auto result = fetch(URL_instances);
    if ( result.first ) {
        try {
            json data = json::parse(result.second);
            parse_instances(data);
        } catch (const std::exception& e) {
            std::stringstream parse_result;
            parse_result << e.what();
            log("Error parsing JSON: " + parse_result.str(), 4);
        }
    } else {
        log("Curl is unable to contact API: " + URL_instances, 4);
    }
}

void add_key_input ( int key = 0) {
    if ( ! key == 0 ) {
        input_list.push_back(key);
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
                log("Unknown key!", 3);
            }
        }
    }
    if ( collapse_threads == true ) {
        usleep(1100); // Ensure this quits after thread.join is ready. If not, next message will print at last printed location.
        std::cout << "\nPress any key to exit...\n";
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
    } else if ( input == 113 ) {
        quit = true;
    } else {
        add_key_input(input);
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


    draw_box( box1_top_w, box1_top_h, box1_bot_w, box1_bot_h, true, 1, "Testing title" );
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
                std::cout << "Double Dash argument! Still WIP\n";
                return 1;
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

    // Start process for updating local instances.
    std::thread instance_update_thread(THREAD_update_instances);
    instance_update_thread.detach();

    // Local UI Elements
    int tmp_w, tmp_h, w, h; // Used to store previous window size to detect changes.
    struct winsize size;

    std::cout << "\033c"; // Clear screen.
    std::cout << "\e[?25l"; // remove cursor

    std::thread input_thread(THREAD_input); // Start input thread

    setvbuf(stdout, NULL, _IONBF, 0); // Disables buffering

    while ( true ) { // Main loop

        if ( current_menu == 0 && quit ) {
            collapse_threads = true;
        }

        if ( collapse_threads == true ) {
            log("Exiting main function", 1);
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


        // Test:
        usleep(10000000);

        log("Items in input vector: " + to_string_int(input_list.size()));

        for (unsigned i=0; i<input_list.size(); i++) {
            log("Input vector iteration: " + to_string_int(i) + " Value: " + to_string_int(input_list[i]));
        }


        usleep(1000);
    }

    fputs("\e[?25h", stdout); // Show cursor again.
    printf("\033[%d;%dH", h, 0); // move cursor to end of screen.

    collapse_threads = true;

    if ( interrupt ) {
        //printf("\033[%d;%dH", h, 0);
        log("Interrupt signal received", 4);
        std::cout << "\nInterrupt!\n";
        return 1;
    }

    input_thread.join();

    std::cout << "\nExiting...\n";

    return 0;
}
