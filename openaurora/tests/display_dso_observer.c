/* Test-only load/unload observation. Linked only into the fixture DSO. */
extern void display_fixture_event(int loaded);
static void loaded(void) __attribute__((constructor));
static void unloaded(void) __attribute__((destructor));
static void loaded(void) { display_fixture_event(1); }
static void unloaded(void) { display_fixture_event(0); }
