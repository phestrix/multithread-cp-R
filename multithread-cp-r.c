#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 8192

typedef struct {
  char source[PATH_MAX];
  char target[PATH_MAX];
} thread_args_t;

void *copy_file(void *args);
void *copy_directory(void *args);

void *copy_file(void *args) {
  thread_args_t *paths = (thread_args_t *)args;
  char *source = paths->source;
  char *target = paths->target;

  int src_fd, dest_fd;
  char buffer[BUFFER_SIZE];
  ssize_t bytes_read, bytes_written;

  while ((src_fd = open(source, O_RDONLY)) == -1 && errno == EMFILE) {
	sleep(1);
  }

  if (src_fd == -1) {
	perror("Error opening source file");
	pthread_exit(NULL);
  }

  while ((dest_fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1 && errno == EMFILE) {
	sleep(1);
  }

  if (dest_fd == -1) {
	perror("Error opening target file");
	close(src_fd);
	pthread_exit(NULL);
  }

  while ((bytes_read = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
	bytes_written = write(dest_fd, buffer, bytes_read);
	if (bytes_written != bytes_read) {
	  perror("Error writing to target file");
	  close(src_fd);
	  close(dest_fd);
	  pthread_exit(NULL);
	}
  }

  if (bytes_read == -1) {
	perror("Error reading from source file");
  }

  close(src_fd);
  close(dest_fd);
  pthread_exit(NULL);
}


void *copy_directory(void *args) {
  thread_args_t *paths = (thread_args_t *)args;
  char *source = paths->source;
  char *target = paths->target;

  DIR *src_dir;
  struct dirent *entry;
  struct stat entry_stat;
  pthread_t threads[256];
  int thread_count = 0;

  while ((src_dir = opendir(source)) == NULL && errno == EMFILE) {
	sleep(1);
  }
  if (src_dir == NULL) {
	perror("Error opening source directory");
	pthread_exit(NULL);
  }

  if (mkdir(target, 0755) == -1 && errno != EEXIST) {
	perror("Error creating target directory");
	closedir(src_dir);
	pthread_exit(NULL);
  }

  while ((entry = readdir(src_dir)) != NULL) {
	if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
	  continue;
	}

	char src_path[PATH_MAX];
	char dest_path[PATH_MAX];

	snprintf(src_path, PATH_MAX, "%s/%s", source, entry->d_name);
	snprintf(dest_path, PATH_MAX, "%s/%s", target, entry->d_name);

	if (stat(src_path, &entry_stat) == -1) {
	  perror("Error stating file");
	  continue;
	}

	thread_args_t *new_args = malloc(sizeof(thread_args_t));
	strncpy(new_args->source, src_path, PATH_MAX);
	strncpy(new_args->target, dest_path, PATH_MAX);

	if (S_ISDIR(entry_stat.st_mode)) {
	  pthread_create(&threads[thread_count++], NULL, copy_directory, new_args);
	} else if (S_ISREG(entry_stat.st_mode)) {
	  pthread_create(&threads[thread_count++], NULL, copy_file, new_args);
	} else {
	  free(new_args);
	}

	if (thread_count >= 256) {
	  for (int i = 0; i < thread_count; i++) {
		pthread_join(threads[i], NULL);
	  }
	  thread_count = 0;
	}
  }

  for (int i = 0; i < thread_count; i++) {
	pthread_join(threads[i], NULL);
  }

  closedir(src_dir);
  pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
	fprintf(stderr, "Usage: %s <source_directory> <target_directory>\n", argv[0]);
	return EXIT_FAILURE;
  }

  struct stat src_stat;
  if (stat(argv[1], &src_stat) == -1) {
	perror("Error stating source directory");
	return EXIT_FAILURE;
  }

  if (!S_ISDIR(src_stat.st_mode)) {
	fprintf(stderr, "Source must be a directory\n");
	return EXIT_FAILURE;
  }

  thread_args_t args;
  strncpy(args.source, argv[1], PATH_MAX);
  strncpy(args.target, argv[2], PATH_MAX);

  pthread_t main_thread;
  pthread_create(&main_thread, NULL, copy_directory, &args);
  pthread_join(main_thread, NULL);

  return EXIT_SUCCESS;
}