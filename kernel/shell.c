#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "fat.h"
#include "timer.h"
#include "rtc.h"
#include "power.h"
#include "string.h"
#include <stddef.h>
#include <stdint.h>

#define INPUT_MAX 512
#define BUF_MAX   4096

/* ---- Helpers -------------------------------------------------------- */
static void print_prompt(void) {
    char cwd[FAT_PATH_MAX];
    fat_getcwd(cwd, FAT_PATH_MAX);
    vga_set_fg(VGA_BLUE);
    vga_puts("AquaOS @ ");
    vga_puts(cwd);
    vga_puts(": ");
    vga_set_fg(VGA_WHITE);
}

static void read_line(char *buf, size_t sz) {
    keyboard_readline(buf, sz);
}

/* ---- Command implementations ---------------------------------------- */

static void cmd_help(void) {
    vga_set_fg(VGA_YELLOW);
    vga_puts("about = os information\n");
    vga_puts("help = list of commands\n");
    vga_puts("shutdown = shuts down system\n");
    vga_puts("mkfile (filename) = creates a file\n");
    vga_puts("mkdir (dirname) = makes a directory\n");
    vga_puts("deldir (dirname) = deletes a directory\n");
    vga_puts("delfile (filename) = deletes a file\n");
    vga_puts("readfile (filename) = reads from file\n");
    vga_puts("ls = lists files and directories\n");
    vga_puts("cd (dirname) = changes directory\n");
    vga_puts("cd\\ = returns to 0:\\ directory\n");
    vga_puts("clear = clears the screen\n");
    vga_puts("echo (text) = echoes user input\n");
    vga_puts("gettime = displays date and time\n");
    vga_puts("run (program) = runs an aquapp\n");
    vga_puts("\n");
    vga_puts("Included applications: \n");
    vga_puts("Scribbler: A text file editor written in 8 lines of code!\n");
    vga_puts("To use, run the command: scribble\n");
    vga_puts("AppWithInfo: A demonstrational aquapp that contains info\n");
    vga_puts("HelloUser: A demonstrational aquapp that says hello to the user\n");
    vga_set_fg(VGA_WHITE);
}

static void cmd_about(void) {
    vga_set_fg(VGA_CYAN);
    vga_puts("AquaOS version 0.40 by Kai Howard\n");
    vga_set_fg(VGA_WHITE);
}

static void cmd_clear(void) {
    vga_clear();
}

static void cmd_gettime(void) {
    vga_set_fg(VGA_RED);
    rtc_print();
    vga_set_fg(VGA_WHITE);
}

static void cmd_echo(const char *text) {
    vga_puts(text);
    vga_putchar('\n');
}

static void cmd_creator(void) {
    vga_clear();
    vga_set_fg(VGA_BROWN);
    vga_puts("          _____                    _____                    _____  \n");
    vga_puts("         /\\    \\                  /\\    \\                  /\\    \\ \n");
    vga_puts("        /::\\____\\                /::\\    \\                /::\\    \\\n");
    vga_puts("       /:::/    /               /::::\\    \\               \\:::\\    \\\n");
    vga_puts("      /:::/    /               /::::::\\    \\       .        \\:::\\    \\\n");
    vga_puts("     /:::/    /               /:::/\\:::\\    \\               \\:::\\    \\\n");
    vga_puts("    /:::/____/               /:::/__\\:::\\    \\               \\:::\\    \\\n");
    vga_puts("   /::::\\    \\              /::::\\   \\:::\\    \\              /::::\\    \\\n");
    vga_puts("  /::::::\\____\\________    /::::::\\   \\:::\\    \\    ____    /::::::\\    \\\n");
    vga_puts(" /:::/\\:::::::::::\\    \\  /:::/\\:::\\   \\:::\\    \\  /\\   \\  /:::/\\:::\\    \\\n");
    vga_puts("/:::/  |:::::::::::\\____\\/:::/  \\:::\\   \\:::\\____\\/::\\   \\/:::/  \\:::\\____\\\n");
    vga_puts("\\::/   |::|~~~|~~~~~     \\::/    \\:::\\  /:::/    /\\:::\\  /:::/    \\::/    / \n");
    vga_puts(" \\/____|::|   |           \\/____/ \\:::\\/:::/    /  \\:::\\/:::/    / \\/____/ \n");
    vga_puts("       |::|   |                    \\::::::/    /    \\::::::/    /\n");
    vga_puts("       |::|   |                     \\::::/    /      \\::::/____/\n");
    vga_puts("       |::|   |                     /:::/    /        \\:::\\    \\ \n");
    vga_puts("       |::|   |                    /:::/    /          \\:::\\    \\\n");
    vga_puts("       |::|   |                   /:::/    /            \\:::\\    \\\n");
    vga_puts("       \\::|   |                  /:::/    /              \\:::\\____\\\n");
    vga_puts("        \\:|   |                  \\::/    /                \\::/    / \n");
    vga_puts("         \\|___|                   \\/____/                  \\/____/ \n");
    vga_set_fg(VGA_WHITE);
}

