void minimise_window(void);
void window_to_back(void);
char *read_clipboard(void);
void write_clipboard(const char *);
void open_url(const char *);
void debug_message(const char *);
void unregister_all_hotkeys(void);
int register_hotkey(int index, int modifiers, const char *key,
		    const char *keyorig);
void error(char *s, ...);

enum {
    LEFT_WINDOWS = 1,
    CTRL = 2,
    ALT = 4,
};

void add_config_filename(const char *filename);
void configure(void);
void run_hotkey(int index);
