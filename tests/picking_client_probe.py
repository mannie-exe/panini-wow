"""Execute the built DLL's picking hook and a supplied client's ray code.

Usage: python tests/picking_client_probe.py path/to/WoW.exe path/to/PaniniWoW.dll
Requires: pefile, unicorn. Client files are read only; no game is launched.
Win32 locks/protection and matrix unprojection are controlled stubs. All picking
logic, caller argument setup, native bounds checks, camera translation, and ABI
instructions execute.
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
    profiles = {
        0x4510B6DB: dict(name="Classic 5875", ray_call=0x48130C, ray=0x4813B0,
                         world_frame=0xB4B2BC, device_width=0x832A44,
                         camera_offset=0x65B8, rect_offset=0x390, unproject=0x7AD2C0,
                         ray_setup=0x4812D0, near_local=-0x10, far_local=-0x1C, no_hit=0x481388),
        0x4C2452FE: dict(name="WotLK 12340", ray_call=0x4F9EC1, ray=0x4F6450,
                         world_frame=0xB7436C, device_width=0xAC0CB4,
                         camera_offset=0x7E20, rect_offset=0x320, unproject=0x4BF0F0,
                         ray_setup=0x4F9E93, near_local=-0xC, far_local=-0x18, no_hit=0x4F9F35),
    }
    profile = profiles.get(client.FILE_HEADER.TimeDateStamp)
    require(profile is not None, "supported client build")
    is_wotlk = client.FILE_HEADER.TimeDateStamp == 0x4C2452FE
    ray_call, native_ray = profile["ray_call"], profile["ray"]
    world_frame, device_width = profile["world_frame"], profile["device_width"]
    camera_offset, rect_offset = profile["camera_offset"], profile["rect_offset"]
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
    caller_check = {}
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
        if address == caller_check.get("target"):
            require(machine.reg_read(UC_X86_REG_ECX) == frame, "caller supplies WorldFrame in ECX")
            actual = tuple(read32(sp+offset) for offset in (0, 4, 8, 12, 16))
            require(actual == caller_check["arguments"], "caller supplies return address, x, y, near, far")
            caller_check["entered"] = True
        if address == profile["unproject"]:
            u, v = read_floats(sp + 4, 2)
            # Classic uses fastcall outputs; WotLK passes all four args on
            # the stack and leaves cleanup to the caller (__cdecl).
            near_output = read32(sp+12) if is_wotlk else machine.reg_read(UC_X86_REG_ECX)
            far_output = read32(sp+16) if is_wotlk else machine.reg_read(UC_X86_REG_EDX)
            floats(near_output, u, v, 0.0)
            floats(far_output, u, v, 1.0)
            finish_stub(0 if is_wotlk else 8)
            return
        name = stubs.get(address)
        if name is None:
            return
        if name in ("AcquireSRWLockShared", "ReleaseSRWLockShared",
                    "AcquireSRWLockExclusive", "ReleaseSRWLockExclusive"):
            finish_stub(4)
        elif name == "VirtualProtect":
            require(read32(sp+4) == ray_call and read32(sp+8) == 5, "patch range")
            if fail_api[0] == name:
                fail_api[0] = None
                finish_stub(16, 0)
                return
            write32(read32(sp+16), 0x20)
            finish_stub(16, 1)
        elif name == "GetCurrentProcess":
            finish_stub(0, -1)
        elif name == "FlushInstructionCache":
            require(read32(sp+8) == ray_call and read32(sp+12) == 5, "flush range")
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
    frame, camera, near, far, projection = arena, arena+0x8000, arena+0x9000, arena+0x9020, arena+0x9100
    write32(frame+camera_offset, camera)
    write32(world_frame, frame)
    floats(camera+8, 10.0, 20.0, 30.0)
    floats(frame+rect_offset, 0.0, 0.0, 0.45, 0.8)
    floats(device_width, 0.8, 0.45)

    def ray(address, x=0.4, y=0.3375):
        bits = struct.unpack("<II", struct.pack("<ff", x, y))
        return call(address, [*bits, near, far], this=frame, cleanup=16)

    def expect_ray(address, expected_x, expected_y):
        require(ray(address) == 1, "valid ray")
        actual = read_floats(near, 3) + read_floats(far, 3)
        expected = (10+expected_x, 20+expected_y, 30, 10+expected_x, 20+expected_y, 31)
        require(all(abs(a-b) < 0.00001 for a, b in zip(actual, expected)), f"ray coordinates: {actual}")

    expect_ray(native_ray, 0.5, 0.75)
    half_tan = math.tan(2.82/2)
    longitude = math.atan(half_tan*16/9)
    zoom = half_tan*16/9 / (1.5*math.sin(longitude)/(0.5+math.cos(longitude)))
    print(f"BASELINE {profile['name']}: native source y=270.000; "
          f"rendered source y={(0.5-0.25/zoom)*1080:.3f} at display y=270 (1080p)")
    original_call = bytes(uc.mem_read(ray_call, 9))
    other_call = profiles[0x4510B6DB if is_wotlk else 0x4C2452FE]["ray_call"]
    untouched_call = bytes(uc.mem_read(other_call, 9))
    require(call(install, [0]) == 0, "reject missing image")
    timestamp = 0x400000 + client.DOS_HEADER.e_lfanew + 8
    for address in (timestamp, ray_call+1, native_ray+100):
        original = bytes(uc.mem_read(address, 1))
        uc.mem_write(address, bytes([original[0] ^ 1]))
        require(call(install, [0x400000]) == 0, "reject changed build/call/function")
        uc.mem_write(address, original)
        require(bytes(uc.mem_read(ray_call, 9)) == original_call, "rejection preserves code")
    for api in ("VirtualProtect", "FlushInstructionCache"):
        fail_api[0] = api
        require(call(install, [0x400000]) == 0, f"reject failed {api}")
        require(bytes(uc.mem_read(ray_call, 9)) == original_call, "failed installation preserves code")
    require(call(install, [0x400000]) == 1, "install production hook")
    require(bytes(uc.mem_read(other_call, 9)) == untouched_call, "only selected client call site is patched")
    hook = (ray_call+5 + read32(ray_call+1)) & 0xFFFFFFFF
    require(hook != native_ray, "call redirected")

    def expect_caller_ray(x, y, expected=None):
        # Start after unrelated hit-test setup, leaving the client's own
        # instructions to construct all four arguments and execute CALL/test/JE.
        bp, sp = stack + 0xD000, stack + 0xCF00
        near_local, far_local = bp + profile["near_local"], bp + profile["far_local"]
        floats(bp+8, x, y)
        floats(near_local, -123, -123, -123)
        floats(far_local, -123, -123, -123)
        for register in saved_registers:
            uc.reg_write(register, 0x12345678)
        uc.reg_write(UC_X86_REG_EBP, bp)
        uc.reg_write(UC_X86_REG_ESI, frame)
        uc.reg_write(UC_X86_REG_ECX, 0xBAD)
        uc.reg_write(UC_X86_REG_ESP, sp)
        bits = struct.unpack("<II", struct.pack("<ff", x, y))
        caller_check.update(target=hook, arguments=(ray_call+5, *bits, near_local, far_local), entered=False)
        continuation = ray_call+9 if expected is not None else profile["no_hit"]
        try:
            uc.emu_start(profile["ray_setup"], continuation, count=100000)
            require(caller_check["entered"], "patched CALL reaches hook")
            require(uc.reg_read(UC_X86_REG_EIP) == continuation, "native hit/no-hit branch")
            require(uc.reg_read(UC_X86_REG_EAX) == int(expected is not None), "caller receives ray result")
            require(uc.reg_read(UC_X86_REG_ESP) == sp, "caller stack restored")
            for register in saved_registers:
                value = bp if register == UC_X86_REG_EBP else frame if register == UC_X86_REG_ESI else 0x12345678
                require(uc.reg_read(register) == value, "caller registers preserved")
            actual = read_floats(near_local, 3) + read_floats(far_local, 3)
            result = ((10+expected[0], 20+expected[1], 30, 10+expected[0], 20+expected[1], 31)
                      if expected is not None else (0,)*6)
            require(all(abs(a-b) < 0.00001 for a, b in zip(actual, result)), "caller output vectors")
        finally:
            caller_check.clear()

    expect_ray(hook, 0.5, 0.75)
    expect_caller_ray(0.4, 0.3375, (0.5, 0.75))
    floats(projection, 0.5, half_tan, zoom, 0.0, 16/9)
    call(publish, [frame, projection])
    expected_y = 0.5 + 0.25/zoom
    expect_caller_ray(0.4, 0.3375, (0.5, expected_y))
    for _ in range(1000):
        expect_ray(hook, 0.5, expected_y)
    floats(frame+rect_offset, 0.05, 0.1, 0.4, 0.7)
    expect_ray(hook, (0.4-0.1)/0.6, (expected_y*0.45-0.05)/0.35)
    floats(frame+rect_offset, 0.0, 0.0, 0.45, 0.8)
    call(invalidate)
    expect_ray(hook, 0.5, 0.75)
    call(publish, [frame, projection])
    write32(world_frame, frame+16)
    expect_ray(hook, 0.5, 0.75)
    write32(world_frame, frame)
    floats(camera+0x100+8, 10.0, 20.0, 30.0)
    write32(frame+camera_offset, camera+0x100)
    expect_ray(hook, 0.5, 0.75)
    write32(frame+camera_offset, camera)
    floats(device_width, 0.9)
    expect_ray(hook, 0.5, 0.75)
    call(publish, [frame, projection])  # mismatched render aspect invalidates
    floats(device_width, 0.8)
    expect_ray(hook, 0.5, 0.75)
    floats(projection, 0.0)
    call(publish, [frame, projection])
    expect_ray(hook, 0.5, 0.75)
    expect_caller_ray(0.4, 0.3375, (0.5, 0.75))
    floats(projection, 0.5, math.tan(1.57/2), 1.0, 0.0, 16/9)
    call(publish, [frame, projection])
    floats(near, -123, -123, -123)
    floats(far, -123, -123, -123)
    require(ray(hook, 0.8, 0.225) == 0, "black border returns no hit")
    expect_caller_ray(0.8, 0.225)
    for x, y in ((-0.1, 0.225), (0.9, 0.225), (0.4, -0.1), (0.4, 0.5)):
        require(ray(native_ray, x, y) == 0, "native outside-frame returns no hit")
    require(read_floats(near, 3) == (-123,)*3 and read_floats(far, 3) == (-123,)*3,
            "no-hit leaves output vectors untouched")
    print(f"PASS {profile['name']} ({client_path.name}) + {dll_path}: patch guards, native caller, 1000 native rays, ABI, viewport, "
          "camera/frame/resize invalidation, disabled, black borders")


if __name__ == "__main__":
    require(len(sys.argv) == 3, __doc__)
    run(Path(sys.argv[1]), Path(sys.argv[2]))
