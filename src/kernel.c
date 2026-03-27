// IMPORTING LIBRARIES

// DEFINING STUFF THAT IS DEFINED IN STANDARD C LIBRARIES
typedef unsigned char BOOL;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

// IMPORT ASSEMBLY LIBS
extern void srand(unsigned int seed);
extern unsigned int rand(void);
extern unsigned int time(void);
extern void wait_cycles(unsigned int cycles);
unsigned long long measure_frequency(void);


// IMPORT C LIBS
#include "lib.h"
#include "io.h"
#include "keyb.h"
#include "FAT32.h"

#define true 1
#define false 0

#define WIDTH 80
#define HEIGHT 25

#define FAT_MAX_SIZE (4096 * 4)
static uint8_t fat_table[FAT_MAX_SIZE];

char textbuffer[HEIGHT * WIDTH] = {0}; // all zeroes, first char is '\0'
char usercommand[256];
volatile char* video = (volatile char*)0xB8000;

char* message;


void u_clear_screen() {
    volatile char* video = (volatile char*)0xB8000;
    for (int i = 0; i < HEIGHT * WIDTH; i++) {
        video[i * 2] = ' ';        // space character
        video[i * 2 + 1] = 0x07;   // normal attribute (light grey on black)
    }
}


void r_printraw(char* string, int pos) {
    for (int i = 0; string[i] != '\0'; i++) {
        if (string[i] == '\n') {
            // Move to start of next line
            pos += WIDTH - (pos % WIDTH);
        } else if (string[i] == '\t') {
            for (int i = 0; i < 4; i++) {
                video[pos * 2] = ' ';       // character
                video[pos * 2 + 1] = 0x07;         // color attribute
                pos++;
            }
        }
        else {
            video[pos * 2] = string[i];       // character
            video[pos * 2 + 1] = 0x07;         // color attribute
            pos++;
        }
    }
}


void u_print(char* string) {
    AppendSubstring(textbuffer, string);
    ClampSizeFromBeginning(textbuffer, WIDTH * HEIGHT);
    FixToLineAmount(textbuffer, HEIGHT);

    u_clear_screen();
    r_printraw(textbuffer, 0);
}


void u_readinput(char* buf) {
    copy_string(buf, usercommand);
}


void u_clearinputbuffer() {
    for (int j = 0; usercommand[j] != '\0'; j++) {
        usercommand[j] = '\0';
    }
}



void start_message() {
    unsigned int r = rand() % 6;

    static char possiblemessages[6][256] = {"\"Judgement!\"\n\n",
                                "\"Thy end is now!\"\n\n",
                                "\"Prepare thyself!\"\n\n",
                                "\"Forgive me my children, for I have failed to bring you salvation from this cold, dark world...\"\n\n",
                                "\"Ah... free at last. O Gabriel... now dawns thy reckoning, and thy gore shall glisten before the temples of man!\"\n\n",
                                "\"Creature of steel, my gratitude upon thee for my freedom. But the crimes thy kind have committed against humanity are not forgotten! And thy punishment... is death!\"\n\n"};


    message = possiblemessages[r];
    //u_print(message);
}


void printcommand(char* usercommand, char* textbuf, int offset) {
    u_clear_screen();
    r_printraw(message, 0);
    CodeBufferToAscii(usercommand, textbuf);
    r_printraw(textbuf, offset);
}


void kernel_main() {
    unsigned int seed = time();
    srand(seed);

    // initialize FAT32
    FAT32_FS fs;
    fs.fat_start = 1;          // LBA where FAT starts
    fs.data_start = 100;       // LBA where data region starts
    fs.sectors_per_cluster = 8;
    fs.bytes_per_sector = 512;
    fs.root_cluster = 2;       // usually 2
    fs.total_clusters = 4096;  // example
    fs.fat_table = fat_table;

    textbuffer[0] = '\0';  // Start empty
    u_clear_screen();

    start_message();

    char textbuf[20 * 85];

    int cursorptr = 0;

    int offset = (CountOccurrences(message, '\n') + 1 + (int)StringLength(message) / WIDTH) * WIDTH;

    uint8_t buffer[128];

    // Create folder
    fat32_create_folder(&fs, fs.root_cluster, "MYFOLDER");

    // Write file
    const char *msg = "Hello FAT32!";
    fat32_write_file_path(&fs, "MYFOLDER/HELLO.TXT", (const uint8_t*)msg, strlen_custom(msg));

    // Read file
    fat32_read_file_path(&fs, "MYFOLDER/HELLO.TXT", buffer, sizeof(buffer));
    buffer[16] = '\0';

    r_printraw(message, 0);

    /*
    while (true) {
        char typed = GetKeyboardInput();
        if (typed != 0 && typed != -1 && typed != '\n') { // normal char
            if (cursorptr < sizeof(usercommand) - 1) {
                usercommand[cursorptr] = typed;
                cursorptr++;
                usercommand[cursorptr] = '\0';  // Null-terminate after the new char
            }
            printcommand(usercommand, textbuf, offset);
        }
        else if (typed == -1) { // backspace
            if (cursorptr > 0) {
                cursorptr--;
                usercommand[cursorptr] = '\0';  // Remove last char
                printcommand(usercommand, textbuf, offset);
            }
        }
        else if (typed == '\n') {
            cursorptr = 0;

            for (int j = 0; usercommand[j] != '\0'; j++) {
                usercommand[j] = '\0';
            }

            printcommand(usercommand, textbuf, offset);
        }
    }
    */
}
