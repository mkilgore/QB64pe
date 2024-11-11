
#include "audio.h"
#include "miniaudio.h"
#include "vfs.h"
#include "framework.h"
#include "logging.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <stack>
#include <string_view>
#include <map>
#include <utility>
#include <vector>

struct fileState {
    ma_int64 offset;
    uint64_t key;
};

struct pe_vfs {
    // Has to be first entry
    ma_vfs_callbacks cb;
    ma_allocation_callbacks allocCb;

    uint64_t nextFd;

    std::map<uint64_t, fileState> fileMap;
    BufferMap *map;
};

static ma_result peVFSonOpen(ma_vfs *pVFS, const char *pFilePath, ma_uint32 openMode, ma_vfs_file *pFile) {
    pe_vfs *pe = (pe_vfs *)pVFS;

    // Transform pFilePath, look-up in BufferMap
    uint64_t key = strtoull(pFilePath, NULL, 10);

    AUDIO_DEBUG_PRINT("File path %s, opening buffer %llu", pFilePath, key);
    auto [buffer, bufferSize] = pe->map->GetBuffer(key);

    if (buffer == nullptr)
        return MA_DOES_NOT_EXIST;

    pe->nextFd++;
    pe->fileMap[pe->nextFd].key = key;

    AUDIO_DEBUG_PRINT("Opening fd %llu", pe->nextFd);

    *pFile = (ma_vfs_file)pe->nextFd;
    return MA_SUCCESS;
}

static ma_result peVFSonClose(ma_vfs *pVFS, ma_vfs_file file) {
    pe_vfs *pe = (pe_vfs *)pVFS;
    uint64_t fd = (uint64_t)file;

    AUDIO_DEBUG_PRINT("Closing fd %llu", fd);

    pe->fileMap.erase(fd);
    return MA_SUCCESS;
}

static ma_result peVFSonRead(ma_vfs *pVFS, ma_vfs_file file, void *pDst, size_t sizeInBytes, size_t *pBytesRead) {
    pe_vfs *pe = (pe_vfs *)pVFS;
    uint64_t fd = (uint64_t)file;
    fileState *state = &pe->fileMap[fd];

    //AUDIO_DEBUG_PRINT("Reading fd %llu, size: %zd", fd, sizeInBytes);

    auto [buffer, bufferSize] = pe->map->GetBuffer(state->key);

    if (bufferSize <= state->offset) {
        *pBytesRead = 0;
        return MA_SUCCESS;
    }

    if (sizeInBytes <= bufferSize - state->offset) {
        memcpy(pDst, (const char *)buffer + state->offset, sizeInBytes);
        *pBytesRead = sizeInBytes;
        state->offset += *pBytesRead;
        AUDIO_DEBUG_PRINT("Reading %zd bytes, offset: %lld", sizeInBytes, state->offset);
        return MA_SUCCESS;
    }

    size_t total = bufferSize - state->offset;
    memcpy(pDst, (const char *)buffer + state->offset, total);
    *pBytesRead = total;
    state->offset += *pBytesRead;
    AUDIO_DEBUG_PRINT("Reading %zd bytes, at end! offset: %lld", total, state->offset);
    return MA_SUCCESS;
}

static ma_result peVFSonSeek(ma_vfs *pVFS, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin) {
    pe_vfs *pe = (pe_vfs *)pVFS;
    uint64_t fd = (uint64_t)file;
    fileState *state = &pe->fileMap[fd];

    auto [buffer, bufferSize] = pe->map->GetBuffer(state->key);

    switch (origin) {
        case ma_seek_origin_start:
            state->offset = offset;
            break;

        case ma_seek_origin_current:
            state->offset = state->offset + offset;
            break;

        case ma_seek_origin_end:
            state->offset = bufferSize + offset;
            break;
    }

    if (state->offset < 0)
        state->offset = 0;

    if (state->offset > bufferSize)
        state->offset = bufferSize;

    AUDIO_DEBUG_PRINT("Seek %llu, kind: %d, offset: %lld, result: %lld", fd, origin, offset, state->offset);
    return MA_SUCCESS;
}

static ma_result peVFSonTell(ma_vfs *pVFS, ma_vfs_file file, ma_int64 *pCursor) {
    pe_vfs *pe = (pe_vfs *)pVFS;
    uint64_t fd = (uint64_t)file;
    fileState *state = &pe->fileMap[fd];
    AUDIO_DEBUG_PRINT("Tell %llu, offset: %lld", fd, state->offset);

    *pCursor = state->offset;

    return MA_SUCCESS;
}

static ma_result peVFSonInfo(ma_vfs *pVFS, ma_vfs_file file, ma_file_info *pInfo) {
    pe_vfs *pe = (pe_vfs *)pVFS;
    uint64_t fd = (uint64_t)file;
    fileState *state = &pe->fileMap[fd];
    AUDIO_DEBUG_PRINT("info %llu", fd);

    auto [buffer, bufferSize] = pe->map->GetBuffer(state->key);

    pInfo->sizeInBytes = bufferSize;

    return MA_SUCCESS;
}

ma_vfs *pe_vfs_create(BufferMap *map)
{
    pe_vfs *vfs = new pe_vfs();
    vfs->map = map;
    vfs->cb.onOpen = peVFSonOpen;
    vfs->cb.onClose = peVFSonClose;
    vfs->cb.onRead = peVFSonRead;
    vfs->cb.onSeek = peVFSonSeek;
    vfs->cb.onTell = peVFSonTell;
    vfs->cb.onInfo = peVFSonInfo;

    return (ma_vfs *)vfs;
}

void pe_vfs_destroy(ma_vfs *vfs) {
     delete (pe_vfs *)vfs;
}
