"""Verify each embedded Windows icon image against the approved ICO bytes."""
import ctypes
from ctypes import wintypes as W
from pathlib import Path
import struct
import sys

api = ctypes.WinDLL("kernel32", use_last_error=True)
api.LoadLibraryExW.argtypes = [W.LPCWSTR, W.HANDLE, W.DWORD]
api.LoadLibraryExW.restype = W.HMODULE
api.FreeLibrary.argtypes = [W.HMODULE]
api.FindResourceW.argtypes = [W.HMODULE, W.LPCVOID, W.LPCVOID]
api.FindResourceW.restype = W.HRSRC
api.SizeofResource.argtypes = [W.HMODULE, W.HRSRC]
api.SizeofResource.restype = W.DWORD
api.LoadResource.argtypes = [W.HMODULE, W.HRSRC]
api.LoadResource.restype = W.HGLOBAL
api.LockResource.argtypes = [W.HGLOBAL]
api.LockResource.restype = W.LPVOID
callback_type = ctypes.WINFUNCTYPE(W.BOOL, W.HMODULE, W.LPCVOID, W.LPCVOID, W.LPARAM)
api.EnumResourceNamesW.argtypes = [W.HMODULE, W.LPCVOID, callback_type, W.LPARAM]

def resource(module, kind, name):
    handle = api.FindResourceW(module, name, kind)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    data = api.LockResource(api.LoadResource(module, handle))
    return ctypes.string_at(data, api.SizeofResource(module, handle))

ico = Path(sys.argv[1]).read_bytes()
assert struct.unpack_from("<HH", ico) == (0, 1)
expected = set()
for i in range(struct.unpack_from("<H", ico, 4)[0]):
    size, offset = struct.unpack_from("<II", ico, 6+i*16+8)
    expected.add(ico[offset:offset+size])
assert len(expected) == 7
for executable in sys.argv[2:]:
    module = api.LoadLibraryExW(str(Path(executable).resolve()), None, 2)
    if not module:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        groups = []
        @callback_type
        def collect(handle, kind, name, param):
            groups.append(resource(handle, 14, name))
            return True
        assert api.EnumResourceNamesW(module, 14, collect, 0)
        assert len(groups) == 1
        group = groups[0]
        actual = set()
        for i in range(struct.unpack_from("<H", group, 4)[0]):
            icon_id = struct.unpack_from("<H", group, 6+i*14+12)[0]
            actual.add(resource(module, 3, icon_id))
        assert actual == expected, f"Icon mismatch: {executable}"
        print(f"PASS seven approved icon images embedded: {executable}")
    finally:
        api.FreeLibrary(module)
