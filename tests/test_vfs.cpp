/*
 * VOS Unit Test — Virtual Filesystem
 */
#include <cassert>
#include <cstdio>
#include "core/vfs.h"

using namespace vos;

void test_init() {
    VirtualFS vfs;
    auto r = vfs.init();
    assert(r.ok());
    assert(vfs.exists("/"));
    assert(vfs.exists("/home"));
    assert(vfs.exists("/tmp"));
    assert(vfs.exists("/apps"));
    assert(vfs.exists("/system"));
    printf("[PASS] test_init\n");
}

void test_write_read() {
    VirtualFS vfs;
    vfs.init();

    ByteBuffer data = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    auto w = vfs.write_file("/home/test.txt", data);
    assert(w.ok());

    auto r = vfs.read_file("/home/test.txt");
    assert(r.ok());
    assert(r.value == data);
    printf("[PASS] test_write_read\n");
}

void test_delete() {
    VirtualFS vfs;
    vfs.init();

    ByteBuffer data = {1, 2, 3};
    vfs.write_file("/tmp/delete_me.bin", data);
    assert(vfs.exists("/tmp/delete_me.bin"));

    auto d = vfs.delete_file("/tmp/delete_me.bin");
    assert(d.ok());
    assert(!vfs.exists("/tmp/delete_me.bin"));
    printf("[PASS] test_delete\n");
}

void test_delete_not_found() {
    VirtualFS vfs;
    vfs.init();
    auto d = vfs.delete_file("/nonexistent");
    assert(!d.ok());
    assert(d.status == StatusCode::ERR_NOT_FOUND);
    printf("[PASS] test_delete_not_found\n");
}

void test_mkdir_and_list() {
    VirtualFS vfs;
    vfs.init();

    auto m = vfs.mkdir("/home/user");
    assert(m.ok());

    vfs.write_file("/home/user/file1.txt", {1});
    vfs.write_file("/home/user/file2.txt", {2});

    auto list = vfs.list_dir("/home/user");
    assert(list.ok());
    assert(list.value.size() == 2);
    printf("[PASS] test_mkdir_and_list\n");
}

void test_overwrite() {
    VirtualFS vfs;
    vfs.init();

    vfs.write_file("/home/data.bin", {0xAA});
    vfs.write_file("/home/data.bin", {0xBB, 0xCC});

    auto r = vfs.read_file("/home/data.bin");
    assert(r.ok());
    assert(r.value.size() == 2);
    assert(r.value[0] == 0xBB);
    printf("[PASS] test_overwrite\n");
}

void test_stats() {
    VirtualFS vfs;
    vfs.init();

    vfs.write_file("/home/a.txt", {1, 2, 3});
    vfs.write_file("/home/b.txt", {4, 5});

    assert(vfs.total_files() == 2);
    assert(vfs.total_size() == 5);
    printf("[PASS] test_stats\n");
}

int main() {
    printf("=== VFS Tests ===\n");
    test_init();
    test_write_read();
    test_delete();
    test_delete_not_found();
    test_mkdir_and_list();
    test_overwrite();
    test_stats();
    printf("All VFS tests passed!\n\n");
    return 0;
}
