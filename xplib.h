void minimise_window(int);
void kill_window(int);
void maximise_window(int);
void window_to_back(int);
char *read_clipboard(void);
void write_clipboard(const char *);
void open_url(const char *);
void spawn_process(const char *);
void debug_message(const char *);
void unregister_all_hotkeys(void);
int register_hotkey(int index, int modifiers, const char *key,
		    const char *keyorig);
void error(char *s, ...);
void set_unix_url_opener_command(const char *);

enum {
    LEFT_WINDOWS = 1,
    CTRL = 2,
    ALT = 4,
};

/*
 * Method for storing a preference list of ways to select the active
 * window.
 */
enum {
    END_OF_OPTIONS,
    WINDOW_UNDER_MOUSE,
    WINDOW_WITH_FOCUS,
};
#define CHOICE_SHIFT 2
#define FIRST_CHOICE_MASK ((1 << CHOICE_SHIFT) - 1)

void add_config_filename(const char *filename);
void configure(void);
void run_hotkey(int index);
