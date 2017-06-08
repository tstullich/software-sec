#define IMG_TEST	0

static const char media_path[] = "/media";

// Directory Scan API
struct media_file {
  int type;
  char name[0];
};
struct media_file_chunk {
  struct media_file_chunk *next;
  char dir[32];
  int count;
  int next_file_offset;
  char files[0];
};
#define file_to_chunk(f)	((struct media_file_chunk *)(((intptr_t)f) & ~4095UL))
void init_scan_subsystem(void);
void scan_dir(const char *);
void forget_dir(const char *);
struct media_file *next_file(struct media_file *);
void advance_cursor(void);
unsigned int total_count;
struct media_file *cursor;

// Media Presentation
void init_play_subsystem(void);
void play_file(struct media_file *);
int find_handler(const char *);

// Main Event Loop
void mev_add(int, void (*)(), void (*)(), void (*)());
void mev_run(void);

// Misc.
void mediacenter_log(int, const char *, ...);

// Notify
void init_notify(void);
void add_notify(const char *);
void del_notify(const char *);
void rescan_all(void);

// TCP Socket
int network_open(void);
void network_close(void);
