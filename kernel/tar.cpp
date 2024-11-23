#include <tar.h>

#include <assert.h>
#include <logging.h>

TarArchive::TarArchive(void *data, size_t len)
    : m_data((TarBlock *)data) {
    assert(len % RECORD_SIZE == 0);
    m_n_blocks = len / RECORD_SIZE;
}

void TarArchive::enumerate_files(Fn<bool &, const char *, size_t, void *> callback) {
    char name[PATH_LEN] = {0};
    
    for (size_t i = 0; i < m_n_blocks; i++) {
        TarBlock *block = &m_data[i];

        memcpy(name, block->record.name, PATH_LEN);
        
        // Ensure the pathname is null terminated
        bool is_valid_name = false;
        for (size_t j = 0; j < PATH_LEN; j++) {
            if (name[j] == '\0') {
                is_valid_name = true;
                break;
            }
        }

        if (!is_valid_name) {
            log_error("warn: invalid name in TAR archive");
            continue;
        }

        if (block->record.name[0] == '\0') {
            break;
        }

        if (block->record.type != FILE_TYPE_REGULAR) {
            log_error("warn: skipping non-regular file in TAR archive");
            continue;
        }

        size_t size = oct_string_to_int(block->record.size, FILE_SIZE_LEN - 1);

        bool is_done = false;
        callback.call(is_done, name, size, block + 1);

        if (is_done) {
            return;
        }
    }
}

int TarArchive::extract_file(const char *name, void **out_data, size_t *len) {
    *out_data = nullptr;
    
    enumerate_files([&](bool &is_done, const char *file_name, size_t size, void *data) {
        if (strcmp(file_name, name) == 0) {
            *out_data = data;
            *len = size;
            is_done = true;
        }
    });

    return (out_data == nullptr);
}

int64_t TarArchive::oct_string_to_int(const char *str, size_t len) {
    assert(len <= 11);
    
    int64_t ret = 0;

    for (size_t i = 0; i < len; i++) {
        ret = ret * 8 + str[i] - '0';
    }

    return ret;
}