/* ---- ls callback ---------------------------------------------------- */
static void ls_cb(const fat_dirent_t *e, void *arg) {
    (void)arg;
    if (e->attr & FAT_ATTR_DIR) {
        vga_set_fg(VGA_GREEN);
    } else {
        vga_set_fg(VGA_CYAN);
    }
    vga_puts(e->name);
    vga_putchar('\n');
    vga_set_fg(VGA_WHITE);
}

static void cmd_ls(void) {
    char cwd[FAT_PATH_MAX];
    fat_getcwd(cwd, FAT_PATH_MAX);
    if (fat_readdir(cwd, ls_cb, 0) < 0)
        vga_puts("Error listing directory.\n");
}

static void cmd_cd(const char *arg) {
    if (strcmp(arg, "\\") == 0 || strcmp(arg, "/") == 0 || strcmp(arg, "") == 0) {
        fat_chdir("0:\\");
        return;
    }
    if (fat_chdir(arg) < 0)
        vga_puts("No such directory.\n");
}

static void cmd_mkdir(const char *arg) {
    if (fat_mkdir(arg) < 0)
        vga_puts("Error creating directory.\n");
    else
        vga_puts("OK\n");
}

static void cmd_deldir(const char *arg) {
    if (fat_rmdir(arg) < 0)
        vga_puts("Error deleting directory.\n");
}

static void cmd_mkfile(const char *arg) {
    char cwd[FAT_PATH_MAX];
    fat_getcwd(cwd, FAT_PATH_MAX);
    char path[FAT_PATH_MAX];
    strncpy(path, cwd, FAT_PATH_MAX - 1);
    size_t cl = strlen(path);
    if (cl > 0 && path[cl-1] != '\\') { path[cl] = '\\'; path[cl+1] = '\0'; }
    strncat(path, arg, FAT_PATH_MAX - strlen(path) - 1);
    if (fat_create(path) < 0)
        vga_puts("Error creating file.\n");
}

static void cmd_delfile(const char *arg) {
    if (fat_unlink(arg) < 0)
        vga_puts("Error deleting file.\n");
    else
        vga_puts("OK\n");
}

static void cmd_readfile(const char *arg) {
    char buf[BUF_MAX];
    char cwd[FAT_PATH_MAX];
    fat_getcwd(cwd, FAT_PATH_MAX);
    char path[FAT_PATH_MAX];
    strncpy(path, cwd, FAT_PATH_MAX - 1);
    size_t cl = strlen(path);
    if (cl > 0 && path[cl-1] != '\\') { path[cl] = '\\'; path[cl+1] = '\0'; }
    strncat(path, arg, FAT_PATH_MAX - strlen(path) - 1);

    int r = fat_read(path, buf, sizeof(buf));
    if (r < 0) { vga_puts("Error reading file.\n"); return; }
    vga_putchar('\n');
    vga_puts(buf);
    vga_putchar('\n');
}

static void cmd_scribble(void) {
    char txtfile[FAT_PATH_MAX];
    vga_puts("1st line: txt file. 2nd line: text to add.\n");
    read_line(txtfile, sizeof(txtfile));

    char cwd[FAT_PATH_MAX];
    fat_getcwd(cwd, FAT_PATH_MAX);
    char path[FAT_PATH_MAX];
    strncpy(path, cwd, FAT_PATH_MAX - 1);
    size_t cl = strlen(path);
    if (cl > 0 && path[cl-1] != '\\') { path[cl] = '\\'; path[cl+1] = '\0'; }
    strncat(path, txtfile, FAT_PATH_MAX - strlen(path) - 1);

    char existing[BUF_MAX];
    vga_puts("Contents of ");
    vga_puts(txtfile);
    vga_puts(": \n");
    int r = fat_read(path, existing, sizeof(existing));
    if (r > 0) vga_puts(existing);

    vga_puts("Text to add to file: \n");
    char ttw[512];
    read_line(ttw, sizeof(ttw));

    /* Append newline + text */
    char nl = '\n';
    fat_append(path, &nl, 1);
    fat_append(path, ttw, strlen(ttw));
    vga_putchar('\n');
}

