from pathlib import Path
from datetime import datetime
import subprocess
import re
import sys
import time
import json

Import("env")

global_env = DefaultEnvironment()

PROJECT_DIR = Path(env["PROJECT_DIR"])
INI_PATH = PROJECT_DIR / "platformio.ini"

BUILD_START_EPOCH = time.time()
BUILD_START_MONO = time.perf_counter()

print("[export_versions] script loaded")


def _fmt_bytes(n):
    """将字节数格式化为更易读的字符串。"""
    try:
        n = int(n)
    except Exception:
        return str(n)

    if n >= 1024 * 1024:
        return f"{n} bytes ({n / 1024 / 1024:.2f} MB)"
    if n >= 1024:
        return f"{n} bytes ({n / 1024:.2f} KB)"
    return f"{n} bytes"


def _fmt_duration(seconds):
    """将秒数格式化为易读的耗时字符串。"""
    try:
        seconds = float(seconds)
    except Exception:
        return str(seconds)

    if seconds < 1:
        return f"{seconds:.3f} s"

    m, s = divmod(seconds, 60)
    h, m = divmod(int(m), 60)

    if h > 0:
        return f"{h}h {m}m {s:.3f}s"
    if m > 0:
        return f"{m}m {s:.3f}s"
    return f"{s:.3f} s"


def _fmt_hex(v):
    """将值转为十六进制字符串。"""
    try:
        if isinstance(v, str):
            v = v.strip()
            if v.lower().startswith("0x"):
                return v.lower()
            v = int(v, 0)
        else:
            v = int(v)
        return f"0x{v:x}"
    except Exception:
        return str(v)


def _parse_size_to_int(value):
    """将分区表中的大小字段解析为整数字节数。"""
    if value is None:
        return None

    s = str(value).strip()
    if not s:
        return None

    try:
        if s.lower().startswith("0x"):
            return int(s, 16)

        m = re.match(r"^(\d+)\s*([kKmM]?)$", s)
        if m:
            num = int(m.group(1))
            unit = m.group(2).lower()
            if unit == "k":
                return num * 1024
            if unit == "m":
                return num * 1024 * 1024
            return num

        return int(s, 0)
    except Exception:
        return None


def _parse_capacity_to_bytes(value):
    """将 16MB / 8MB / 320KB 之类的文本解析为字节数。"""
    if value is None:
        return None

    s = str(value).strip()
    if not s:
        return None

    m = re.match(r"^(\d+)\s*([kKmMgG]?[bB]?)?$", s)
    if not m:
        return _parse_size_to_int(s)

    num = int(m.group(1))
    unit = (m.group(2) or "").lower()

    if unit in ("kb", "k"):
        return num * 1024
    if unit in ("mb", "m"):
        return num * 1024 * 1024
    if unit in ("gb", "g"):
        return num * 1024 * 1024 * 1024
    return num


def _decode_subprocess_output(data):
    """解码子进程输出，兼容 Windows 下非 UTF-8 编码。"""
    if data is None:
        return ""

    if isinstance(data, str):
        return data

    for enc in ("utf-8", "gbk", "mbcs", "latin1"):
        try:
            return data.decode(enc)
        except Exception:
            pass

    return data.decode("utf-8", errors="replace")


def _get_project_options_dict(env_):
    """读取当前环境的 project options。"""
    try:
        return env_.GetProjectOptions(as_dict=True)
    except Exception:
        return {}


def _run_pio_pkg_list_text():
    """执行 `pio pkg list`，返回当前环境的依赖包文本输出。"""
    try:
        result = subprocess.run(
            [
                sys.executable,
                "-m",
                "platformio",
                "pkg",
                "list",
                "-e",
                env["PIOENV"],
            ],
            cwd=str(PROJECT_DIR),
            capture_output=True,
            text=False,
            check=True
        )
        print("[export_versions] pkg list executed")
        return _decode_subprocess_output(result.stdout)
    except subprocess.CalledProcessError as e:
        print("[export_versions] pio pkg list failed")
        print("[export_versions] returncode =", e.returncode)

        stdout_text = _decode_subprocess_output(e.stdout)
        stderr_text = _decode_subprocess_output(e.stderr)

        if stdout_text:
            print("[export_versions] stdout:")
            print(stdout_text)
        if stderr_text:
            print("[export_versions] stderr:")
            print(stderr_text)
        return None
    except Exception as e:
        print("[export_versions] pio pkg list failed:", e)
        return None


