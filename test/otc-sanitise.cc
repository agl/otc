// A very simple driver program while sanitises the file given as argv[1] and
// writes the sanitised version to stdout.

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include "opentype-condom.h"
#include "file-stream.h"

static int
usage(const char *argv0) {
  fprintf(stderr, "Usage: %s <ttf file>\n", argv0);
  return 1;
}

int
main(int argc, char **argv) {
  if (argc != 2)
    return usage(argv[0]);

  const int fd = open(argv[1], O_RDONLY);
  if (fd < 0) {
    perror("open");
    return 1;
  }

  struct stat st;
  fstat(fd, &st);

  uint8_t *data = (uint8_t *) malloc(st.st_size);
  read(fd, data, st.st_size);
  close(fd);

  FILEStream output(stdout);
  const bool result = otc_process(&output, data, st.st_size);
  free(data);

  if (!result)
    fprintf(stderr, "Failed to sanitise file!\n");

  return !result;
}
