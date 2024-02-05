// Video Client Main

/*
https://en.cppreference.com/w/cpp/language/ascii

*/

// Libraries
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>

// Namespace
//using namespace std;

// Global Variables
bool logging = true;
bool logfile = true;

unsigned char input_character; // Input character.

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

void input () {
    struct termios old_tattr, new_tattr;
    tcgetattr(STDIN_FILENO,&old_tattr);
    new_tattr=old_tattr;
    new_tattr.c_lflag &=(~ICANON & ~ECHO);
    tcsetattr(STDIN_FILENO,TCSANOW,&new_tattr);
    // Continous loop
    while ( input_character != 'q' ) {
        input_character=getchar();
        //std::cout << "Input: " << input_character << "\n";
        //printf("%d ",c);
    }
    tcsetattr(STDIN_FILENO,TCSANOW,&old_tattr);
}

int main() {

    log("Very long and complicated log message!!!");
    log("Warning message!", 1);
    log("Errormessage!", 2);
    log("FATALITY!!!", 3);

    std::string printmessage;

    printmessage = color_red + "message is red" + color_reset + "\n";
    std::cout << printmessage;
    printmessage = color_blue + "message is blue" + color_reset + "\n";
    std::cout << printmessage;
    printmessage = color_cyan + "message is cyan" + color_reset + "\n";
    std::cout << printmessage;
    printmessage = color_green + "message is green" + color_reset + "\n";
    std::cout << printmessage;
    printmessage = color_yellow + "message yellow" + color_reset + "\n";
    std::cout << printmessage;

    std::cout << "morestuff\n";

    input();

    std::cout << "Test\n";

    return 0;
}
