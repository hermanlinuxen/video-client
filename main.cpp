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
#include <algorithm>

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
bool browse_opened = false;
int current_list_item = 0;
int list_shift = 0;

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
struct inv_videos{                      // invidious videos
    // Active variables
    std::string URL;                    // 11 character video identifier
    int published;                      // epoch time of release date
    int lengthseconds;                  // video Length in seconds
    int viewcount;                      // video views
    int downloaded_time = 0;            // epoch time when video was downloaded
    std::string title;                  // video title
    std::string publishedtext;          // release date converted to human readable format?
    std::string author;                 // video creator
    std::string author_id;              // ID of video creator
    std::string description;            // video description.
    bool favorite = false;              // if video is favorite
    bool downloaded = false;            // if video is downloaded or not
    bool currently_downloading = false; // in video is currently downloading
    bool manual_update = false;         // if video has been manually updated
};
std::vector<inv_videos> inv_videos_vector;

struct inv_instances{
    bool enabled;               // if the program is going to use this instance
    bool api_enabled;           // if the API is enabled for this instance
    bool banned = false;        // if the instance is banned by user
    bool updated = false;       // if the instance has been updated by update function.
    std::string name;           // instance name, got from top of array
    std::string URL;            // URL
    std::string type;           // https or other
    int last_get = 0;           // ignored and 0 if enabled, if unreachable, retry after 10 minutes. Apply current epoch to this int.
    int last_update_popular;    // last time popular was updated
    int health;                 // instance health 90d as int
    std::string region;         // instance region
};
std::vector<inv_instances> inv_instances_vector;

std::vector<std::string> vec_browse_popular;            // VideoID's of popular list sorted.
std::vector<std::string> vec_browse_subscriptions;      // VideoID's of subscribed list sorted.
std::vector<std::string> vec_browse_downloaded;         // VideoID's of downloads list sorted.
std::vector<std::string> vec_browse_favorites;          // VideoID's of favorites list sorted.

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

// Random number
int random_number ( const int min, const int max ) { // Can return both the min and max values, aswell as every in between.
    return rand()%(max-min + 1) + min;
}

// From range to range
int convert_range(std::pair<double, double> from_range, double from_value, std::pair<double, double> to_range) {
    double ratio = (from_value - from_range.first) / (from_range.second - from_range.first);
    double ratioThousand = ratio * 1000;
    double to_value = to_range.first + (ratioThousand / 1000) * (to_range.second - to_range.first);
    return (int)to_value;
}

// Convert seconds to video length format
std::string seconds_to_list_format ( int seconds ) {
    int hours = seconds / 3600;
    seconds %= 3600;
    int minutes = seconds / 60;
    seconds %= 60;
    std::stringstream ss;
    if ( hours > 0 ) {
        ss << hours << 'h';
        if ( hours < 10 ) { ss << ':'; }
        if ( minutes < 10 ) { ss << '0'; }
        ss << minutes;
    } else {
        if ( minutes < 10 ) { ss << '0'; }
        ss << minutes << ':';
        if ( seconds < 10 ) { ss << '0'; }
        ss << seconds;
    }
    std::string result = ss.str();
    if (result.length() > 5)
        return "*****"; // Overflow, return asterisks
    return result;
}

// Converts seconds to time since upload format
std::string uploaded_format(int seconds) {
    std::pair<std::string, int> intervals[] = {
        {"s", 60},
        {"m", 60},
        {"h", 24},
        {"d", 30},
        {"y", 12}
    };
    int result = seconds;
    std::string unit = "s";
    for (const auto& interval : intervals) {
        if (result < interval.second) {
            return std::to_string(result) + unit;
        }
        result /= interval.second;
        unit = interval.first;
    }
    return std::to_string(result) + "y";
}

// Truncate String
std::string truncate(const std::string& input, int max) {
    if (input.length() <= max) {
        return input;
    } else {
        return input.substr(0, max - 3) + "...";
    }
}