def _parse_pkg_list_text(text):
    """解析 `pio pkg list` 输出，分类为 platform / libraries / tools。"""
    result = {
        "platform": [],
        "libraries": [],
        "tools": []
    }

    seen = set()

    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line:
            continue

        line = re.sub(r"^[│├└─\s]+", "", line).strip()
        m = re.match(r"(.+?)\s+@\s+(.+)$", line)
        if not m:
            continue

        name = m.group(1).strip()
        version = m.group(2).strip()
        spec = f"{name}@{version}"

        if spec in seen:
            continue
        seen.add(spec)

        lower_name = name.lower()

        if "espressif32" in lower_name and (
            lower_name.startswith("platform")
            or lower_name.startswith("platformio/")
            or "platform " in lower_name
        ):
            result["platform"].append(spec)
            continue

        if (
            lower_name.startswith("platformio/tool")
            or lower_name.startswith("tool-")
            or lower_name.startswith("platformio/framework")
            or lower_name.startswith("framework-")
            or "toolchain-" in lower_name
        ):
            result["tools"].append(spec)
            continue

        result["libraries"].append(spec)

    return result


def _infer_psram_info(board_name, board_config):
    """根据板卡名称和配置推断 PSRAM 信息。"""
    board_name_low = (board_name or "").lower()

    if "no psram" in board_name_low:
        return "No PSRAM"

    if "psram" in board_name_low:
        return "PSRAM present (size not declared in board manifest)"

    psram_type = board_config.get("build.psram_type", "")
    if psram_type:
        return f"PSRAM configured: {psram_type}"

    extra_flags = board_config.get("build.extra_flags", [])
    if isinstance(extra_flags, list) and any("BOARD_HAS_PSRAM" in str(x) for x in extra_flags):
        return "PSRAM present"

    return "Unknown / not declared in board manifest"


def _get_physical_flash_size(env_, board_config):
    """获取物理 Flash 容量。"""
    project_options = _get_project_options_dict(env_)

    value = project_options.get("board_upload.flash_size")
    if value:
        parsed = _parse_capacity_to_bytes(value)
        if parsed is not None:
            return parsed

    value = board_config.get("upload.flash_size", "")
    if value:
        parsed = _parse_capacity_to_bytes(value)
        if parsed is not None:
            return parsed

    return board_config.get("upload.maximum_size", 0)


def _get_hardware_info(env_):
    """从 PlatformIO 板卡配置中读取硬件信息。"""
    board_config = env_.BoardConfig()

    board_id = env_.get("BOARD", "")
    board_name = board_config.get("name", "")
    mcu = board_config.get("build.mcu", "")
    f_cpu = board_config.get("build.f_cpu", "")
    physical_flash = _get_physical_flash_size(env_, board_config)
    max_ram = board_config.get("upload.maximum_ram_size", 0)

    return {
        "board": board_id,
        "board_name": board_name,
        "mcu": mcu,
        "cpu_freq": f_cpu,
        "flash_max": physical_flash,
        "ram_max": max_ram,
        "eeprom": "N/A (ESP32 uses flash/NVS, no dedicated hardware EEPROM)",
        "psram": _infer_psram_info(board_name, board_config),
    }


def _find_partition_file(env_):
    """查找当前使用的分区表文件。"""
    project_options = _get_project_options_dict(env_)
    board_config = env_.BoardConfig()

    partition_name = None
    for key in ("board_build.partitions", "build.partitions", "custom_partitions"):
        value = project_options.get(key)
        if value:
            partition_name = str(value).strip()
            break

    if not partition_name:
        value = board_config.get("build.partitions", "")
        if value:
            partition_name = str(value).strip()

    candidates = []

    if partition_name:
        p = Path(partition_name)

        if p.is_absolute():
            candidates.append(p)
        else:
            candidates.append(PROJECT_DIR / p)

        try:
            platform = env_.PioPlatform()
            framework_dir = platform.get_package_dir("framework-arduinoespressif32")
            if framework_dir:
                candidates.append(Path(framework_dir) / "tools" / "partitions" / partition_name)
        except Exception as e:
            print("[export_versions] resolve framework package dir failed:", e)

    for name in ("partitions.csv", "default.csv", "partitions/default.csv"):
        candidates.append(PROJECT_DIR / name)

    for p in candidates:
        if p and p.exists() and p.is_file():
            return p

    return None


