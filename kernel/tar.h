#pragma once

#include <le/fn.h>

#include <stddef.h>
#include <stdint.h>

class TarArchive {
    static constexpr size_t RECORD_SIZE = 512;
    static constexpr size_t PATH_LEN = 100;
    static constexpr size_t MODE_LEN = 8;
    static constexpr size_t UID_LEN = 8;
    static constexpr size_t GID_LEN = 8;
    static constexpr size_t FILE_SIZE_LEN = 12;
    static constexpr size_t MOD_TIME_LEN = 12;
    static constexpr size_t CHECKSUM_LEN = 8;
    static constexpr size_t USTAR_LEN = 6;
    static constexpr size_t VERSION_LEN = 2;
    static constexpr size_t USERNAME_LEN = 32;

    static constexpr char FILE_TYPE_REGULAR = '0';
    static constexpr char FILE_TYPE_HARD_LINK = '1';
    static constexpr char FILE_TYPE_SYMBOLIC_LINK = '2';

public:
    TarArchive(void *data, size_t len);

    void enumerate_files(Fn<bool &, const char *, size_t, void *> callback);
    int extract_file(const char *name, void **data, size_t *len);

private:
    struct TarRecord {
        char name[PATH_LEN];
        char mode[MODE_LEN];
        char uid[UID_LEN];
        char gid[GID_LEN];
        char size[FILE_SIZE_LEN];
        char mod_time[MOD_TIME_LEN];
        char checksum[CHECKSUM_LEN];
        char type;
        char link[PATH_LEN];
        char ustar[USTAR_LEN];
        char version[VERSION_LEN];
        char owner[USERNAME_LEN];
        char group[USERNAME_LEN];
        char major[8];
        char minor[8];
        char prefix[155];
    };

    union TarBlock {
        char data[RECORD_SIZE];
        TarRecord record;
    };

    static int64_t oct_string_to_int(const char *str, size_t len);

    TarBlock *m_data;
    size_t m_n_blocks;

    static_assert(sizeof(TarRecord) <= RECORD_SIZE);
};
