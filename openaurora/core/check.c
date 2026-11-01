/* Read-only loader preflight. Never dlopen a module: even that runs dependency
 * constructors. Use the executable's own ELF interpreter in ldd relocation-
 * trace mode, and inspect the module note/entry symbol directly from the file. */
#include <dirent.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <link.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "host.h"

static int read_at(FILE *f, uint64_t offset, void *out, size_t size)
{
    return offset <= LONG_MAX && fseek(f, (long)offset, SEEK_SET) == 0 &&
           fread(out, 1, size, f) == size ? 0 : -1;
}
static void *section_data(FILE *f, const ElfW(Shdr) *section)
{
    /* Modules are small; reject implausible metadata rather than trust sizes. */
    if (!section->sh_size || section->sh_size > 16 * 1024 * 1024)
        return NULL;
    char *data = malloc((size_t)section->sh_size + 1);
    if (!data) return NULL;
    if (read_at(f, section->sh_offset, data, (size_t)section->sh_size)) {
        free(data);
        return NULL;
    }
    data[section->sh_size] = 0;
    return data;
}
static int elf_header(FILE *f, ElfW(Ehdr) *header)
{
    return read_at(f, 0, header, sizeof *header) == 0 &&
           !memcmp(header->e_ident, ELFMAG, SELFMAG) &&
           header->e_ident[EI_CLASS] == (sizeof(void *) == 8 ? ELFCLASS64 : ELFCLASS32) &&
           header->e_ident[EI_VERSION] == EV_CURRENT ? 0 : -1;
}
static int module_metadata(const char *path, const ElfW(Ehdr) *self)
{
    FILE *f = fopen(path, "rb");
    if (!f) { printf("FAIL %s: %s\n", path, strerror(errno)); return -1; }
    ElfW(Ehdr) h;
    ElfW(Shdr) *sections = NULL;
    char *names = NULL;
    int valid_note = 0, entry = 0, result = -1;
    const char *reason = "invalid/incompatible ELF header";
    if (elf_header(f, &h) || h.e_machine != self->e_machine ||
        h.e_ident[EI_DATA] != self->e_ident[EI_DATA] || h.e_type != ET_DYN)
        goto done;
    reason = "invalid ELF section table";
    if (!h.e_shnum || h.e_shnum > 4096 || h.e_shentsize != sizeof *sections ||
        h.e_shstrndx >= h.e_shnum)
        goto done;
    sections = malloc((size_t)h.e_shnum * sizeof *sections);
    if (!sections || read_at(f, h.e_shoff, sections, (size_t)h.e_shnum * sizeof *sections))
        goto done;
    names = section_data(f, &sections[h.e_shstrndx]);
    if (!names) goto done;
    for (unsigned i = 0; i < h.e_shnum; i++) {
        ElfW(Shdr) *s = &sections[i];
        if (s->sh_name >= sections[h.e_shstrndx].sh_size) goto done;
        if (!strcmp(names + s->sh_name, ".note.openaurora")) {
            oa_module_note note;
            reason = "invalid openaurora ABI note";
            if (s->sh_size != sizeof note || read_at(f, s->sh_offset, &note, sizeof note) ||
                note.namesz != 11 || note.descsz != 12 || note.type != OA_MODULE_MAGIC ||
                memcmp(note.name, "openaurora", 11) || note.magic != OA_MODULE_MAGIC)
                goto done;
            reason = "module ABI/descriptor-size mismatch";
            if (note.abi != OA_ABI_VERSION || note.size < sizeof(oa_module))
                goto done;
            valid_note = 1;
        }
        if (s->sh_type == SHT_DYNSYM) {
            reason = "invalid dynamic symbol table";
            if (s->sh_link >= h.e_shnum || s->sh_entsize != sizeof(ElfW(Sym)) ||
                s->sh_size % sizeof(ElfW(Sym)))
                goto done;
            ElfW(Sym) *symbols = section_data(f, s);
            char *strings = section_data(f, &sections[s->sh_link]);
            if (!symbols || !strings) { free(symbols); free(strings); goto done; }
            for (size_t j = 0; j < s->sh_size / sizeof *symbols; j++) {
                ElfW(Sym) *sym = &symbols[j];
                if (sym->st_name < sections[s->sh_link].sh_size && sym->st_shndx != SHN_UNDEF &&
                    ELF32_ST_TYPE(sym->st_info) == STT_FUNC &&
                    (ELF32_ST_BIND(sym->st_info) == STB_GLOBAL || ELF32_ST_BIND(sym->st_info) == STB_WEAK) &&
                    (ELF32_ST_VISIBILITY(sym->st_other) == STV_DEFAULT ||
                     ELF32_ST_VISIBILITY(sym->st_other) == STV_PROTECTED) &&
                    !strcmp(strings + sym->st_name, OA_MODULE_SYMBOL))
                    entry = 1;
            }
            free(symbols);
            free(strings);
        }
    }
    reason = "missing ABI note (legacy/unverifiable module); rebuild with OA_MODULE_EXPORT";
    if (!valid_note) goto done;
    reason = "missing exported oa_module_entry";
    if (!entry) goto done;
    result = 0;
done:
    if (result) printf("FAIL %s: %s\n", path, reason);
    free(names);
    free(sections);
    fclose(f);
    return result;
}
static int loader_trace(const char *loader, const char *path)
{
    int fds[2];
    if (pipe2(fds, O_CLOEXEC)) { perror("check pipe"); return -1; }
    char *library_path = NULL;
    const char *search = getenv("LD_LIBRARY_PATH");
    if (search && asprintf(&library_path, "LD_LIBRARY_PATH=%s", search) < 0) {
        close(fds[0]); close(fds[1]); return -1;
    }
    printf("CHECK %s\n", path);
    fflush(stdout);
    pid_t child = fork();
    if (child == 0) {
        close(fds[0]);
        if (dup2(fds[1], STDOUT_FILENO) < 0 || dup2(fds[1], STDERR_FILENO) < 0)
            _exit(125);
        close(fds[1]);
        /* A small environment prevents loader debug/profiling/audit/preload
         * settings from writing files or introducing extra module code. */
        char *env[] = { "LC_ALL=C", "LD_TRACE_LOADED_OBJECTS=1", "LD_WARN=yes",
                        "LD_BIND_NOW=yes", "LD_VERBOSE=1", library_path, NULL };
        char *args[] = { (char *)loader, (char *)path, NULL };
        execve(loader, args, env);
        dprintf(STDERR_FILENO, "cannot execute ELF interpreter: %s\n", strerror(errno));
        _exit(126);
    }
    close(fds[1]);
    free(library_path);
    if (child < 0) { close(fds[0]); perror("check fork"); return -1; }
    int failed = 0, status = 0;
    uint64_t deadline = oa_now_ms() + 10000;
    char tail[128] = "";
    for (;;) {
        uint64_t now = oa_now_ms();
        int timeout = deadline > now ? (int)(deadline - now) : 0;
        struct pollfd p = { .fd = fds[0], .events = POLLIN };
        int ready = poll(&p, 1, timeout);
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) {
            printf("FAIL %s: loader trace %s\n", path, ready ? "poll error" : "timed out");
            kill(child, SIGKILL);
            failed = 1;
            break;
        }
        char chunk[4097];
        ssize_t n = read(fds[0], chunk, sizeof chunk - 1);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { if (n < 0) failed = 1; break; }
        chunk[n] = 0;
        fputs(chunk, stdout);
        char combined[sizeof tail + sizeof chunk];
        snprintf(combined, sizeof combined, "%s%s", tail, chunk);
        /* glibc trace mode can print unresolved-symbol diagnostics yet exit 0. */
        if (strstr(combined, "undefined symbol:") || strstr(combined, "=> not found") ||
            strstr(combined, "not found (required by") || strstr(combined, "relocation error:"))
            failed = 1;
        size_t len = strlen(combined);
        const char *last = combined + (len >= sizeof tail ? len - sizeof tail + 1 : 0);
        memcpy(tail, last, strlen(last) + 1);
    }
    close(fds[0]);
    pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    if (waited < 0 || !WIFEXITED(status) || WEXITSTATUS(status)) failed = 1;
    printf("%s %s\n", failed ? "FAIL" : "OK", path);
    return failed ? -1 : 0;
}
static int module_file(const struct dirent *file)
{
    size_t n = strlen(file->d_name);
    return n > 7 && !strncmp(file->d_name, "mod_", 4) && !strcmp(file->d_name + n - 3, ".so");
}
int oa_runtime_check(const char *modules_dir)
{
    ElfW(Ehdr) self;
    char executable[PATH_MAX], loader[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", executable, sizeof executable - 1);
    if (len < 0 || (size_t)len == sizeof executable - 1) { perror("check executable"); return 1; }
    executable[len] = 0;
    FILE *f = fopen(executable, "rb");
    if (!f) { perror("check executable"); return 1; }
    int have_loader = 0;
    if (!elf_header(f, &self) && self.e_phentsize == sizeof(ElfW(Phdr))) {
        for (unsigned i = 0; i < self.e_phnum; i++) {
            ElfW(Phdr) ph;
            if (read_at(f, self.e_phoff + (uint64_t)i * sizeof ph, &ph, sizeof ph)) break;
            if (ph.p_type == PT_INTERP && ph.p_filesz > 1 && ph.p_filesz <= sizeof loader &&
                !read_at(f, ph.p_offset, loader, ph.p_filesz) && loader[ph.p_filesz - 1] == 0) {
                have_loader = 1;
                break;
            }
        }
    }
    fclose(f);
    if (!have_loader) { puts("FAIL no supported ELF interpreter in executable"); return 1; }
    printf("Read-only runtime preflight; interpreter %s; module ABI %u\n", loader, OA_ABI_VERSION);
    int failed = loader_trace(loader, executable) != 0;
    struct dirent **files = NULL;
    int count = scandir(modules_dir, &files, module_file, alphasort);
    if (count < 0) { printf("FAIL %s: %s\n", modules_dir, strerror(errno)); failed = 1; }
    else if (!count) { printf("FAIL %s: no mod_*.so modules\n", modules_dir); failed = 1; }
    for (int i = 0; i < count; i++) {
        char path[PATH_MAX];
        int n = snprintf(path, sizeof path, "%s/%s", modules_dir, files[i]->d_name);
        if (n < 0 || (size_t)n >= sizeof path) { puts("FAIL module path too long"); failed = 1; }
        else {
            struct stat st;
            if (stat(path, &st) || !S_ISREG(st.st_mode)) {
                printf("FAIL %s: not a readable regular file\n", path);
                failed = 1;
            } else {
                if (module_metadata(path, &self)) failed = 1;
                if (loader_trace(loader, path)) failed = 1;
            }
        }
        free(files[i]);
    }
    free(files);
    printf("Runtime preflight %s. No module entry/init/start, control socket or hardware access.\n",
           failed ? "FAILED" : "PASSED");
    puts("This checks ELF metadata and library relocations, not device operation or display scanout.");
    return failed ? 1 : 0;
}