/* ---- Aquapp script interpreter -------------------------------------- */
static void cmd_run(const char *arg) {
    char cwd[FAT_PATH_MAX];
    fat_getcwd(cwd, FAT_PATH_MAX);
    char path[FAT_PATH_MAX];
    strncpy(path, cwd, FAT_PATH_MAX - 1);
    size_t cl = strlen(path);
    if (cl > 0 && path[cl-1] != '\\') { path[cl] = '\\'; path[cl+1] = '\0'; }
    strncat(path, arg, FAT_PATH_MAX - strlen(path) - 1);

    char script[BUF_MAX];
    int r = fat_read(path, script, sizeof(script));
    if (r < 0) { vga_puts("Could not open file.\n"); return; }

    char usrinput[512]; usrinput[0] = '\0';

    /* Parse line by line */
    char *line = script;
    while (*line) {
        char *end = line;
        while (*end && *end != '\n') end++;
        size_t llen = (size_t)(end - line);
        char lbuf[512];
        if (llen >= sizeof(lbuf)) llen = sizeof(lbuf) - 1;
        memcpy(lbuf, line, llen);
        lbuf[llen] = '\0';

        if (strstr(lbuf, "info = ")) {
            const char *info = str_skip(lbuf, 7);
            vga_puts(info);
            vga_putchar('\n');
            vga_puts("Press any key to start the aquapp! ");
            read_line(lbuf, sizeof(lbuf));
            vga_putchar('\n');
        } else if (strstr(lbuf, "writeline")) {
            const char *msg = str_skip(lbuf, 10);
            /* strip parens */
            if (*msg == '(') msg++;
            char tmp[512];
            strncpy(tmp, msg, 510);
            size_t tl = strlen(tmp);
            if (tl > 0 && tmp[tl-1] == ')') tmp[tl-1] = '\0';
            vga_puts(tmp);
            vga_putchar('\n');
        } else if (strstr(lbuf, "write")) {
            const char *msg = str_skip(lbuf, 6);
            if (*msg == '(') msg++;
            char tmp[512];
            strncpy(tmp, msg, 510);
            size_t tl = strlen(tmp);
            if (tl > 0 && tmp[tl-1] == ')') tmp[tl-1] = '\0';
            vga_puts(tmp);
        } else if (strstr(lbuf, "sayinput")) {
            vga_puts(usrinput);
        } else if (strstr(lbuf, "inputget")) {
            const char *prompt = str_skip(lbuf, 9);
            if (*prompt == '(') prompt++;
            char tmp[512];
            strncpy(tmp, prompt, 510);
            size_t tl = strlen(tmp);
            if (tl > 0 && tmp[tl-1] == ')') tmp[tl-1] = '\0';
            vga_puts(tmp);
            read_line(usrinput, sizeof(usrinput));
        } else if (strstr(lbuf, "newline")) {
            vga_putchar('\n');
        }
        /* skip unknown lines */

        line = *end ? end + 1 : end;
    }
}

/* ---- Erase command -------------------------------------------------- */
static void cmd_erase(void) {
    char cwd[FAT_PATH_MAX];
    fat_getcwd(cwd, FAT_PATH_MAX);

    vga_puts("Old file (to erase from): ");
    char ftef[FAT_NAME_MAX]; read_line(ftef, sizeof(ftef));
    vga_puts("New filename: ");
    char nfn[FAT_NAME_MAX];  read_line(nfn, sizeof(nfn));

    char old_path[FAT_PATH_MAX], new_path[FAT_PATH_MAX];
    strncpy(old_path, cwd, FAT_PATH_MAX - 1);
    size_t cl = strlen(old_path);
    if (cl > 0 && old_path[cl-1] != '\\') { old_path[cl] = '\\'; old_path[cl+1] = '\0'; }
    strncpy(new_path, old_path, FAT_PATH_MAX - 1);
    strncat(old_path, ftef, FAT_PATH_MAX - strlen(old_path) - 1);
    strncat(new_path, nfn,  FAT_PATH_MAX - strlen(new_path) - 1);

    char buf[BUF_MAX];
    int r = fat_read(old_path, buf, sizeof(buf));
    if (r < 0) { vga_puts("Cannot read file.\n"); return; }

    char out_buf[BUF_MAX]; out_buf[0] = '\0';
    size_t out_len = 0;

    /* Process line by line */
    char *line = buf;
    while (*line) {
        char *end = line;
        while (*end && *end != '\n') end++;
        size_t llen = (size_t)(end - line);
        char lbuf[512];
        if (llen >= sizeof(lbuf)) llen = sizeof(lbuf) - 1;
        memcpy(lbuf, line, llen);
        lbuf[llen] = '\0';

        vga_puts("Line: ");
        vga_puts(lbuf);
        vga_putchar('\n');
        vga_puts("Delete line?(Y/n): \n");
        char answer[4]; read_line(answer, sizeof(answer));

        if (strcmp(answer, "n") == 0) {
            if (out_len + llen + 1 < BUF_MAX) {
                memcpy(out_buf + out_len, lbuf, llen);
                out_len += llen;
                out_buf[out_len++] = '\n';
            }
        }
        line = *end ? end + 1 : end;
    }

    fat_write(new_path, out_buf, out_len);
    fat_unlink(old_path);
}

