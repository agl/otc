#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

  char *result;
  size_t result_len;
  FILE *memstream = open_memstream(&result, &result_len);
  FILEStream output(memstream);
  bool r = otc_process(&output, data, st.st_size);
  fclose(memstream);
  if (!r) {
    free(result);
    fprintf(stderr, "Failed to sanitise file!\n");
    return 1;
  }
  free(data);

  char *result2;
  size_t result2_len;
  memstream = open_memstream(&result2, &result2_len);
  FILEStream output2(memstream);
  r = otc_process(&output2, (const uint8_t *) result, result_len);
  fclose(memstream);
  if (!r) {
    free(result);
    free(result2);
    fprintf(stderr, "Failed to sanitise previous output!");
    return 1;
  }

  bool dump_results = false;
  if (result2_len != result_len) {
    fprintf(stderr, "Outputs differ in length\n");
    dump_results = true;
  } else if (memcmp(result2, result, result_len)) {
    fprintf(stderr, "Outputs differ in context\n");
    dump_results = true;
  }

  if (dump_results) {
    fprintf(stderr, "Dumping results to out1.tff and out2.tff\n");
    int fd = open("out1.ttf", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    int fd2 = open("out2.ttf", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0 || fd2 < 0) {
      perror("opening output file");
      free(result);
      free(result2);
      return 1;
    }

    write(fd, result, result_len);
    write(fd2, result2, result2_len);
    close(fd);
    close(fd2);
  }

  free(result);
  free(result2);

  return 0;
}
