#include "blitzmax/embedded_runtime.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>

#if !DT_NODE_EXISTS(DT_NODELABEL(bmx_qspi_partition))
#error "NRF52.Storage.LittleFS requires a XIAO board with external QSPI flash"
#endif

#define BMX_LFS_MOUNT_POINT "/littlefs"
#define BMX_LFS_PATH_SIZE 256u
#define BMX_LFS_SCAN_SIZE 256u

typedef struct {
    struct fs_file_t file;
} BMXNrf52LittleFSFile;

typedef struct {
    struct fs_dir_t dir;
} BMXNrf52LittleFSDirectory;

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(bmx_lfs_config);
static struct fs_mount_t bmx_lfs_mount = {
    .type = FS_LITTLEFS,
    .fs_data = &bmx_lfs_config,
    .storage_dev = (void *)PARTITION_ID(bmx_qspi_partition),
    .mnt_point = BMX_LFS_MOUNT_POINT,
    .flags = FS_MOUNT_FLAG_NO_FORMAT,
};
static int32_t bmx_lfs_mounted;
static int32_t bmx_lfs_last_error;

static int32_t bmx_lfs_result(int32_t result) {
    bmx_lfs_last_error = result;
    return result;
}

static int bmx_lfs_blank(void) {
    const struct flash_area *area;
    int result = flash_area_open(PARTITION_ID(bmx_qspi_partition), &area);
    if (result < 0) return result;
    uint8_t bytes[BMX_LFS_SCAN_SIZE];
    int blank = 1;
    for (size_t offset = 0; offset < area->fa_size; offset += sizeof(bytes)) {
        size_t count = area->fa_size - offset;
        if (count > sizeof(bytes)) count = sizeof(bytes);
        result = flash_area_read(area, offset, bytes, count);
        if (result < 0) { blank = result; break; }
        for (size_t i = 0; i < count; ++i) {
            if (bytes[i] != 0xffu) { blank = 0; break; }
        }
        if (blank == 0) break;
    }
    flash_area_close(area);
    return blank;
}

static char *bmx_lfs_path(const BMXEmbeddedString *path) {
    if (!bmx_lfs_mounted || !path) return NULL;
    char *relative = (char *)bmx_embedded_string_to_utf8_string(path);
    if (!relative) return NULL;
    size_t length = strlen(relative);
    if (length > BMX_LFS_PATH_SIZE - sizeof(BMX_LFS_MOUNT_POINT) - 1u) {
        bbMemFree(relative);
        return NULL;
    }
    char *result = bbMemAlloc(BMX_LFS_PATH_SIZE);
    if (result) {
        const char *separator = length && relative[0] == '/' ? "" : "/";
        size_t prefix = sizeof(BMX_LFS_MOUNT_POINT) - 1u;
        memcpy(result, BMX_LFS_MOUNT_POINT, prefix);
        size_t slash = strlen(separator);
        memcpy(result + prefix, separator, slash);
        memcpy(result + prefix + slash, relative, length + 1u);
        if (fs_normalize_path(result, result, BMX_LFS_PATH_SIZE) < 0 ||
                strncmp(result, BMX_LFS_MOUNT_POINT, prefix) != 0 ||
                (result[prefix] != '/' && result[prefix] != '\0')) {
            bbMemFree(result);
            result = NULL;
        }
    }
    bbMemFree(relative);
    return result;
}

int32_t bmx_nrf52_littlefs_mount(int32_t format_blank) {
    if (bmx_lfs_mounted) return bmx_lfs_result(0);
    bmx_lfs_mount.flags = FS_MOUNT_FLAG_NO_FORMAT;
    int result = fs_mount(&bmx_lfs_mount);
    if (result < 0 && format_blank) {
        int blank = bmx_lfs_blank();
        if (blank < 0) return bmx_lfs_result(blank);
        if (blank) {
            bmx_lfs_mount.flags = 0;
            result = fs_mount(&bmx_lfs_mount);
        }
    }
    bmx_lfs_mount.flags = FS_MOUNT_FLAG_NO_FORMAT;
    if (result == 0) bmx_lfs_mounted = 1;
    return bmx_lfs_result(result);
}

int32_t bmx_nrf52_littlefs_unmount(void) {
    if (!bmx_lfs_mounted) return bmx_lfs_result(0);
    int result = fs_unmount(&bmx_lfs_mount);
    if (result == 0) bmx_lfs_mounted = 0;
    return bmx_lfs_result(result);
}

int32_t bmx_nrf52_littlefs_format(void) {
    int result = bmx_nrf52_littlefs_unmount();
    if (result < 0) return result;
    result = fs_mkfs(FS_LITTLEFS, PARTITION_ID(bmx_qspi_partition),
        &bmx_lfs_config, 0);
    if (result < 0) return bmx_lfs_result(result);
    return bmx_nrf52_littlefs_mount(0);
}

