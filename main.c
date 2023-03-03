#include "functions.h"

int main() {
    initscr();
    keypad(stdscr, TRUE);
    noecho();
    cbreak();

    manage_color();

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        char error[] = "getcwd() error";
        handle_error(error);
    }

    while (1) {
        getmaxyx(stdscr, max_rows, max_cols);
        max_rows -= 2;
        move(0,0);

        print_header();
        print_directories();

        refresh();

        int call = getch();
        switch (call) {
            case KEY_DOWN:
                if (current_file == n_files - 1) {
                    current_file = 0;
                    start_row = 0;
                    end_row = max_rows;
                } else {
                    if (current_file == end_row - 1) {
                        start_row++;
                        end_row++;
                    }
                    current_file++;
                }
                break;
            case KEY_UP:
                if (current_file != 0) {
                    if (current_file == start_row) {
                        start_row--;
                        end_row--;
                    }
                    current_file--;
                } else {
                    current_file = n_files - 1;
                    end_row = n_files;
                    if (n_files < max_rows) {
                        start_row = 0;
                    } else {
                        start_row = n_files - max_rows;
                    }
                }
                break;
            case '\n':
                process_enter(pointed_directory);
                clear();
                break;
            case (int)'q':
                endwin();
                return 0;
            case (int)'D':
                delete_file();
                break;
            case (int)'H':
                change_view_mod();
                break;
            case (int)'C':
                copy_file();
                break;
            case (int)'V':
                paste_file();
                break;
            case (int)'X':
                cut_file();
                break;
            default:
                break;
        }
    }
}