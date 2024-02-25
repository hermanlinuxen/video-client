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
#include <string>
#include <unistd.h>
#include <termios.h>
#include <thread>
#include <fstream>
#include <signal.h>
#include <ctime>
#include <bits/stdc++.h>
#include <filesystem>

// Curl
#include <curl/curl.h>

// Json
#include "include/nlohmann/json.hpp"

// Version
const std::string version_number = "0.0.1";

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
const std::string config_file_subscriptions = configdir + "/subscriptions.conf";
const std::string config_file_downloads = configdir + "/downloads.conf";
const std::string config_file_favorites = configdir + "/favorites.conf";
const std::string config_file_banned_instances = configdir + "/banned-instances.conf";
const std::string config_file_banned_channels = configdir + "/banned-channels.conf";

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
    1 - Browse / Contentview
    2 - Search
    3 - Status Page
    4 - Settings / Options
*/
int current_menu = 0;
const std::string menu_items[5] = {"Main Menu", "Browse", "Search", "Status", "Settings"};

/*
    Browse Types:
    0 - Popular
    1 - Subscriptions
    2 - Downloaded
    3 - Favorite
    4 - Channel (not scrollable, only accessible by manually selecting channel)
*/
int current_browse_type = 0;
const std::string browse_types[5] = {"Popular", "Subscriptions", "Downloaded", "Favorite", "Channel"};

const std::string logo[5] = {
    "__     ___     _               ____ _ _            _",
    "\\ \\   / (_) __| | ___  ___    / ___| (_) ___ _ __ | |_",
    " \\ \\ / /| |/ _` |/ _ \\/ _ \\  | |   | | |/ _ \\ '_ \\| __|",
    "  \\ V / | | (_| |  __/ (_) | | |___| | |  __/ | | | |_",
    "   \\_/  |_|\\__,_|\\___|\\___/   \\____|_|_|\\___|_| |_|\\__|"
};

// colors:
const std::string color_reset  = "\033[0m";
const std::string color_bold   = "\033[1m";
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
struct inv_videos{              // invidious videos
    std::string URL;            // 11 character video identifier
    int published;              // epoch time of release date
    int lengthseconds;          // video Length in seconds
    int viewCount;              // video views
    int downloaded_time = 0;    // epoch time when video was downloaded
    bool downloaded = false;    // if video is downloaded or not
    bool favorite = false;      // if video is favorite
    std::string title;          // video title
    std::string publishedtext;  // release date converted to human readable format?
    std::string author;         // video creator
    std::string author_id;      // ID of video creator
    std::string description;    // video description.
};
std::vector<inv_videos> inv_videos_vector;

struct inv_instances{
    bool enabled;               // if the program is going to use this instance
    bool api_enabled;           // if the API is enabled for this instance
    std::string name;           // instance name, got from top of array
    std::string URL;            // URL
    std::string type;           // https or other
    int last_get = 0;           // ignored and 0 if enabled, if unreachable, retry after 10 minutes. Apply current epoch to this int.
    int health;                 // instance health 90d as int
    std::string region;         // instance region
};
std::vector<inv_instances> inv_instances_vector;

std::vector<std::string> vec_browse_popular;            // VideoID's of popular list.
std::vector<std::string> vec_browse_subscriptions;      // VideoID's of subscribed list.
std::vector<std::string> vec_browse_downloaded;         // VideoID's of downloads list.
std::vector<std::string> vec_browse_favorites;          // VideoID's of favorites list.

std::vector<std::string> vec_global_banned_instances;   // All banned instances
std::vector<std::string> vec_global_banned_channels;    // All bannen channels

std::vector<std::string> vec_subscribed_channels;       // Locally sourced list of subscribed channel ID's
std::vector<std::string> vec_downloaded_videos;         // Locally sourced list of downloaded video ID's
std::vector<std::string> vec_favorited_videos;          // Locally sourced list of favorited video ID's

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

