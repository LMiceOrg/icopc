MessageIdTypedef=DWORD

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
    Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
    Warning=0x2:STATUS_SEVERITY_WARNING
    Error=0x3:STATUS_SEVERITY_ERROR
)


FacilityNames=(System=0x0:FACILITY_SYSTEM
    Runtime=0x2:FACILITY_RUNTIME
    Stubs=0x3:FACILITY_STUBS
    Io=0x4:FACILITY_IO_ERROR_CODE
)

LanguageNames=(Chinese=0x0804:MSG0804)

; // The following are message definitions.

MessageId=1 
Severity=Error
Facility=Runtime
SymbolicName=SVC_UNKNOWN_ERROR
Language=Chinese
发生一个未知错误 (%1).
.

MessageId=2
Severity=Error
Facility=Runtime
SymbolicName=SVC_RUNTIME_ERROR
Language=Chinese
(%1)运行时错误 (%2).
.

MessageId=3
Severity=Informational
Facility=Runtime
SymbolicName=SVC_RUNTIME_INFO
Language=Chinese
(%1)运行时信息 (%2).
.

; // A message file must end with a period on its own line
; // followed by a blank line.