int32_t bmx_nrf52_littlefs_is_mounted(void) { return bmx_lfs_mounted; }
int32_t bmx_nrf52_littlefs_last_error(void) { return bmx_lfs_last_error; }

int64_t bmx_nrf52_littlefs_capacity(void) {
    struct fs_statvfs stat;
    int result = bmx_lfs_mounted ? fs_statvfs(BMX_LFS_MOUNT_POINT, &stat) : -ENODEV;
    bmx_lfs_result(result);
    return result < 0 ? -1 : (int64_t)stat.f_blocks * stat.f_frsize;
}

int64_t bmx_nrf52_littlefs_used(void) {
    struct fs_statvfs stat;
    int result = bmx_lfs_mounted ? fs_statvfs(BMX_LFS_MOUNT_POINT, &stat) : -ENODEV;
    bmx_lfs_result(result);
    return result < 0 ? -1 : (int64_t)(stat.f_blocks - stat.f_bfree) * stat.f_frsize;
}

void *bmx_nrf52_littlefs_open(const BMXEmbeddedString *path, int32_t readable,
        int32_t write_mode) {
    char *native_path = bmx_lfs_path(path);
    if (!native_path) { bmx_lfs_result(-EINVAL); return NULL; }
    BMXNrf52LittleFSFile *handle = bbMemAlloc(sizeof(*handle));
    if (!handle) { bbMemFree(native_path); bmx_lfs_result(-ENOMEM); return NULL; }
    fs_file_t_init(&handle->file);
    fs_mode_t flags = readable ? FS_O_READ : 0;
    if (write_mode == 1) flags |= FS_O_WRITE | FS_O_CREATE | FS_O_TRUNC;
    else if (write_mode == 2) flags |= FS_O_WRITE | FS_O_CREATE | FS_O_APPEND;
    int result = fs_open(&handle->file, native_path, flags);
    bbMemFree(native_path);
    if (result < 0) { bbMemFree(handle); bmx_lfs_result(result); return NULL; }
    bmx_lfs_result(0);
    return handle;
}

int32_t bmx_nrf52_littlefs_close(void *opaque) {
    if (!opaque) return bmx_lfs_result(0);
    BMXNrf52LittleFSFile *handle = opaque;
    int result = fs_close(&handle->file);
    bbMemFree(handle);
    return bmx_lfs_result(result);
}

int64_t bmx_nrf52_littlefs_read(void *opaque, void *buffer, int64_t count) {
    if (!opaque || !buffer || count < 0 || count > INT32_MAX) return bmx_lfs_result(-EINVAL);
    ssize_t result = fs_read(&((BMXNrf52LittleFSFile *)opaque)->file, buffer, (size_t)count);
    bmx_lfs_result(result < 0 ? result : 0);
    return result;
}

int64_t bmx_nrf52_littlefs_write(void *opaque, void *buffer, int64_t count) {
    if (!opaque || !buffer || count < 0 || count > INT32_MAX) return bmx_lfs_result(-EINVAL);
    ssize_t result = fs_write(&((BMXNrf52LittleFSFile *)opaque)->file, buffer, (size_t)count);
    bmx_lfs_result(result < 0 ? result : 0);
    return result;
}

int64_t bmx_nrf52_littlefs_position(void *opaque) {
    if (!opaque) return bmx_lfs_result(-EINVAL);
    off_t result = fs_tell(&((BMXNrf52LittleFSFile *)opaque)->file);
    bmx_lfs_result(result < 0 ? result : 0);
    return result;
}

int64_t bmx_nrf52_littlefs_size(void *opaque) {
    if (!opaque) return bmx_lfs_result(-EINVAL);
    struct fs_file_t *file = &((BMXNrf52LittleFSFile *)opaque)->file;
    off_t position = fs_tell(file);
    if (position < 0) return bmx_lfs_result(position);
    int result = fs_seek(file, 0, FS_SEEK_END);
    if (result < 0) return bmx_lfs_result(result);
    off_t size = fs_tell(file);
    result = fs_seek(file, position, FS_SEEK_SET);
    bmx_lfs_result(result < 0 ? result : (size < 0 ? size : 0));
    return result < 0 ? result : size;
}

int64_t bmx_nrf52_littlefs_seek(void *opaque, int64_t offset, int32_t whence) {
    if (!opaque || offset < INT32_MIN || offset > INT32_MAX || whence < 0 || whence > 2)
        return bmx_lfs_result(-EINVAL);
    struct fs_file_t *file = &((BMXNrf52LittleFSFile *)opaque)->file;
    int result = fs_seek(file, (off_t)offset, whence == 0 ? FS_SEEK_SET :
        whence == 1 ? FS_SEEK_CUR : FS_SEEK_END);
    if (result < 0) return bmx_lfs_result(result);
    return bmx_nrf52_littlefs_position(opaque);
}

