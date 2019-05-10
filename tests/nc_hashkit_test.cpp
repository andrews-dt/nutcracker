#include <nc_hashkit.h>

int main(int argc, char **argv)
{
    NcHashKit kit;
    const char *key = "af;ahjgkhalgj;fgkerabgjnalkflkaglnfaljc;lla;cx;sdl;aj";
    size_t key_length = strlen(key);

    uint32_t value = kit.hash(HASH_ONE_AT_A_TIME, key, key_length);
    LOG_DEBUG("HASH_ONE_AT_A_TIME value : %ld", value);
    value = kit.hash(HASH_MD5, key, key_length);
    LOG_DEBUG("HASH_MD5 value : %ld", value);
    value = kit.hash(HASH_CRC16, key, key_length);
    LOG_DEBUG("HASH_CRC16 value : %ld", value);
    value = kit.hash(HASH_CRC32, key, key_length);
    LOG_DEBUG("HASH_CRC32 value : %ld", value);
    value = kit.hash(HASH_CRC32A, key, key_length);
    LOG_DEBUG("HASH_CRC32A value : %ld", value);
    value = kit.hash(HASH_FNV1_64, key, key_length);
    LOG_DEBUG("HASH_FNV1_64 value : %ld", value);
    value = kit.hash(HASH_FNV1A_64, key, key_length);
    LOG_DEBUG("HASH_FNV1A_64 value : %ld", value);
    value = kit.hash(HASH_FNV1_32, key, key_length);
    LOG_DEBUG("HASH_FNV1_32 value : %ld", value);
    value = kit.hash(HASH_FNV1A_32, key, key_length);
    LOG_DEBUG("HASH_FNV1A_32 value : %ld", value);
    value = kit.hash(HASH_HSIEH, key, key_length);
    LOG_DEBUG("HASH_HSIEH value : %ld", value);
    value = kit.hash(HASH_MURMUR, key, key_length);
    LOG_DEBUG("HASH_MURMUR value : %ld", value);
    value = kit.hash(HASH_JENKINS, key, key_length);
    LOG_DEBUG("HASH_JENKINS value : %ld", value);

    return 0;
}