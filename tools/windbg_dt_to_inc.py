"""Parse the WinDbg batch dt output (saved as JSON by the MCP tool) where
struct names are NOT in the output. We rely on the known order of dt commands
issued, separated by ==== markers."""
import json
import re
import sys
from pathlib import Path

STRUCT_ORDER = [
    "_EPROCESS",
    "_CLIENT_ID",
    "_CONTROL_AREA",
    "_DEVICE_OBJECT",
    "_DRIVER_EXTENSION",
    "_DRIVER_OBJECT",
    "_ENODE",
    "_EPARTITION",
    "_ETHREAD",
    "_ETW_SILODRIVERSTATE",
    "_EX_PARTITION",
    "_EX_WORK_QUEUE",
    "_FILE_OBJECT",
    "_HANDLE_TABLE",
    "_KAPC_STATE",
    "_KPCR",
    "_KPRCB",
    "_KPROCESS",
    "_KTHREAD",
    "_LDR_DATA_TABLE_ENTRY",
    "_MMSUPPORT_FULL",
    "_MMSUPPORT_SHARED",
    "_MMVAD",
    "_MMVAD_SHORT",
    "_OBJECT_HEADER",
    "_OBJECT_SYMBOLIC_LINK",
    "_OBJECT_TYPE",
    "_OBJECT_TYPE_INITIALIZER",
    "_PEB",
    "_PEB_LDR_DATA",
    "_RTL_USER_PROCESS_PARAMETERS",
    "_SEP_LOGON_SESSION_REFERENCES",
    "_SUBSECTION",
    "_TOKEN",
    "_WMI_LOGGER_CONTEXT",
]

WANTED = [
    ("_CLIENT_ID", "UniqueProcess"),
    ("_CLIENT_ID", "UniqueThread"),
    ("_CONTROL_AREA", "FilePointer"),
    ("_DEVICE_OBJECT", "Queue"),
    ("_DRIVER_EXTENSION", "DriverObject"),
    ("_DRIVER_EXTENSION", "ServiceKeyName"),
    ("_DRIVER_OBJECT", "DriverExtension"),
    ("_DRIVER_OBJECT", "DriverName"),
    ("_DRIVER_OBJECT", "DriverSection"),
    ("_DRIVER_OBJECT", "DriverSize"),
    ("_DRIVER_OBJECT", "DriverStart"),
    ("_DRIVER_OBJECT", "FastIoDispatch"),
    ("_DRIVER_OBJECT", "MajorFunction"),
    ("_ENODE", "HotAddProcessorWorkItem"),
    ("_ENODE", "Ncb"),
    ("_EPARTITION", "ExPartition"),
    ("_EPROCESS", "ActiveProcessLinks"),
    ("_EPROCESS", "ActiveThreads"),
    ("_EPROCESS", "CreateTime"),
    ("_EPROCESS", "DebugPort"),
    ("_EPROCESS", "Flags3"),
    ("_EPROCESS", "ImageFileName"),
    ("_EPROCESS", "ImageFilePointer"),
    ("_EPROCESS", "InheritedFromUniqueProcessId"),
    ("_EPROCESS", "ObjectTable"),
    ("_EPROCESS", "Peb"),
    ("_EPROCESS", "Protection"),
    ("_EPROCESS", "SeAuditProcessCreationInfo"),
    ("_EPROCESS", "Session"),
    ("_EPROCESS", "ThreadListHead"),
    ("_EPROCESS", "Token"),
    ("_EPROCESS", "UniqueProcessId"),
    ("_EPROCESS", "VadCount"),
    ("_EPROCESS", "VadRoot"),
    ("_EPROCESS", "Vm"),
    ("_EPROCESS", "WoW64Process"),
    ("_ETHREAD", "Cid"),
    ("_ETHREAD", "CreateTime"),
    ("_ETHREAD", "StartAddress"),
    ("_ETHREAD", "ThreadListEntry"),
    ("_ETHREAD", "Win32StartAddress"),
    ("_ETW_SILODRIVERSTATE", "EtwpLoggerContext"),
    ("_EX_PARTITION", "WorkQueues"),
    ("_EX_WORK_QUEUE", "WorkPriQueue"),
    ("_FILE_OBJECT", "DeviceObject"),
    ("_FILE_OBJECT", "FileName"),
    ("_HANDLE_TABLE", "NextHandleNeedingPool"),
    ("_HANDLE_TABLE", "TableCode"),
    ("_KAPC_STATE", "Process"),
    ("_KPCR", "GdtBase"),
    ("_KPCR", "IdtBase"),
    ("_KPRCB", "CurrentThread"),
    ("_KPRCB", "RspBase"),
    ("_KPRCB", "TimerTable"),
    ("_KPROCESS", "AddressPolicy"),
    ("_KTHREAD", "ApcState"),
    ("_KTHREAD", "ContextSwitches"),
    ("_KTHREAD", "PreviousMode"),
    ("_KTHREAD", "Priority"),
    ("_KTHREAD", "Process"),
    ("_KTHREAD", "State"),
    ("_KTHREAD", "SystemCallNumber"),
    ("_KTHREAD", "Teb"),
    ("_KTHREAD", "ThreadFlags"),
    ("_LDR_DATA_TABLE_ENTRY", "BaseDllName"),
    ("_LDR_DATA_TABLE_ENTRY", "DllBase"),
    ("_LDR_DATA_TABLE_ENTRY", "FullDllName"),
    ("_LDR_DATA_TABLE_ENTRY", "SizeOfImage"),
    ("_MMSUPPORT_FULL", "Shared"),
    ("_MMSUPPORT_SHARED", "ShadowMapping"),
    ("_MMVAD", "Core"),
    ("_MMVAD", "Subsection"),
    ("_MMVAD_SHORT", "EndingVpn"),
    ("_MMVAD_SHORT", "EndingVpnHigh"),
    ("_MMVAD_SHORT", "StartingVpn"),
    ("_MMVAD_SHORT", "StartingVpnHigh"),
    ("_MMVAD_SHORT", "u"),
    ("_OBJECT_HEADER", "InfoMask"),
    ("_OBJECT_HEADER", "PointerCount"),
    ("_OBJECT_HEADER", "TypeIndex"),
    ("_OBJECT_SYMBOLIC_LINK", "LinkTarget"),
    ("_OBJECT_TYPE", "CallbackList"),
    ("_OBJECT_TYPE", "Index"),
    ("_OBJECT_TYPE", "Name"),
    ("_OBJECT_TYPE", "TypeInfo"),
    ("_OBJECT_TYPE_INITIALIZER", "CloseProcedure"),
    ("_OBJECT_TYPE_INITIALIZER", "DeleteProcedure"),
    ("_OBJECT_TYPE_INITIALIZER", "DumpProcedure"),
    ("_OBJECT_TYPE_INITIALIZER", "OkayToCloseProcedure"),
    ("_OBJECT_TYPE_INITIALIZER", "OpenProcedure"),
    ("_OBJECT_TYPE_INITIALIZER", "ParseProcedure"),
    ("_OBJECT_TYPE_INITIALIZER", "QueryNameProcedure"),
    ("_OBJECT_TYPE_INITIALIZER", "SecurityProcedure"),
    ("_OBJECT_TYPE_INITIALIZER", "ValidAccessMask"),
    ("_PEB", "BeingDebugged"),
    ("_PEB", "ImageBaseAddress"),
    ("_PEB", "Ldr"),
    ("_PEB", "ProcessParameters"),
    ("_PEB_LDR_DATA", "InLoadOrderModuleList"),
    ("_RTL_USER_PROCESS_PARAMETERS", "CommandLine"),
    ("_SEP_LOGON_SESSION_REFERENCES", "AccountName"),
    ("_SUBSECTION", "ControlArea"),
    ("_TOKEN", "LogonSession"),
    ("_WMI_LOGGER_CONTEXT", "GetCpuClock"),
]