int32_t bmx_nrf52_littlefs_resize(void *opaque, int64_t size) {
    if (!opaque || size < 0 || size > INT32_MAX) return bmx_lfs_result(-EINVAL);
    return bmx_lfs_result(fs_truncate(&((BMXNrf52LittleFSFile *)opaque)->file,
        (off_t)size));
}

int32_t bmx_nrf52_littlefs_flush(void *opaque) {
    if (!opaque) return bmx_lfs_result(-EINVAL);
    return bmx_lfs_result(fs_sync(&((BMXNrf52LittleFSFile *)opaque)->file));
}

int32_t bmx_nrf52_littlefs_stat(const BMXEmbeddedString *path, int32_t *type,
        int64_t *size, int64_t *modified, int64_t *created, int64_t *accessed) {
    char *native_path = bmx_lfs_path(path);
    if (!native_path) return bmx_lfs_result(-EINVAL);
    struct fs_dirent entry;
    int result = fs_stat(native_path, &entry);
    bbMemFree(native_path);
    if (result == 0) {
        if (type) *type = entry.type == FS_DIR_ENTRY_DIR ? 2 : 1;
        if (size) *size = entry.size;
        if (modified) *modified = 0;
        if (created) *created = 0;
        if (accessed) *accessed = 0;
    }
    return bmx_lfs_result(result);
}

int32_t bmx_nrf52_littlefs_set_time(const BMXEmbeddedString *path,
        int64_t time, int32_t time_type) {
    (void)path; (void)time; (void)time_type;
    return bmx_lfs_result(-ENOTSUP);
}

int32_t bmx_nrf52_littlefs_mkdir(const BMXEmbeddedString *path) {
    char *native_path = bmx_lfs_path(path);
    if (!native_path) return bmx_lfs_result(-EINVAL);
    int result = fs_mkdir(native_path);
    bbMemFree(native_path);
    return bmx_lfs_result(result);
}

int32_t bmx_nrf52_littlefs_remove(const BMXEmbeddedString *path) {
    char *native_path = bmx_lfs_path(path);
    if (!native_path) return bmx_lfs_result(-EINVAL);
    int result = fs_unlink(native_path);
    bbMemFree(native_path);
    return bmx_lfs_result(result);
}

int32_t bmx_nrf52_littlefs_rename(const BMXEmbeddedString *old_path,
        const BMXEmbeddedString *new_path) {
    char *from = bmx_lfs_path(old_path);
    char *to = bmx_lfs_path(new_path);
    if (!from || !to) {
        if (from) bbMemFree(from);
        if (to) bbMemFree(to);
        return bmx_lfs_result(-EINVAL);
    }
    int result = fs_rename(from, to);
    bbMemFree(from);
    bbMemFree(to);
    return bmx_lfs_result(result);
}

void *bmx_nrf52_littlefs_directory_open(const BMXEmbeddedString *path) {
    char *native_path = bmx_lfs_path(path);
    if (!native_path) { bmx_lfs_result(-EINVAL); return NULL; }
    BMXNrf52LittleFSDirectory *handle = bbMemAlloc(sizeof(*handle));
    if (!handle) { bbMemFree(native_path); bmx_lfs_result(-ENOMEM); return NULL; }
    fs_dir_t_init(&handle->dir);
    int result = fs_opendir(&handle->dir, native_path);
    bbMemFree(native_path);
    if (result < 0) { bbMemFree(handle); bmx_lfs_result(result); return NULL; }
    bmx_lfs_result(0);
    return handle;
}

const BMXEmbeddedString *bmx_nrf52_littlefs_directory_next(void *opaque) {
    if (!opaque) { bmx_lfs_result(-EINVAL); return &bmx_embedded_empty_string; }
    struct fs_dirent entry;
    int result = fs_readdir(&((BMXNrf52LittleFSDirectory *)opaque)->dir, &entry);
    bmx_lfs_result(result);
    if (result < 0 || !entry.name[0]) return &bmx_embedded_empty_string;
    return bmx_embedded_string_from_utf8_string((const uint8_t *)entry.name);
}

int32_t bmx_nrf52_littlefs_directory_close(void *opaque) {
    if (!opaque) return bmx_lfs_result(0);
    BMXNrf52LittleFSDirectory *handle = opaque;
    int result = fs_closedir(&handle->dir);
    bbMemFree(handle);
    return bmx_lfs_result(result);
}
