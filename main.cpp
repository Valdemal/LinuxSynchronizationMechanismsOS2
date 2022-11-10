#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <semaphore>

const char SHARED_FILE_NAME[] = "_t_timer_shared_file";

const size_t MEMORY_SIZE = sizeof(sem_t) +  sizeof(unsigned long);

class TimerProcessDescriptor {
public:
    explicit TimerProcessDescriptor(const char *shared_file_name, const char *process_name) : name(process_name) {

        bool need_to_init = false;
        auto file_descriptor = get_file_descriptor(shared_file_name, need_to_init);

        // Отображаем файл в память
        // Первый адрес - семафор, второй - счетчик
        void *shared_memory_ptr = mmap(
                nullptr,
                MEMORY_SIZE,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                file_descriptor,
                0
        );
        close(file_descriptor);

        /* Инициализация указателей на данные путем последовательного чтения из файла */
        this->sem_ptr = (sem_t*) (shared_memory_ptr);
        this->timer_ptr = (unsigned long *) ((char*)shared_memory_ptr + sizeof(sem_t));

        if (need_to_init) {
            sem_init(this->sem_ptr, 1, 1);
            *timer_ptr = 1;
        }

        if (this->sem_ptr == SEM_FAILED)
            throw std::runtime_error("Ошибка при открытии семафора!");
    }

    void step() {
        sem_wait(sem_ptr);
        for (size_t i = 0; i < 5; i++) {
            sleep(1);
            std::cout << "Таймер: " << name << ". Секунд прошло: " << *timer_ptr << std::endl;
            *timer_ptr += 1;
        }
        sem_post(sem_ptr);
        sleep(1);
    }

private:
    sem_t *sem_ptr{nullptr};
    unsigned long *timer_ptr{nullptr};
    std::string name;

    /* Возвращает дескриптор файла общего для нескольких потоков */
    static int get_file_descriptor(const char *filename, bool &need_to_init_mutex) {
        // O_EXCL нужен, для того чтобы, если файл уже создан вернуть -1
        int file = open(filename, O_RDWR | O_CREAT | O_EXCL, 0666);

        if (file != -1) {
            // Задать размер файла
            ftruncate(file, MEMORY_SIZE);
            need_to_init_mutex = true;
        } else {
            // Иначе просто открыть файл
            file = open(filename, O_RDWR, 0666);
        }

        if (file == -1)
            throw std::runtime_error("Не удалось открыть файл!");

        return file;
    }
};


int main(int argc, char **argv) {
    // считываем имя таймера
    char *name = argv[1];

    TimerProcessDescriptor process(SHARED_FILE_NAME, name);

    while (true) {
        process.step();
    }
}