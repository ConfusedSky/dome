#ifndef __EMSCRIPTEN__


// This header is placed at the end of the file
#pragma pack(push, 1)
typedef struct {
  uint8_t magic1[4];
  uint8_t version;
  uint64_t offset;
  uint8_t magic2[4];
} DOME_FUSED_HEADER;
#pragma pack(pop)

typedef struct {
  FILE* fd;
  size_t offset;
} FUSE_STREAM;


internal char*
FUSE_getExecutablePath() {
  char* path = NULL;
  size_t size = wai_getExecutablePath(NULL, 0, NULL);
  if (size > 0) {
    path = malloc(size + 1);
    if (path != NULL) {
      wai_getExecutablePath(path, size, NULL);
      path[size] = '\0';
    }
  }
  return path;
}

void FUSE_usage() {

}

int FUSE_perform(char **argv) {
  struct optparse options;
  int option;
  optparse_init(&options, argv);
  options.permute = 0;
  struct optparse_long longopts[] = {
    {"help", 'h', OPTPARSE_NONE},
    {"fuse", 'f', OPTPARSE_NONE},
    {0}
  };
  while ((option = optparse_long(&options, longopts, NULL)) != -1) {
    switch (option) {
      case 'h':
        FUSE_usage();
        return EXIT_SUCCESS;
      case '?':
        fprintf(stderr, "%s: %s\n", argv[0], options.errmsg);
        return EXIT_FAILURE;
    }
  }
  argv += options.optind;

  char* fileName = optparse_arg(&options);
  if (fileName == NULL) {
    fprintf(stderr, "Not enough arguments\n");
    FUSE_usage();
    return EXIT_FAILURE;
  }
  char* outputFileName = optparse_arg(&options);
  if (outputFileName == NULL) {
#if defined _WIN32 || __MINGW32__
    outputFileName = "game.exe";
#else
    outputFileName = "game";
#endif
  }

  char* binaryPath = FUSE_getExecutablePath();
  if (binaryPath != NULL) {
    FILE* binaryIn = fopen(binaryPath, "rb");
    free(binaryPath);
    binaryPath = NULL;
    FILE* binaryOut = fopen(outputFileName, "wb");
    if (binaryIn == NULL || binaryOut == NULL) {
      perror("Error loading DOME binary");
      return EXIT_FAILURE;
    }

    mtar_t tar;
    int tarResult = mtar_open(&tar, fileName, "rb");
    FILE* egg = tar.stream;
    if (tarResult != MTAR_ESUCCESS) {
      fprintf(stderr, "Could not fuse: %s is not a valid EGG file.\n", fileName);
      return EXIT_FAILURE;
    }

    int c;
    while((c = fgetc(binaryIn)) != EOF) {
      fputc(c, binaryOut);
    }
    fclose(binaryIn);
    uint64_t size = sizeof(DOME_FUSED_HEADER);
    while((c = fgetc(egg)) != EOF) {
      fputc(c, binaryOut);
      size++;
    }
    mtar_close(&tar);

    DOME_FUSED_HEADER header;
    memcpy(header.magic1, "DOME", 4);
    memcpy(header.magic2, "DOME", 4);
    header.version = 1;
    header.offset = size;
    fwrite(&header, sizeof(DOME_FUSED_HEADER), 1, binaryOut);
    fclose(binaryOut);

    // Set permissions
    // All owner permissions, everyone else reads and executes
    int result = chmod(outputFileName, 0755);
    if (result != 0) {
      perror("Couldn't set permissions on the fused file");
      return EXIT_FAILURE;
    }

    printf("Fused '%s' into DOME successfully as '%s'", fileName, outputFileName);
  }
  free(binaryPath);
  return EXIT_SUCCESS;
}

int FUSE_read(mtar_t* tar, void *data, unsigned size) {
  FUSE_STREAM* stream = tar->stream;
  unsigned res = fread(data, 1, size, stream->fd);
  return (res == size) ? MTAR_ESUCCESS : MTAR_EREADFAIL;
}

int FUSE_seek(mtar_t* tar, unsigned pos) {
  FUSE_STREAM* stream = tar->stream;
  assert(pos <= stream->offset);
  int res = fseek(stream->fd, pos - stream->offset, SEEK_END);
  return (res == 0) ? MTAR_ESUCCESS : MTAR_ESEEKFAIL;
}

int FUSE_close(mtar_t* tar) {
  FUSE_STREAM* stream = tar->stream;
  if (stream->fd != NULL) {
    fclose(stream->fd);
  }
  free(stream);
  tar->stream = NULL;
  return MTAR_ESUCCESS;
}

int FUSE_open(mtar_t* tar, FILE* fd, size_t offset) {
  FUSE_STREAM* stream = malloc(sizeof(FUSE_STREAM));
  if (stream == NULL) {
    return MTAR_EFAILURE;
  }
  stream->fd = fd;
  stream->offset = offset;

  tar->stream = stream;
  tar->read = FUSE_read;
  tar->seek = FUSE_seek;
  tar->close = FUSE_close;
  tar->write = NULL;

  return MTAR_ESUCCESS;
}

#endif
