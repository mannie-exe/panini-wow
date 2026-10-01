"""Execute the built DLL's picking hook and a supplied Classic client's ray code.

Usage: python tests/picking_client_probe.py path/to/WoW.exe path/to/PaniniWoW.dll
Requires: pefile, unicorn. Client files are read only; no game is launched.
Win32 locks/protection and matrix unprojection are controlled stubs. All picking
logic, native bounds checks, camera translation, and ABI instructions execute.
"""

import math
from pathlib import Path
import struct
import sys

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP,
)


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def run(client_path, dll_path):
    client = pefile.PE(str(client_path))
    dll = pefile.PE(str(dll_path))
    require(client.OPTIONAL_HEADER.ImageBase == 0x400000, "client image base")
    require(client.FILE_HEADER.TimeDateStamp == 0x4510B6DB, "Classic build 5875")
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    for pe in (client, dll):
        base = pe.OPTIONAL_HEADER.ImageBase
        size = (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095
        uc.mem_map(base, size)
        uc.mem_write(base, pe.get_memory_mapped_image())
    arena, stack, stop, imports = 0x20000000, 0x30000000, 0x31000000, 0x32000000
    for address, size in ((arena, 0x10000), (stack, 0x10000), (stop, 0x1000), (imports, 0x10000)):
        uc.mem_map(address, size)

    def read32(address):
        return struct.unpack("<I", uc.mem_read(address, 4))[0]

    def write32(address, value):
        uc.mem_write(address, struct.pack("<I", value & 0xFFFFFFFF))

    def floats(address, *values):
        uc.mem_write(address, struct.pack("<" + "f" * len(values), *values))

    def read_floats(address, count):
        return struct.unpack("<" + "f" * count, uc.mem_read(address, count * 4))

    stubs = {}
    fail_api = [None]
    for descriptor in dll.DIRECTORY_ENTRY_IMPORT:
        for entry in descriptor.imports:
            address = imports + len(stubs) * 16
            stubs[address] = entry.name.decode() if entry.name else f"ordinal {entry.ordinal}"
            write32(entry.address, address)

    def finish_stub(cleanup, value=0):
        sp = uc.reg_read(UC_X86_REG_ESP)
        uc.reg_write(UC_X86_REG_EAX, value & 0xFFFFFFFF)
        uc.reg_write(UC_X86_REG_EIP, read32(sp))
        uc.reg_write(UC_X86_REG_ESP, sp + 4 + cleanup)

    def intercept(machine, address, size, data):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x7AD2C0:
            u, v = read_floats(sp + 4, 2)
            floats(machine.reg_read(UC_X86_REG_ECX), u, v, 0.0)
            floats(machine.reg_read(UC_X86_REG_EDX), u, v, 1.0)
            finish_stub(8)
            return
        name = stubs.get(address)
        if name is None:
            return
        if name in ("AcquireSRWLockShared", "ReleaseSRWLockShared",
                    "AcquireSRWLockExclusive", "ReleaseSRWLockExclusive"):
            finish_stub(4)
        elif name == "VirtualProtect":
            require(read32(sp+4) == 0x48130C and read32(sp+8) == 5, "patch range")
            if fail_api[0] == name:
                fail_api[0] = None
                finish_stub(16, 0)
                return
            write32(read32(sp+16), 0x20)
            finish_stub(16, 1)
        elif name == "GetCurrentProcess":
            finish_stub(0, -1)
        elif name == "FlushInstructionCache":
            require(read32(sp+8) == 0x48130C and read32(sp+12) == 5, "flush range")
            if fail_api[0] == name:
                fail_api[0] = None
                finish_stub(12, 0)
                return
            finish_stub(12, 1)
        elif name == "memcmp":
            a, b, length = (read32(sp + offset) for offset in (4, 8, 12))
            left, right = bytes(uc.mem_read(a, length)), bytes(uc.mem_read(b, length))
            finish_stub(0, (left > right) - (left < right))
        else:
            raise AssertionError(f"unexpected imported call: {name}")

    uc.hook_add(UC_HOOK_CODE, intercept)
    exports = {entry.name.decode(): dll.OPTIONAL_HEADER.ImageBase + entry.address
               for entry in dll.DIRECTORY_ENTRY_EXPORT.symbols if entry.name}

    def exported(prefix):
        matches = [address for name, address in exports.items() if prefix in name]
        require(len(matches) == 1, f"export {prefix}")
        return matches[0]

    saved_registers = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)

    def call(address, arguments=(), this=0, cleanup=0):
        sp = stack + 0xF000
        uc.mem_write(sp, struct.pack("<" + "I" * (len(arguments)+1), stop, *arguments))
        uc.reg_write(UC_X86_REG_ESP, sp)
        uc.reg_write(UC_X86_REG_ECX, this)
        for register in saved_registers:
            uc.reg_write(register, 0x12345678)
        uc.emu_start(address, stop, count=100000)
        require(uc.reg_read(UC_X86_REG_EIP) == stop, "function returned")
        require(uc.reg_read(UC_X86_REG_ESP) == sp + 4 + cleanup, "stack cleanup")
        for register in saved_registers:
            require(uc.reg_read(register) == 0x12345678, "callee-saved register")
        return uc.reg_read(UC_X86_REG_EAX)

    install, publish, invalidate = (exported(name) for name in
                                   ("Picking_Install", "Picking_Publish", "Picking_Invalidate"))
    original_call = bytes(uc.mem_read(0x48130C, 9))
    require(call(install, [0]) == 0, "reject missing image")
    timestamp = 0x400000 + client.DOS_HEADER.e_lfanew + 8
    for address in (timestamp, 0x48130D, 0x481414):
        original = bytes(uc.mem_read(address, 1))
        uc.mem_write(address, bytes([original[0] ^ 1]))
        require(call(install, [0x400000]) == 0, "reject changed build/call/function")
        uc.mem_write(address, original)
        require(bytes(uc.mem_read(0x48130C, 9)) == original_call, "rejection preserves code")
    for api in ("VirtualProtect", "FlushInstructionCache"):
        fail_api[0] = api
        require(call(install, [0x400000]) == 0, f"reject failed {api}")
        require(bytes(uc.mem_read(0x48130C, 9)) == original_call, "failed installation preserves code")
    require(call(install, [0x400000]) == 1, "install production hook")
    hook = (0x481311 + read32(0x48130D)) & 0xFFFFFFFF
    require(hook != 0x4813B0, "call redirected")

    frame, camera, near, far, projection = arena, arena+0x8000, arena+0x9000, arena+0x9020, arena+0x9100
    write32(frame+0x65B8, camera)
    write32(0xB4B2BC, frame)
    floats(camera+8, 10.0, 20.0, 30.0)
    floats(frame+0x390, 0.0, 0.0, 0.45, 0.8)
    floats(0x832A44, 0.8, 0.45)

    def ray(address, x=0.4, y=0.3375):
        bits = struct.unpack("<II", struct.pack("<ff", x, y))
        return call(address, [*bits, near, far], this=frame, cleanup=16)

    def expect_ray(address, expected_x, expected_y):
        require(ray(address) == 1, "valid ray")
        actual = read_floats(near, 3) + read_floats(far, 3)
        expected = (10+expected_x, 20+expected_y, 30, 10+expected_x, 20+expected_y, 31)
        require(all(abs(a-b) < 0.00001 for a, b in zip(actual, expected)), f"ray coordinates: {actual}")

    expect_ray(0x4813B0, 0.5, 0.75)
    expect_ray(hook, 0.5, 0.75)
    half_tan = math.tan(2.82/2)
    longitude = math.atan(half_tan*16/9)
    zoom = half_tan*16/9 / (1.5*math.sin(longitude)/(0.5+math.cos(longitude)))
    floats(projection, 0.5, half_tan, zoom, 0.0, 16/9)
    call(publish, [frame, projection])
    expected_y = 0.5 + 0.25/zoom
    for _ in range(1000):
        expect_ray(hook, 0.5, expected_y)
    floats(frame+0x390, 0.05, 0.1, 0.4, 0.7)
    expect_ray(hook, (0.4-0.1)/0.6, (expected_y*0.45-0.05)/0.35)
    floats(frame+0x390, 0.0, 0.0, 0.45, 0.8)
    call(invalidate)
    expect_ray(hook, 0.5, 0.75)
    call(publish, [frame, projection])
    write32(0xB4B2BC, frame+16)
    expect_ray(hook, 0.5, 0.75)
    write32(0xB4B2BC, frame)
    floats(camera+0x100+8, 10.0, 20.0, 30.0)
    write32(frame+0x65B8, camera+0x100)
    expect_ray(hook, 0.5, 0.75)
    write32(frame+0x65B8, camera)
    floats(0x832A44, 0.9)
    expect_ray(hook, 0.5, 0.75)
    call(publish, [frame, projection])  # mismatched render aspect invalidates
    floats(0x832A44, 0.8)
    expect_ray(hook, 0.5, 0.75)
    floats(projection, 0.0)
    call(publish, [frame, projection])
    expect_ray(hook, 0.5, 0.75)
    floats(projection, 0.5, math.tan(1.57/2), 1.0, 0.0, 16/9)
    call(publish, [frame, projection])
    floats(near, -123, -123, -123)
    floats(far, -123, -123, -123)
    require(ray(hook, 0.8, 0.225) == 0, "black border returns no hit")
    require(ray(0x4813B0, -0.1, 0.225) == 0, "native outside-frame returns no hit")
    require(read_floats(near, 3) == (-123,)*3 and read_floats(far, 3) == (-123,)*3,
            "no-hit leaves output vectors untouched")
    print(f"PASS {client_path.name} + {dll_path}: patch guards, 1000 native rays, ABI, viewport, "
          "camera/frame/resize invalidation, disabled, black borders")


if __name__ == "__main__":
    require(len(sys.argv) == 3, __doc__)
    run(Path(sys.argv[1]), Path(sys.argv[2]))
