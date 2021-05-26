#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"


#define ASCII_0_VALU 48
#define ASCII_9_VALU 57
#define ASCII_A_VALU 65
#define ASCII_F_VALU 70



void
helpMenu(){
    printf("\nUse this program to change current terminal colours.\n");
    printf("Usage: colour [OPTION]...\n");

    printf("Command line options:\n");
    printf("         -h, --help: Show help promt.\n");
    printf("         -fg, --foreground: Change text colour.\n");
    printf("         -bg, --backgroung: Change background colour.\n");
    printf("         reset: Reset to default xv6 colours.\n");
    printf("         [without parameters]: Set background and foreground colours.\n");


}


int
atou(const char *s)
{
	int n;

	n = 0;
    *s++;
    *s++;

	while('0' <= *s && *s <= '9' || 'a' <= *s && *s <= 'z' || 'A' <= *s && *s <= 'Z'){
        n <<= 4;
        if (*s >= '0' && *s <= '9')
            n +=  *s++ - '0';
        else if(*s >= 'A' && *s <= 'F')
            n +=  *s++ - 'A' + 10;
        else if(*s >= 'a' && *s <= 'f')
            n +=  *s++ - 'a' + 10;
        }
	return n;
}





int
main(int argc, char *argv[])
{
    int i;
    char* fg;
    char* bg;

    if(argc < 2){
        helpMenu();
        exit();
    }

    if(argc == 2){
        if(!strcmp(argv[1], "reset")){
            colour(0x07, 0);
        }else if(!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")){
            helpMenu();
        }
        else{
            colour(atou(argv[1]), 0);
        }
    }
    //* “black”, “blue”, “green”,
    //* “aqua”, “red”, “purple”, “yellow”, ili “white”
    for(i = 1; i < argc; i++){
         if(!strcmp(argv[i], "-bg") || !strcmp(argv[i], "--background")){
            bg = argv[++i];
            if(!strcmp(bg, "black")){
                colour(0x00, 1);
            }else if(!strcmp(bg, "blue")){
                colour(0x10, 1);
            }else if(!strcmp(bg, "green")){
                colour(0x20, 1);
            }else if(!strcmp(bg, "aqua")){
                colour(0x30, 1);
            }else if(!strcmp(bg, "red")){
                colour(0x40, 1);
            }else if(!strcmp(bg, "purple")){
                colour(0x50, 1);
            }else if(!strcmp(bg, "yellow")){
                colour(0x60, 1);
            }else if(!strcmp(bg, "white")){
                colour(0x70, 1);
            }else if(!strcmp(bg, "Lblack")){
                colour(0x80, 1);
            }else if(!strcmp(bg, "Lblue")){
                colour(0x90, 1);
            }else if(!strcmp(bg, "Lgreen")){
                colour(0xA0, 1);
            }else if(!strcmp(bg, "Laqua")){
                colour(0xB0, 1);
            }else if(!strcmp(bg, "Lred")){
                colour(0xC0, 1);
            }else if(!strcmp(bg, "Lpurple")){
                colour(0xD0, 1);
            }else if(!strcmp(bg, "Lyellow")){
                colour(0xE0, 1);
            }else if(!strcmp(bg, "Lwhite")){
                colour(0xF0, 1);
            }
        }
        if(!strcmp(argv[i], "-fg") || !strcmp(argv[i], "--foreground")){
            fg = argv[++i];
            if(!strcmp(fg, "black")){
                colour(0x00, 2);
            }else if(!strcmp(fg, "blue")){
                colour(0x01, 2);
            }else if(!strcmp(fg, "green")){
                colour(0x02, 2);
            }else if(!strcmp(fg, "aqua")){
                colour(0x03, 2);
            }else if(!strcmp(fg, "red")){
                colour(0x04, 2);
            }else if(!strcmp(fg, "purple")){
                colour(0x05, 2);
            }else if(!strcmp(fg, "yellow")){
                colour(0x06, 2);
            }else if(!strcmp(fg, "white")){
                colour(0x07, 2);
            }else if(!strcmp(fg, "Lblack")){
                colour(0x08, 2);
            }else if(!strcmp(fg, "Lblue")){
                colour(0x09, 2);
            }else if(!strcmp(fg, "Lgreen")){
                colour(0x0A, 2);
            }else if(!strcmp(fg, "Laqua")){
                colour(0x0B, 2);
            }else if(!strcmp(fg, "Lred")){
                colour(0x0C, 2);
            }else if(!strcmp(fg, "Lpurple")){
                colour(0x0D, 2);
            }else if(!strcmp(fg, "Lyellow")){
                colour(0x0E, 2);
            }else if(!strcmp(fg, "Lwhite")){
                colour(0x0F, 2);
            }

        }

    }



	exit(); // mora exit na kraju poziva
}