// Get videoid from main video vector
std::pair<bool, int> get_videoid_from_vector ( std::string id ) {
    for ( int i = 0; i < inv_videos_vector.size(); ++i ) {
        if ( inv_videos_vector[i].URL == id ) {
            return std::make_pair(true, i);
        }
    }
    return std::make_pair(false, 0);
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
        bool api_enabled = entry[1]["api"];
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
            inv_instances_vector[inv_instances_vector_iteration].api_enabled = api_enabled;
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

void update_instance_info (const int instance) {
    bool skip = false;
    log("Updating instance information for: " + inv_instances_vector[instance].name);
    // Check if instance can be skipped
    if ( inv_instances_vector[instance].enabled == false ) { skip = true; }
    if ( inv_instances_vector[instance].api_enabled == false ) { skip = true; }

    // Update information:
    if ( ! skip ) {
        if ( find_string_in_file(config_file_banned_instances, inv_instances_vector[instance].name) ) {
            log("Banning instance: " + inv_instances_vector[instance].name);
            inv_instances_vector[instance].banned = true;
        }
        if ( ! ( inv_instances_vector[instance].type == "https" )) {
            inv_instances_vector[instance].enabled = false;
        }
    }

    inv_instances_vector[instance].updated = true;
}

// Update popular list
bool update_browse_popular ( int instance ) { // https://instance.name/api/v1/popular
    json data;
    std::stringstream url;
    url << "https://" << inv_instances_vector[instance].name << "/api/v1/popular";
    std::string url_string = url.str();
    auto result = fetch(url_string);
    if ( result.first ) {
        try {
            data = json::parse(result.second);
        } catch (const std::exception& e) {
            std::stringstream parse_result;
            parse_result << e.what();
            log("Error parsing JSON: " + parse_result.str(), 3);
            return false;
        }
    } else {
        log("Curl is unable to contact API: " + url_string, 3);
        return false;
    }
    if ( ! data.is_array() ) {
        log("Instance does not support popular? " + inv_instances_vector[instance].name);
        return false;
    }

    std::vector<std::string> vec_browse_popular_temp; // init temporary video vector

    for (const auto& item : data) { // for each video in popular list

        std::string videoid =   item["videoId"];
        std::string title =     item["title"];
        int length =            item["lengthSeconds"];
        int published =         item["published"];
        int viewcount =         item["viewCount"];
        std::string author =    item["author"];
        std::string author_id = item["authorId"];

        auto video_in_list = get_videoid_from_vector(videoid);
        int end_of_list = inv_videos_vector.size();

        if ( video_in_list.first ) {
            int video = video_in_list.second;
            inv_videos_vector[video].title = title;
            inv_videos_vector[video].author = author;
            inv_videos_vector[video].author_id = author_id;
            inv_videos_vector[video].lengthseconds = length;
            inv_videos_vector[video].published = published;
            inv_videos_vector[video].viewcount = viewcount;
        } else {
            inv_videos_vector.push_back(inv_videos());
            inv_videos_vector[end_of_list].URL = videoid;
            inv_videos_vector[end_of_list].title = title;
            inv_videos_vector[end_of_list].author = author;
            inv_videos_vector[end_of_list].author_id = author_id;
            inv_videos_vector[end_of_list].lengthseconds = length;
            inv_videos_vector[end_of_list].published = published;
            inv_videos_vector[end_of_list].viewcount = viewcount;
        }
        // Add to temporary vector
        vec_browse_popular_temp.push_back(videoid);
    }

    // merge to temp
    bool merge_skip;
    int popular_iterator_count = vec_browse_popular.size();
    for ( int i = 0; i < popular_iterator_count; ++i ) {
        merge_skip = false;
        for ( int used_in_pop_i = 0; used_in_pop_i < vec_browse_popular_temp.size(); ++used_in_pop_i ) {
            if ( vec_browse_popular_temp[used_in_pop_i] == vec_browse_popular[i] ) {
                merge_skip = true;
                break; // Video allready in temp, duplicate
            }
        }
        if ( ! merge_skip ) {
            vec_browse_popular_temp.push_back(vec_browse_popular[i]);
        }
    }

    std::vector<std::string> vec_browse_popular_sorted; // init temporary sorted vector, sorted largest numbers first

    // vec_browse_popular_temp contains all videoIDs needed to sort.
    int sort_count = vec_browse_popular_temp.size();

    int largest = 0;
    int winner;
    int epoch_release;
    int id;
    bool skip;

    for ( int sort_i = 0; sort_i < sort_count; ++sort_i ) {
        // Add largest epoch found to first available element in vec_browse_popular_sorted, and remove it from temp vector.
        largest = 0;
        winner = 0;

        for ( int current_i = 0; current_i < vec_browse_popular_temp.size(); ++current_i ) {
            skip = false;
            for ( int used_i = 0; used_i < vec_browse_popular_sorted.size(); ++used_i ) { // check if current has been used.
                if ( vec_browse_popular_sorted[used_i] == vec_browse_popular_temp[current_i] ) {
                    skip = true;
                }
            }
            if ( skip ) {
                continue;
            }
            auto id_result = get_videoid_from_vector(vec_browse_popular_temp[current_i]);
            if ( ! id_result.first ) {
                log("VideoID missing from main list" + vec_browse_popular_temp[current_i], 4);
                continue;
            }
            epoch_release = inv_videos_vector[id_result.second].published;
            if ( epoch_release > largest ) {
                largest = epoch_release;
                winner = current_i;
            }
        }
        // Add largest to sorted list
        vec_browse_popular_sorted.push_back(vec_browse_popular_temp[winner]);
    }
    vec_browse_popular.clear();
    for ( int add_i = 0; add_i < vec_browse_popular_sorted.size(); ++add_i ) {
        vec_browse_popular.push_back(vec_browse_popular_sorted[add_i]);
    }
    return true;
}

// Add key int to key list vector.
void add_key_input ( int key = 0) {
    if ( ! key == 0 ) {
        input_list.push_back(key);
    }
}

// Background worker and update thread.
void THREAD_background_worker () {

    int last_update_instances;

    update_instances(); // Update local instances.
    last_update_instances = epoch();
    log("WRK_THR: Instances updated.");

    while ( true ) {
        if ( collapse_threads ) { break; }
        if ( inv_instances_vector.size() != 0 ) {
            // instances loaded
            if ( epoch() >= last_update_instances + 600 ) {
                log("Instances not updated in 10 minutes, updating now...", 1);
                update_instances();
                last_update_instances = epoch();
            }
            for ( int i_instance = 0; i_instance < inv_instances_vector.size(); ++i_instance ) {
                if ( ! inv_instances_vector[i_instance].updated ) {
                    update_instance_info(i_instance);
                }
                if ( ( inv_instances_vector[i_instance].enabled && inv_instances_vector[i_instance].api_enabled ) && ( inv_instances_vector[i_instance].banned == false ) ) {
                    // Instance loop for enabled and unbanned instances.

                    if ( browse_opened ) {
                        if ( epoch() >= inv_instances_vector[i_instance].last_update_popular + 300 ) { // If last update time is more than 5 minutes.
                            if ( update_browse_popular(i_instance) ) {
                                update_ui = true;
                                log("Popular updated from instance: " + inv_instances_vector[i_instance].name + " refreshing this instance in 5-10 minutes");
                                inv_instances_vector[i_instance].last_update_popular = epoch() + random_number(0, 300); // Add random delay to update cycle
                            } else {
                                log("Popular update failed for instance, waiting 10-20 minutes: " + inv_instances_vector[i_instance].name);
                                inv_instances_vector[i_instance].last_update_popular = epoch() + random_number(600, 1200); // Add 10-20 minutes until next check.
                            }
                        }
                    }
                }
            }

        // ToDo: For each loop, walk trough 1 video at a time, to update any possibly missing information.
        // Check manual_update key
        // Known missing: Description. Check API for other values: https://docs.invidious.io/api/#get-apiv1videosid

        } else { // Attempt again after 10 seconds.
            log("WRK_THR: Unable to update local instances.", 4);
            usleep(60000000); // 60s
            update_instances();
        }
        usleep(100000); // 0.1s sleep
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

// input processing
void calculate_inputs () {

    if ( input_list.size() == 1 ) {
        usleep(5000);
        if ( input_list.size() == 1 ) {
            if ( input_list[0] == 27 ) {
                if ( input_character == 27 ) {
                    log("Received exit signal.");
                    collapse_threads = true;
                    return;
                }
            }
        }
    }

    if ( ! input_list.size() == 0 ) {

        //log("Items in input vector: " + to_string_int(input_list.size()));

        update_ui = true;
        int list_item_limit = 0;

        for (unsigned i=0; i<input_list.size(); i++) { // Main check loop.
            if ( input_list.size() == 3 ) { // Check if arrow keys.
                if ( input_list[i] == 27 ){
                    i++;
                    if ( input_list[i] == 91 ){
                        i++;
                        if ( input_list[i] == 65 ) {
                            if ( current_menu == 1 ) {
                                if ( ! ( current_list_item <= 0 )) {
                                    --current_list_item;
                                }
                            }
                        } else if ( input_list[i] == 66 ) {
                            if ( current_menu == 1 ) {
                                if ( current_browse_type == 0 ) {
                                    list_item_limit = vec_browse_popular.size() - 1;
                                }

                                if ( current_list_item < list_item_limit ) {
                                    ++current_list_item;
                                }
                            }
                        } else if ( input_list[i] == 67 ) {
                            if ( current_menu == 1 ) {
                                if ( current_browse_type < 3 ) {
                                    ++current_browse_type;
                                    current_list_item = 0;
                                    update_ui = true;
                                }
                            }
                        } else if ( input_list[i] == 68 ) {
                            if ( current_menu == 1 ) {
                                if ( ! current_browse_type < 1 ) {
                                    --current_browse_type;
                                    current_list_item = 0;
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
            if ( input_list[i] == 49 ) { current_menu = 0; current_list_item = 0; } // Key 1 pressed / main menu
            else if ( input_list[i] == 50 ) { current_menu = 1; current_list_item = 0; } // Key 2 pressed / main menu
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

void draw_list_popular ( int top_w, int top_h, int bot_w, int bot_h ) {

    int list_length = bot_h - top_h + 1;

    int scrollbar_length = bot_h - top_h + 3;
    std::string scrollbar_character;
    std::pair<double, double> from_vidlist = std::make_pair(0, vec_browse_popular.size());
    std::pair<double, double> to_scroll = std::make_pair(0, scrollbar_length);
    double from_scrollbar_value = current_list_item;
    int scrollbar_handle = convert_range(from_vidlist, from_scrollbar_value, to_scroll);
    if ( scrollbar_handle <= 1 ) { // Stop scrollbar from clipping
        scrollbar_handle = 1;
    } else if ( scrollbar_handle >= scrollbar_length - 2 ) {
        scrollbar_handle = scrollbar_length - 2;
    }

    for ( int scrollbar = 0; scrollbar < scrollbar_length; ++scrollbar ) {
        printf("\033[%d;%dH", top_h + scrollbar - 1, bot_w + 1 );
        if ( scrollbar == 0 ) {
            scrollbar_character = "┳";
        } else if ( scrollbar == scrollbar_length - 1 ) {
            scrollbar_character = "┻";
        } else if ( scrollbar == scrollbar_handle ) {
            scrollbar_character = "█";
        } else {
            scrollbar_character = "┃";
        }
        std::cout << color_cyan << scrollbar_character << color_reset;
    }

    /*
        What to display: ( . dot = whitespace)
        Title - Should be 40 Characters. Colors: White normal, green downloaded, yellow downloading red failed download. Cutting required
        Video Length - mm:ss, if 1+ Hour: 1H-12, 2H-36 etc. If 10+ Hours: 10 H
        Creator - 20 Characters?. Cutting required
        When video was released. Short: (4 Characters) ..1H, .14H, ..1D, 73D, 123D, ..3Y Long: (8 Characters) .45 Mins, ..1 Hour, 17 Hours, ...1 Day, .40 Days, 296 Days, ..1 Year, .4 Years
        Views (10 Chars) Short (4): 127K, .15K, 266M, .138 Long: (10) 946K Views, .15K Views, ...1 View.
        Star at the end for favorite. ✦
    */
    int last_item = vec_browse_popular.size();
    int list_shown_item = 0;
    std::string title;
    std::string author;
    std::string views;
    std::string released;
    std::string length;
    bool favorite;
    bool wide_list = true; // Test

    int pos_star = bot_w - 5;
    int pos_views;
    int pos_released;

    if ( wide_list ) {
        pos_views = bot_w - 16;
        pos_released = bot_w - 25;
    } else {
        pos_views = bot_w - 10;
        pos_released = bot_w - 15;
    }

    if ( current_list_item == 0 ) { list_shift = 0; }
    if ( current_list_item - list_shift >= list_length - 5 ) {
        if ( ! ( current_list_item + 5 >= last_item )) {
            ++list_shift;
        }
    } else if ( current_list_item - list_shift <= 4 ) {
        if ( current_list_item >= 5 ) {
            --list_shift;
        }
    }

    for ( int line = 0; line < list_length; ++line ) { // Each menu list, line iterated downwards.
        if ( vec_browse_popular.size() != 0 ) {
            //std::string videoid_test = vec_browse_popular[list_shown_item];
            auto video_vector_number = get_videoid_from_vector(vec_browse_popular[list_shown_item + list_shift]);
            title = inv_videos_vector[video_vector_number.second].title;
            author = inv_videos_vector[video_vector_number.second].author;
            favorite = inv_videos_vector[video_vector_number.second].favorite;
            length = seconds_to_list_format(inv_videos_vector[video_vector_number.second].lengthseconds);
            //released = uploaded_format(epoch() - inv_videos_vector[video_vector_number.second].published);
            released = to_string_int(epoch() - inv_videos_vector[video_vector_number.second].published);

            /*if ( wide_list ) { // 10 Long
                views = "946K Views";
                released = "296 Days";
            } else { // 4 Long
                views = "127K";
                released = "123D";
            }*/

        } else {
            title = "Loading...";
        }

        if ( current_list_item == line + list_shift) {
            printf("\033[%d;%dH", top_h + line, top_w - 2);
            std::cout << color_red << color_bold << "▶" << color_reset;
            printf("\033[%d;%dH", top_h + line, bot_w - 2);
            std::cout << color_red << color_bold << "◀" << color_reset;
        }
        printf("\033[%d;%dH", top_h + line, top_w + 1); // Title
        std::cout << truncate(title, 40);

        printf("\033[%d;%dH", top_h + line, top_w + 42); // Video Length
        std::cout << color_blue << length << color_reset;
        /*
        printf("\033[%d;%dH", top_h + line, top_w + 48); // author
        std::cout << truncate(author, 20);*/

        printf("\033[%d;%dH", top_h + line, pos_released); // released
        std::cout << released;

        printf("\033[%d;%dH", top_h + line, pos_views); // Views
        std::cout << views;

        if ( favorite ) {
            printf("\033[%d;%dH", top_h + line, pos_star); // Star
            std::cout << color_yellow << "✦" << color_reset;
        }


        if ( vec_browse_popular.size() == 0 ) { break; }
        ++list_shown_item;
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

    // Sats:
    printf("\033[%d;%dH", 2, 3 + 1);
    std::cout << current_list_item + 1 << " / " << vec_browse_popular.size() << " Videos";

    if ( current_browse_type == 0 ) { // Popular
        int list_top_w = 5;
        int list_bot_w = w - 4;
        int list_top_h = fixed_height + 3;
        int list_bot_h = h - 2;
        draw_list_popular(list_top_w, list_top_h, list_bot_w, list_bot_h);
    }
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

    // Source configs for banned channels and instances
    std::string line;
    std::ifstream config_file_banned_channels_file(config_file_banned_channels);
    std::ifstream config_file_banned_instances_file(config_file_banned_instances);
    std::ifstream config_file_subscriptions_file(config_file_subscriptions);
    std::ifstream config_file_downloads_file(config_file_downloads);
    std::ifstream config_file_favorites_file(config_file_favorites);
    while (std::getline(config_file_banned_channels_file, line)) { log("Adding banned channel: " + line); vec_global_banned_channels.push_back(line); }
    while (std::getline(config_file_banned_instances_file, line)) { log("Adding banned instance: " + line); vec_global_banned_instances.push_back(line); }
    while (std::getline(config_file_subscriptions_file, line)) { log("Adding subscriptions: " + line); vec_subscribed_channels.push_back(line); }
    while (std::getline(config_file_downloads_file, line)) { log("Adding downloads: " + line); vec_downloaded_videos.push_back(line); }
    while (std::getline(config_file_favorites_file, line)) { log("Adding favorites: " + line); vec_favorited_videos.push_back(line); }

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

        if ( ( current_menu == 1 ) && ( ! browse_opened ) ) {
            browse_opened = true;
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

    // Test:
    log("Videos in popular: " + to_string_int(vec_browse_popular.size()));
    for ( int test_i = 0; test_i < vec_browse_popular.size(); ++test_i ) {
        auto test_video_vector_number = get_videoid_from_vector(vec_browse_popular[test_i]);
        std::string test_video_vector_id = inv_videos_vector[test_video_vector_number.second].URL;
        int test_video_vector_epoch = inv_videos_vector[test_video_vector_number.second].published;
        log("TEST! ID: " + test_video_vector_id + " Epoch: " + to_string_int(epoch() - test_video_vector_epoch));
    }

    fputs("\e[?25h", stdout); // Show cursor again.
    printf("\033[%d;%dH", h, 0); // move cursor to end of screen.
    std::cout << "\nPress any key to quit...";

    collapse_threads = true;

    if ( interrupt ) {
        log("Interrupt signal received", 4);
        std::cout << "\nInterrupt!\n";
        return 1;
    }

    input_thread.join();

    std::cout << "\nExiting...\n";

    return 0;
}
