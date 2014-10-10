import os
import ctypes
import shutil

class DBException(Exception):
    pass

class DBC(ctypes.Structure):
    _fields_ = [
        ('db_size', ctypes.c_size_t),
        ('chunk_size', ctypes.c_size_t),
    ]

class Database(object):
    def_path = './libmydb.so'

    def format_error(self):
        return "Error!"

    def __init__(self, path=None):
        self.so_path = path if path else self.def_path
        try:
            assert(os.path.exists(self.so_path))
        except AssertionError:
            raise DBException("Can't find SO ad %s", repr(self.so_path))
        self.dll = ctypes.CDLL(self.so_path)
        self.exc = DBException
        a = DBC(16*1024*1024, 4096)
        self.db = self.dll.dbcreate('my.db', a)
        if not self.db:
            raise self.exc("Can't create DB")
        self.fput = self.dll.db_put
        self.fput.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.c_char_p, ctypes.c_size_t
        ]
        self.fget = self.dll.db_get
        self.fget.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_char_p),
            ctypes.POINTER(ctypes.c_size_t)
        ]
        self.fdel = self.dll.db_del
        self.fput.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p, ctypes.c_size_t,
        ]

    def put(self, k, v):
        rc = self.fput(self.db, k, len(k), v, len(v))
        if rc == -1:
            raise self.exc(self.format_error())

    def get(self, k):
        v     = ctypes.c_char_p()
        v_len = ctypes.c_size_t()
        rc = self.fget(self.db, k, len(k), ctypes.byref(v), ctypes.byref(v_len))
        if rc == -1:
            raise self.exc(self.format_error())
        return ctypes.string_at(v, v_len.value)

    def delete(self, k):
        rc = self.fdel(self.db, k, len(k))
        if rc == -1:
            raise self.exc(self.format_error())

    def cleanup(self):
        if os.path.exists('my.db'):
            shutil.rmtree('my.db')

    def close(self):
        if 'db' in dir(self) and self.db:
            self.dll.db_close(self.db)

    def __del__(self):
        self.cleanup()
        self.close()

#############################################
# Child class example for Sophia (Checker)  #
#   Repo: http://github.com/pmwkaa/sophia/  #
# Docs: http://sphia.org/documentation.html #
#############################################
class SophiaException(DBException):
    pass

class Sophia(Database):
    SPDIR     = 0x00
    SPO_RDWR   = 0x02
    SPO_CREAT  = 0x04
    def_path = './libsophia.so'

    def format_error(self):
        assert ('dll' in dir(self))
        assert ('env' in dir(self))
        return "Error: %s" % self.dll.sp_error(self.env)

    def __init__(self, path=None):
        self.so_path = path if path else self.def_path
        self.exc  = SophiaException
        try:
            assert(os.path.exists(self.so_path))
        except AssertionError:
            raise DBException("Can't find SO at %s" % repr(self.so_path))
        print self.so_path
        self.dll  = ctypes.CDLL(self.so_path)
        self.env  = self.dll.sp_env()
        if not self.env:
            raise self.exc('Failed to create ENV')
        rc = self.dll.sp_ctl(self.env, self.SPDIR, self.SPO_CREAT | self.SPO_RDWR, './db')
        if rc == -1:
            raise self.exc(self.format_error())
        self.db   = self.dll.sp_open(self.env)
        if not self.db:
            raise self.exc(self.format_error())
        self.fput = self.dll.sp_set
        self.fput.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.c_char_p, ctypes.c_size_t
        ]
        self.fget = self.dll.sp_get
        self.fget.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p, ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_char_p),
            ctypes.POINTER(ctypes.c_size_t)
        ]
        self.fdel = self.dll.sp_delete
        self.fput.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p, ctypes.c_size_t,
        ]

    def cleanup(self):
        if os.path.exists('./db'):
            shutil.rmtree('./db')

    def close(self):
        if 'db' in dir(self) and self.db:
            self.dll.sp_destroy(self.db)
        if 'env' in dir(self) and self.env:
            self.dll.sp_destroy(self.env)

# if __name__ == '__main__':
#     a = Sophia()
#     a.put('1', 'hello, mike')
#     assert(a.get('1') == 'hello, mike')
#     a.put('1', 'hello, mike_bro')
#     assert(a.get('1') == 'hello, mike_bro')
#     a.delete('1')
#     assert(not a.get('1'))
#     a.delete('1')
#     a.close()
#     a.cleanup()

# if __name__ == '__main__':
#     a = Database()
#     a.put('1', 'hello, mike')
#     assert(a.get('1') == 'hello, mike')
#     print a.get('1')
#     a.put('1', 'hello, mike_bro')
#     assert(a.get('1') == 'hello, mike_bro')
#     print a.get('1')
#     a.delete('1')
#     assert(not a.get('1'))
#     print a.get('1')
#     a.delete('1')
#     a.close()
#     a.cleanup()