def _parse_partition_csv(csv_path):
    """解析 ESP32 分区表 CSV 文件。"""
    partitions = []

    try:
        text = csv_path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        text = csv_path.read_text(encoding="utf-8-sig")

    for raw_line in text.splitlines():
        line = raw_line.strip()

        if not line:
            continue
        if line.startswith("#") or line.startswith(";"):
            continue

        parts = [x.strip() for x in line.split(",")]
        if len(parts) < 5:
            continue

        name = parts[0]
        ptype = parts[1]
        subtype = parts[2]
        offset = parts[3]
        size = parts[4]
        flags = parts[5] if len(parts) > 5 else ""

        size_int = _parse_size_to_int(size)
        offset_int = _parse_size_to_int(offset)

        partitions.append({
            "name": name,
            "type": ptype,
            "subtype": subtype,
            "offset": offset,
            "offset_int": offset_int,
            "size": size,
            "size_int": size_int,
            "flags": flags,
        })

    return partitions


def _get_partition_info(env_):
    """获取当前分区表文件路径及解析结果。"""
    partition_file = _find_partition_file(env_)
    if not partition_file:
        return {
            "file": None,
            "entries": [],
        }

    entries = _parse_partition_csv(partition_file)

    try:
        rel = str(partition_file.relative_to(PROJECT_DIR))
    except Exception:
        rel = str(partition_file)

    return {
        "file": rel,
        "entries": entries,
    }


def _get_max_app_partition_size(partition_info):
    """从分区表中找出最大的 app 分区大小。"""
    if not partition_info:
        return None

    max_size = None
    for entry in partition_info.get("entries", []):
        if str(entry.get("type", "")).strip().lower() != "app":
            continue

        size_int = entry.get("size_int")
        if size_int is None:
            continue

        if max_size is None or size_int > max_size:
            max_size = size_int

    return max_size


def _get_build_usage(env_, partition_info=None):
    """
    获取可靠的构建资源占用信息。

    当前策略：
    - Flash used：读取 firmware.bin 文件大小
    - Flash total：取分区表中最大的 app 分区
    - Flash percent：由上面两项计算
    - RAM：只保留总量，不导出 used / percent
    """
    result = {
        "flash_used": None,
        "flash_total": None,
        "flash_percent": None,
        "ram_total": None,
    }

    firmware_bin = PROJECT_DIR / ".pio" / "build" / env_["PIOENV"] / "firmware.bin"
    if firmware_bin.exists():
        try:
            result["flash_used"] = firmware_bin.stat().st_size
            print("[export_versions] flash_used read from firmware.bin")
        except Exception as e:
            print("[export_versions] failed to read firmware.bin size:", e)
    else:
        print("[export_versions] firmware.bin not found:", firmware_bin)

    app_max = _get_max_app_partition_size(partition_info)
    if app_max is not None:
        result["flash_total"] = app_max

    board_config = env_.BoardConfig()
    result["ram_total"] = board_config.get("upload.maximum_ram_size", None)

    if result["flash_total"] is None:
        result["flash_total"] = board_config.get("upload.maximum_size", None)

    if result["flash_used"] is not None and result["flash_total"]:
        result["flash_percent"] = result["flash_used"] * 100.0 / result["flash_total"]

    return result


def _get_build_meta():
    """获取本次构建的开始时间、结束时间和耗时。"""
    end_epoch = time.time()
    end_mono = time.perf_counter()

    start_dt = datetime.fromtimestamp(BUILD_START_EPOCH)
    end_dt = datetime.fromtimestamp(end_epoch)
    duration = end_mono - BUILD_START_MONO

    return {
        "start_time": start_dt.strftime("%Y-%m-%d %H:%M:%S"),
        "end_time": end_dt.strftime("%Y-%m-%d %H:%M:%S"),
        "duration_seconds": duration,
        "duration_text": _fmt_duration(duration),
    }