bool create_folder ( const std::string& folderPath ) {
    if ( ! std::filesystem::exists(folderPath) ) {
        try {
            std::filesystem::create_directories(folderPath);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to create folder: " << folderPath << " - " << e.what() << std::endl;
            return false;
        }
    } else {
        return true;
    }
}

bool create_file ( const std::string& filePath ) {
    std::ifstream file(filePath);
    if ( ! file.good() ) {
        std::ofstream newFile(filePath);
        if ( newFile.good() ) {
            return true;
        } else {
            std::cerr << "Failed to create file: " << filePath << std::endl;
            return false;
        }
    } else {
        return true;
    }
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

// check if file contains string
bool find_string_in_file ( const std::string& filename, const std::string& content ) {
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        if (line == content) {
            return true; // String found in the file
        }
    }
    return false; // String not found in the file
}
// append string to file
int append_file ( const std::string& filename, const std::string& content, bool newline = false ) { // 0 = added, 1 = allready in file, 2 fail.
    if ( find_string_in_file(filename, content) ) {
        return 1;
    }

    std::ofstream file;
    file.open(filename, std::ios::app);
    if (file.is_open()) {
        if ( newline ) {
            file << content << "\n";
        } else {
            file << content;
        }
        file.close();
        return 0;
    } else {
        return 2;
    }
}
// remove line from file
void remove_matching_lines ( const std::string& filename, const std::string& pattern ) {
    std::ifstream inFile(filename);
    std::vector<std::string> lines;
    std::string line;

    while (std::getline(inFile, line)) {
        lines.push_back(line);
    }
    inFile.close();

    auto i = lines.begin();
    while (i != lines.end()) {
        if (i->find(pattern) != std::string::npos) {
            i = lines.erase(i);
        } else { ++i; }
    }

    std::ofstream outFile(filename);
    for (const std::string& updatedLine : lines) {
        outFile << updatedLine << '\n';
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
    if ( inv_instances_vector.size() >= 1 ) {
        log("Successfully added " + to_string_int(inv_instances_vector.size()) + " Instances.", 1);
        local_instances_updated = true;
    } else {
        log("Unable to update local instance list!", 4);
    }
}

// Update local instance list
void update_instances () {
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

// Input thread keeping track of inputs, and adding to queue.
void THREAD_input () {

    log("Started input thread.");
    struct termios old_tattr, new_tattr;
    tcgetattr(STDIN_FILENO,&old_tattr);
    new_tattr=old_tattr;
    new_tattr.c_lflag &=(~ICANON & ~ECHO);
    tcsetattr(STDIN_FILENO,TCSANOW,&new_tattr);

    while ( true ) { // Continous loop
        if ( collapse_threads == true ) {
            break;
        }
        if ( typing_mode == true ) {
            char_input_character=getchar();
            input_character = (int)char_input_character;
        } else {
            input_character=getchar();
        }
        add_key_input(input_character);
    }
    tcsetattr(STDIN_FILENO,TCSANOW,&old_tattr);
}

// Background worker and update thread.
void THREAD_background_worker () {

    int last_update_instances;

    update_instances(); // Update local instances.
    last_update_instances = epoch();
    log("WRK_THR: Instances updated.");

    while ( true ) {
        if ( ! inv_instances_vector.size() <= 1 ) {
            // instances loaded
            if ( epoch() >= last_update_instances + 300 ) {
                log("Instances not updated in 5 minutes, updating now...", 1);
                update_instances();
                last_update_instances = epoch();
            }
        } else { // Attempt again after 10 seconds.
            log("WRK_THR: Unable to update local instances.", 4);
            usleep(10000000); // 10s
            update_instances();
        }
        usleep(500000); // 0.5s sleep
    }
}

// input processing
void calculate_inputs () {

    if ( input_list.size() == 1 ) {
        if ( input_list[0] == 27 ) {
            if ( input_character == 27 ) {
                log("Received exit signal.");
                collapse_threads = true;
                return;
            }
        }
    }

    if ( ! input_list.size() == 0 ) {

        log("Items in input vector: " + to_string_int(input_list.size()));

        update_ui = true;

        for (unsigned i=0; i<input_list.size(); i++) { // Main check loop.
            if ( input_list.size() == 3 ) { // Check if arrow keys.
                if ( input_list[i] == 27 ){
                    i++;
                    if ( input_list[i] == 91 ){
                        i++;
                        if ( input_list[i] == 65 ) {
                            log("Arrow Up");
                        } else if ( input_list[i] == 66 ) {
                            log("Arrow Down");
                        } else if ( input_list[i] == 67 ) {
                            log("Arrow Right");
                            if ( current_menu == 1 ) {
                                if ( current_browse_type < 3 ) {
                                    ++current_browse_type;
                                    update_ui = true;
                                }
                            }
                        } else if ( input_list[i] == 68 ) {
                            log("Arrow Left");
                            if ( current_menu == 1 ) {
                                if ( ! current_browse_type < 1 ) {
                                    --current_browse_type;
                                    update_ui = true;
                                }
                            }
                        } else {
                            log("Unknown combination key!", 3);
                        }
                    } else {
                        log("Unknown combination key!", 3);
                    }
                    input_list.clear();
                    continue;
                }
            }
            log("Input vector iteration: " + to_string_int(i) + " Value: " + to_string_int(input_list[i]));
            if ( input_list[i] == 49 ) { current_menu = 0; } // Key 1 pressed / main menu
            else if ( input_list[i] == 50 ) { current_menu = 1; } // Key 2 pressed / main menu
            else if ( input_list[0] == 113 ) { quit = true; }

            else { log("Key unknown: " + to_string_int(input_list[0])); }
        }
        input_list.clear();
    } else {
        return;
    }
}

void draw_box ( int top_w, int top_h, int bot_w, int bot_h, bool title, int type, std::string title_str = "null" ) {
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
        9) Middle Box.
    */
    std::string character;
    int x; int y;
    bool skip = false;
    for ( x = top_h; x <= bot_h; ++x ) { // For each line downwords
        for ( y = top_w; y <= bot_w; ++y ) { // For each column rightward
            if ( x == top_h && y == top_w ) {
                // top left corner
                if ( type == 2 ) { character = "┬"; }
                else if ( type == 4 || type == 9 ) { character = "├"; }
                else { character = "╭"; }

            } else if ( x == top_h && y == bot_w ) {
                // top right corner
                if ( type == 1 ) { skip = true; }
                else if ( type == 4 || type == 9 ) { character = "┤"; }
                else { character = "╮"; }

            } else if ( x == top_h ) {
                // top bar
                character = "─";

            } else if ( x == bot_h && y == top_w ) {
                // bot left corner
                if ( type == 2 ) { character = "┴"; }
                else if ( type == 3 || type == 9 ) { skip = true; }
                else { character = "╰"; }

            } else if ( x == bot_h && y == bot_w ) {
                // bot right corner
                if ( type == 1 || type == 3 || type == 9 ) { skip = true; }
                else { character = "╯"; }

            } else if ( x == bot_h ) {
                // bot bar
                if ( type == 3 || type == 5 || type == 6 || type == 9 ) { skip = true; }
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
        std::cout << frame_color << "┨ " << frame_title_color << color_bold << title_str << frame_color << " ┠" << color_reset;
    }
}

void menu_item_main ( int w, int h ) {
    // Boxes: 2, top short fixed, bottom more info.

    bool large_canvas;
    if ( w < 120 || h < 40 ) {
        large_canvas = false;
    } else {
        large_canvas = true;
    }

    int vertical_border = 0;
    int horizontal_border = 0;

    int fixed_height = 7;

    int box1_top_w = horizontal_border + 1;
    int box1_bot_w = w - horizontal_border;
    int box1_top_h = vertical_border + 1;
    int box1_bot_h = 1 - vertical_border + fixed_height;

    draw_box( box1_top_w, box1_top_h, box1_bot_w, box1_bot_h, true, 3, "Main Menu" );

    if ( large_canvas ) { // 3 boxes, header main info and footer for small status details.
        int info_footer_height = 15;

        int large_box2_top_w = horizontal_border + 1;
        int large_box2_bot_w = w - horizontal_border;
        int large_box2_top_h = vertical_border + fixed_height + 1;
        int large_box2_bot_h = h - vertical_border - info_footer_height;

        draw_box( large_box2_top_w, large_box2_top_h, large_box2_bot_w, large_box2_bot_h, false, 9 );
        draw_box( large_box2_top_w, large_box2_bot_h, w - horizontal_border, h - vertical_border, false, 4 );
    } else { // 2 Boxes, top header and infobox under:
        int small_box2_top_w = horizontal_border + 1;
        int small_box2_bot_w = w - horizontal_border;
        int small_box2_top_h = vertical_border + fixed_height + 1;
        int small_box2_bot_h = h - vertical_border;

        draw_box( small_box2_top_w, small_box2_top_h, small_box2_bot_w, small_box2_bot_h, false, 4 );
    }
    
    //Version number
    if ( debug && large_canvas ) {
        printf("\033[%d;%dH", 2, 3);
        std::cout << "v" << version_number;
    }

    // Draw main menu logo
    int main_menu_logo_starting_pos = w / 2 - 27;
    std::stringstream main_menu_logo_title_stream;
    main_menu_logo_title_stream << color_blue << color_bold; // Set text type for logo
    std::string main_menu_logo_title_color = main_menu_logo_title_stream.str();
    int logo_array_size = sizeof(logo) / sizeof(logo[0]);

    for ( int l = 0; l < logo_array_size; ++l ) {
        printf("\033[%d;%dH", l + 2, main_menu_logo_starting_pos);
        std::cout << main_menu_logo_title_color << logo[l] << color_reset;
    }

    // Draw menu list
    int menu_list_info_height = fixed_height + 3;
    int menu_list_number_width = 5;
    int menu_list_title_width = 8;
    int menu_list_array_size = sizeof(menu_items) / sizeof(menu_items[0]);

    for ( int i = 0; i < menu_list_array_size; ++i ) {
        printf("\033[%d;%dH", menu_list_info_height + i, menu_list_number_width);
        std::cout << color_red << color_bold << i + 1 << color_reset << ",";
        printf("\033[%d;%dH", menu_list_info_height + i, menu_list_title_width);
        std::cout << color_green << menu_items[i] << color_reset; if ( i == 0 ) { std::cout << " (Current)"; }
    }
}

void menu_item_browse ( int w, int h ) {
    // Boxes: 2, top short fixed, bottom video list.
    
    int vertical_border = 0;
    int horizontal_border = 0;

    int fixed_height = 7;

    int box1_top_w = horizontal_border + 1;
    int box1_bot_w = w - horizontal_border;
    int box1_top_h = vertical_border + 1;
    int box1_bot_h = h - vertical_border + fixed_height;

    int box2_top_w = horizontal_border + 1;
    int box2_bot_w = w - horizontal_border;
    int box2_top_h = vertical_border + fixed_height + 1;
    int box2_bot_h = h - vertical_border;

    draw_box( box1_top_w, box1_top_h, box1_bot_w, box1_bot_h, true, 3, "Browse" );

    draw_box( box2_top_w, box2_top_h, box2_bot_w, box2_bot_h, true, 4, "< " + browse_types[current_browse_type] + " >" );
}

void draw_ui ( int w, int h ) {

    if ( current_menu == 0 ) { // 2 boxes on top of each other
        menu_item_main(w, h);
    } else if ( current_menu == 1 ) {
        menu_item_browse(w, h);
    }

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

    // Pre-Flight checks.
    if ( ! create_folder(userconfigdir) ) { std::cout << "Unable to create config directory: " << userconfigdir << "\n"; return 1; }
    if ( ! create_folder(configdir) ) { std::cout << "Unable to create config directory: " << configdir << "\n"; return 1; }
    if ( ! create_file(logfile) ) { std::cout << "Unable to create log file: " << logfile << "\n"; return 1; }
    if ( ! create_file(config_file_subscriptions) ) { std::cout << "Unable to create config file: " << config_file_subscriptions << "\n"; return 1; }
    if ( ! create_file(config_file_downloads) ) { std::cout << "Unable to create config file: " << config_file_downloads << "\n"; return 1; }
    if ( ! create_file(config_file_favorites) ) { std::cout << "Unable to create config file: " << config_file_favorites << "\n"; return 1; }
    if ( ! create_file(config_file_banned_instances) ) { std::cout << "Unable to create config file: " << config_file_banned_instances << "\n"; return 1; }
    if ( ! create_file(config_file_banned_channels) ) { std::cout << "Unable to create config file: " << config_file_banned_channels << "\n"; return 1; }


    /* Test Stuff
    append_file(config_file_favorites, "abc123xYz91", true);
    append_file(config_file_favorites, "uh5gd53uzaj", true);
    append_file(config_file_favorites, "hlggbvjbhgs", true);
    append_file(config_file_favorites, "5gj2l5o8g42", true);
    std::cout << "added 4 items, sleeping\n";
    usleep(20000000);
    std::cout << "Done sleeping, removing hlggbvjbhgs\n";
    remove_matching_lines(config_file_favorites, "hlggbvjbhgs");
    std::cout << "Removed line\n";

    return 0;
    */

    // Start process for updating local instances.
    std::thread background_thread(THREAD_background_worker);
    background_thread.detach();

    // Local UI Elements
    int tmp_w, tmp_h, w, h; // Used to store previous window size to detect changes.
    struct winsize size;

    std::cout << "\033c"; // Clear screen.
    std::cout << "\e[?25l"; // remove cursor

    std::thread input_thread(THREAD_input); // Start input thread

    setvbuf(stdout, NULL, _IONBF, 0); // Disables buffering

    while ( true ) { // Main loop

        if ( quit ) { // UI quit actions
            if ( current_menu == 0 ) { collapse_threads = true; }
            else if ( current_menu == 1 ) { current_menu = 0; }

            quit = false;
            update_ui = true;
        }

        if ( collapse_threads == true ) { // Stop processes and quit program
            log("Exiting main function", 1);
            break;
        }

        calculate_inputs();

        ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
        w = size.ws_col; h = size.ws_row;
        if ( w < 60 || h < 20 ) { 
            std::cout << "\033c";
            std::cout << "Terminal too small!\nCurrent: Width: " << w << " Heigth: " << h << "\n\nMore Needed: ";

            if ( w < 60 && h < 20 ) {
                std::cout << "Width: " << 60 - w << " Height: " << 20 - h;
            } else if ( w < 60 ) {
                std::cout << "Width: " << 60 - w;
            } else if ( h < 20 ) {
                std::cout << "Height: " << 20 - h;
            }
            std::cout << "\n\nTotal Needed: Width 60 Height 20";

            tmp_w = w;
            tmp_h = h;
            usleep(10000);
            continue;
        }

        if ( tmp_w != w || tmp_h != h || update_ui == true ) {
            std::cout << "\033c"; // Clear screen. Also reset.
            tmp_w = w;
            tmp_h = h;
            update_ui = false;

            // This is where everything is drawn to STDOut.
            draw_ui(w, h);

            std::cout << "\e[?25l"; // remove cursor
        }

        usleep(500);
    }

    fputs("\e[?25h", stdout); // Show cursor again.
    printf("\033[%d;%dH", h, 0); // move cursor to end of screen.
    std::cout << "\nPress any key to quit...";

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