/* ---- Main shell loop ------------------------------------------------ */
void shell_run(int fs_enabled) {
    (void)fs_enabled; /* filesystem availability already set by fat_init */

    char input[INPUT_MAX];

    for (;;) {
        print_prompt();
        read_line(input, sizeof(input));
        beep(0, 0); /* short beep like original */

        /* ---- shutdown ------------------------------------------ */
        if (strcmp(input, "shutdown") == 0) {
            power_shutdown();

        /* ---- run ----------------------------------------------- */
        } else if (strstr(input, "run") == input && input[3] == ' ') {
            cmd_run(str_skip(input, 4));

        /* ---- mkfile -------------------------------------------- */
        } else if (strstr(input, "mkfile") == input && input[6] == ' ') {
            cmd_mkfile(str_skip(input, 7));

        /* ---- mkdir --------------------------------------------- */
        } else if (strstr(input, "mkdir") == input && input[5] == ' ') {
            cmd_mkdir(str_skip(input, 6));

        /* ---- deldir -------------------------------------------- */
        } else if (strstr(input, "deldir") == input && input[6] == ' ') {
            cmd_deldir(str_skip(input, 7));

        /* ---- delfile ------------------------------------------- */
        } else if (strstr(input, "delfile") == input && input[7] == ' ') {
            cmd_delfile(str_skip(input, 8));

        /* ---- cd ------------------------------------------------ */
        } else if (strstr(input, "cd") == input &&
                   (input[2] == ' ' || input[2] == '\\' || input[2] == '\0')) {
            if (input[2] == ' ')
                cmd_cd(str_skip(input, 3));
            else if (input[2] == '\\')
                cmd_cd("\\");
            else
                cmd_cd("\\");

        /* ---- ls ------------------------------------------------ */
        } else if (strcmp(input, "ls") == 0) {
            cmd_ls();

        /* ---- creator ------------------------------------------- */
        } else if (strcmp(input, "creator") == 0) {
            cmd_creator();

        /* ---- gettime ------------------------------------------- */
        } else if (strcmp(input, "gettime") == 0) {
            cmd_gettime();

        /* ---- scribble ------------------------------------------ */
        } else if (strcmp(input, "scribble") == 0) {
            cmd_scribble();

        /* ---- readfile ------------------------------------------ */
        } else if (strstr(input, "readfile") == input && input[8] == ' ') {
            cmd_readfile(str_skip(input, 9));

        /* ---- echo ---------------------------------------------- */
        } else if (strstr(input, "echo") == input) {
            if (input[4] == ' ')
                cmd_echo(str_skip(input, 5));
            /* bare "echo" does nothing */

        /* ---- erase --------------------------------------------- */
        } else if (strcmp(input, "erase") == 0) {
            cmd_erase();

        /* ---- help ---------------------------------------------- */
        } else if (strcmp(input, "help") == 0) {
            cmd_help();

        /* ---- clear --------------------------------------------- */
        } else if (strcmp(input, "clear") == 0) {
            cmd_clear();

        /* ---- about --------------------------------------------- */
        } else if (strcmp(input, "about") == 0) {
            cmd_about();

        /* ---- empty input --------------------------------------- */
        } else if (input[0] == '\0') {
            /* do nothing */

        /* ---- unknown ------------------------------------------- */
        } else {
            vga_set_fg(VGA_WHITE);
            vga_puts("Unknown command: ");
            vga_puts(input);
            vga_putchar('\n');
        }
    }
}