def _build_comment_block(info, hardware=None, usage=None, build_meta=None, partition_info=None):
    """生成最终写入 platformio.ini 的注释块文本。"""
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    lines = []
    lines.append("")
    lines.append("; ===== AUTO EXPORTED BUILD INFO BEGIN =====")
    lines.append(f"; env = {env['PIOENV']}")
    lines.append(f"; export_time = {now}")

    if build_meta:
        lines.append(";")
        lines.append("; build meta")
        lines.append(f"; build_start_time = {build_meta.get('start_time', '')}")
        lines.append(f"; build_end_time = {build_meta.get('end_time', '')}")
        lines.append(f"; build_duration = {build_meta.get('duration_text', '')}")

    if hardware:
        lines.append(";")
        lines.append("; hardware info")
        lines.append(f"; hardware_board = {hardware.get('board', '')}")
        lines.append(f"; hardware_board_name = {hardware.get('board_name', '')}")
        lines.append(f"; hardware_mcu = {hardware.get('mcu', '')}")
        lines.append(f"; hardware_cpu_freq = {hardware.get('cpu_freq', '')}")
        lines.append(f"; hardware_flash_max = {_fmt_bytes(hardware.get('flash_max', 0))}")
        lines.append(f"; hardware_ram_max = {_fmt_bytes(hardware.get('ram_max', 0))}")
        lines.append(f"; hardware_eeprom = {hardware.get('eeprom', '')}")
        lines.append(f"; hardware_psram = {hardware.get('psram', '')}")

    if usage:
        lines.append(";")
        lines.append("; build resource usage")

        if usage.get("flash_used") is not None:
            lines.append(f"; build_flash_used = {_fmt_bytes(usage['flash_used'])}")
        if usage.get("flash_total") is not None:
            lines.append(f"; build_flash_total = {_fmt_bytes(usage['flash_total'])}")
        if usage.get("flash_percent") is not None:
            lines.append(f"; build_flash_percent = {usage['flash_percent']:.1f}%")

        if usage.get("ram_total") is not None:
            lines.append(f"; build_ram_total = {_fmt_bytes(usage['ram_total'])}")

    if partition_info:
        lines.append(";")
        lines.append("; partition table")
        lines.append(f"; partition_file = {partition_info.get('file') or 'Not found'}")

        entries = partition_info.get("entries", [])
        if entries:
            for p in entries:
                size_text = p["size"]
                if p.get("size_int") is not None:
                    size_text = f"{size_text} / {_fmt_bytes(p['size_int'])}"

                offset_text = p["offset"]
                if p.get("offset_int") is not None:
                    offset_text = _fmt_hex(p["offset_int"])

                line = (
                    f"; partition = "
                    f"name:{p['name']}, "
                    f"type:{p['type']}, "
                    f"subtype:{p['subtype']}, "
                    f"offset:{offset_text}, "
                    f"size:{size_text}"
                )

                if p.get("flags"):
                    line += f", flags:{p['flags']}"

                lines.append(line)
        else:
            lines.append("; partition = No entries parsed")

    if info["platform"]:
        lines.append(";")
        lines.append("; resolved platform")
        for x in info["platform"]:
            lines.append(f"; platform_resolved = {x}")

    if info["libraries"]:
        lines.append(";")
        lines.append("; resolved lib_deps")
        for x in info["libraries"]:
            lines.append(f"; lib_dep_resolved = {x}")

    if info["tools"]:
        lines.append(";")
        lines.append("; resolved platform_packages")
        for x in info["tools"]:
            lines.append(f"; platform_package_resolved = {x}")

    lines.append("; ===== AUTO EXPORTED BUILD INFO END =====")
    lines.append("")
    return "\n".join(lines)


def _remove_old_block(text):
    """删除 platformio.ini 中旧的自动导出注释块。"""
    pattern = re.compile(
        r"\n?; ===== AUTO EXPORTED BUILD INFO BEGIN =====.*?; ===== AUTO EXPORTED BUILD INFO END =====\n?",
        re.S
    )
    return re.sub(pattern, "\n", text).rstrip() + "\n"


def export_versions_after_build(target=None, source=None, env=None):
    """在 firmware.bin 生成后导出构建信息到 platformio.ini。"""
    print("[export_versions] post action triggered")

    enabled = env.GetProjectOption("custom_export_build_info", "no").strip().lower()
    if enabled not in ("1", "yes", "true", "on"):
        print("[export_versions] skipped: custom_export_build_info is disabled")
        return 0

    text = _run_pio_pkg_list_text()
    if not text:
        print("[export_versions] skipped: no package data")
        return 0

    info = _parse_pkg_list_text(text)
    hardware = _get_hardware_info(env)
    partition_info = _get_partition_info(env)
    usage = _get_build_usage(env, partition_info)
    build_meta = _get_build_meta()

    comment_block = _build_comment_block(info, hardware, usage, build_meta, partition_info)

    old_text = INI_PATH.read_text(encoding="utf-8")
    new_text = _remove_old_block(old_text) + comment_block
    INI_PATH.write_text(new_text, encoding="utf-8")

    print(f"[export_versions] appended build info comments to {INI_PATH}")
    return 0


# 挂到 firmware.bin 生成后执行
global_env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", export_versions_after_build)