LINE_RE = re.compile(r"^\s*\+0x([0-9a-fA-F]+)\s+(\w+)\s*:")

def main():
    json_path = Path(sys.argv[1])
    obj = json.loads(json_path.read_text(encoding="utf-8"))
    text = obj["result"]
    lines = text.split("\n")
    while lines and (lines[0].strip().startswith("```") or not lines[0].strip()):
        lines.pop(0)
    while lines and (lines[-1].strip().startswith("```") or not lines[-1].strip()):
        lines.pop()

    blocks = []
    cur = []
    for line in lines:
        if line.strip() == "====":
            blocks.append(cur)
            cur = []
        else:
            cur.append(line)
    blocks.append(cur)

    if len(blocks) != len(STRUCT_ORDER):
        print(f"// WARN block count {len(blocks)} != struct order {len(STRUCT_ORDER)}",
              file=sys.stderr)

    struct_map = {}
    for name, block in zip(STRUCT_ORDER, blocks):
        fields = {}
        for line in block:
            m = LINE_RE.match(line)
            if m:
                off = int(m.group(1), 16)
                fname = m.group(2)
                if fname not in fields:
                    fields[fname] = off
        struct_map[name] = fields

    found, missing = [], []
    for s, m in WANTED:
        if s in struct_map and m in struct_map[s]:
            found.append((s, m, struct_map[s][m]))
        else:
            missing.append((s, m))

    print(f"// Resolved {len(found)}/{len(WANTED)} via WinDbg dt output")
    for s, m, off in found:
        print(f'    {{ L"{s}", L"{m}", 0x{off:X} }},')
    print()
    print(f"// MISSING ({len(missing)}):")
    for s, m in missing:
        print(f"//   {s}.{m}")

if __name__ == "__main__":
    sys.exit(main())
