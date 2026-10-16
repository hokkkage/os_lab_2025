#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

int main(int argc, char **argv) {
  // Значение -1 позволяет определить, что обязательный аргумент не передали.
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  bool with_files = false;

// Читаем параметры запуска:
// seed — начальное значение генератора случайных чисел;
// array_size — количество элементов массива;
// pnum — количество дочерних процессов;
// by_files — передавать результаты через файлы.
// Если by_files не указан, используем каналы pipe.
  while (true) {

    // Описываем доступные длинные опции и необходимость их значений.
    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    // Получаем очередную опцию; её значение доступно через optarg.
    int c = getopt_long(argc, argv, "f", options, &option_index);

    // Значение -1 означает, что опции закончились.
    if (c == -1) break;

    switch (c) {
      case 0:
        // Для опций с кодом 0 определяем название по option_index.
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            // your code here
            // error handling
            // Преобразуем строку в число и проверяем положительное значение.
            if (seed <= 0){
              printf("seed must be positive\n");
              return 1;
            }
            
            break;
          case 1:
            array_size = atoi(optarg);
            // your code here
            // error handling
            // Массив должен содержать хотя бы один элемент.
            if (array_size <= 0){
              printf("array_size must be positive\n");
              return 1;
            }
            
            break;
          case 2:
            pnum = atoi(optarg);
            // your code here
            // error handling
            // Количество процессов должно быть положительным.
            if (pnum <=0){
              printf("pnum must be positive\n");
              return 1;
            }
            break;
          case 3:
            with_files = true;
            break;

          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;
      case 'f':
        // Опция --by_files или -f включает передачу результатов через файлы.
        with_files = true;
        break;

      case '?':
        // getopt_long обнаружил неверную опцию или отсутствие её значения.
        return 1;

      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  // После разбора опций не должно остаться посторонних аргументов.
  if (optind < argc) {
    printf("Has at least one no option argument\n");
    return 1;
  }

  // Проверяем, что пользователь передал все обязательные параметры.
  if (seed == -1 || array_size == -1 || pnum == -1) {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" \n",
           argv[0]);
    return 1;
  }
// Выделяем память под исходный массив.
// В pipes храним дескрипторы каналов для дочерних процессов:
// [0] — для чтения, [1] — для записи.
  int *array = malloc(sizeof(int) * array_size);
  
  int(*pipes)[2] = malloc(sizeof(*pipes) * pnum);

  // При ошибке выделения памяти освобождаем то, что удалось выделить.
  if (array == NULL || pipes == NULL){
    perror("malloc");
    free(array);
    free(pipes);
    return 1;
  }
  // Заполняем массив до создания дочерних процессов,
// чтобы все они получили одинаковые исходные данные.

  GenerateArray(array, array_size, seed);
  // В родительском процессе считаем количество созданных детей.
  int active_child_processes = 0;
// Сохраняем PID родителя для составления имён файлов.
// PID отличает разные запуски программы,
// а индекс процесса отличает результаты внутри одного запуска.
  pid_t parent_pid = getpid();

  // Начинаем измерять время создания процессов и обработки массива.
  struct timeval start_time;
  gettimeofday(&start_time, NULL);
// Создаём pnum дочерних процессов.
// Каждый процесс будет искать минимум и максимум
// только на своём участке массива.
  for (int i = 0; i < pnum; i++) {

    if (!with_files){
      // Создаём отдельный канал до fork,
// чтобы его дескрипторы получили и родитель, и ребёнок.
      if (pipe(pipes[i]) == -1){
        perror("pipe");
        return 1;
      }
    }

    // После fork выполнение продолжают два процесса.
    // У ребёнка результат равен 0, у родителя — PID ребёнка.
    // Значение -1 означает ошибку.
    pid_t child_pid = fork();

    if (child_pid >= 0) {
      // successful fork
      active_child_processes += 1;

      if (child_pid == 0) {
        // child process
        // parallel somehow
        // Вычисляем начало участка текущего ребёнка.
        // Расширяем тип перед умножением, чтобы избежать переполнения int.
        unsigned int begin = (unsigned int)(
          (unsigned long long)array_size * i / pnum
        );

        // Конец участка не входит в обрабатываемый диапазон.
        // Такая формула учитывает остаток от деления массива на части.
        unsigned int end = (unsigned int)(
          (unsigned long long)array_size * (i + 1) / pnum
        );

        // Ищем минимум и максимум только на своём участке [begin, end).
        struct MinMax local = GetMinMax(array, begin, end);

        if (with_files) {
          // use files here
          char filename[64];

          // Составляем имя файла из PID родителя и индекса ребёнка.
          snprintf(filename,sizeof(filename), "result_%ld_%d.txt", (long)parent_pid, i);

          // Открываем файл для записи локального результата.
          FILE *file = fopen(filename, "w");

          if (file == NULL){
            perror("fopen");
            return 1;
          }

          // Записываем минимум и максимум в текстовом виде.
          fprintf(file,"%d %d\n", local.min, local.max );
          // Закрываем файл и сбрасываем буфер записи.
          fclose(file);
        } else {
          // use pipe here
          // Ребёнок только пишет в канал, поэтому закрывает конец для чтения.
          close(pipes[i][0]);

          // Передаём всю структуру MinMax как последовательность байтов.
          if (write(pipes[i][1], &local, sizeof(local)) != sizeof(local)){
            perror("write");
            return 1;
          }

          // Закончив передачу, закрываем конец канала для записи.
          close(pipes[i][1]);
        }
        return 0;
      }
      if (!with_files){
        close(pipes[i][1]);
      }

    } else {
      printf("Fork failed!\n");
      return 1;
    }
  }

  // Ожидаем завершения созданных дочерних процессов.
  while (active_child_processes > 0) {
    // your code here
    // Один успешный вызов wait дожидается одного дочернего процесса.
    if (wait(NULL) == -1){
      perror("wait");
      return 1;
    }

    active_child_processes -= 1;
  }

  // Начальные значения для поиска общего минимума и максимума.
  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  // Получаем результат каждого дочернего процесса.
  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;

    if (with_files) {
      // read from files
      char filename[64];

      // Восстанавливаем то же имя, которое использовал ребёнок при записи.
      snprintf(
        filename,
        sizeof(filename),
        "result_%ld_%d.txt",
        (long)parent_pid,
        i
      );

      FILE *file = fopen(filename, "r");

      if (file == NULL)
      {
        perror("fopen");
        return 1;
      }

      // Проверяем, что удалось прочитать оба числа.
      if (fscanf(file, "%d %d", &min, &max) != 2)
      {
        printf("Cannot read result from file\n");
        fclose(file);
        return 1;
      }

      // Результат получен, поэтому закрываем и удаляем временный файл.
      fclose(file);
      remove(filename);

    } else {
      // read from pipes
      struct MinMax local;

      // Читаем структуру, которую ребёнок передал через свой канал.
      if (read(pipes[i][0], &local, sizeof(local)) != sizeof(local))
      {
        printf("Cannot read result from pipe\n");
        return 1;
      }

      // После чтения канал больше не нужен.
      close(pipes[i][0]);

      min = local.min;
      max = local.max;
    }

    // Выбираем наименьший из локальных минимумов
    // и наибольший из локальных максимумов.
    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  // Завершаем измерение времени после сбора всех результатов.
  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  // Переводим разницу времени в миллисекунды.
  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  // Освобождаем память исходного массива и массива дескрипторов.
  free(array);
  free(pipes);

  // Выводим общий результат и время выполнения.
  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}
