option casemap:none
.code

PUBLIC FPUSeedControlWord
FPUSeedControlWord PROC
    fnclex
    mov WORD PTR [rsp+8], cx
    fldcw WORD PTR [rsp+8]
    ret
FPUSeedControlWord ENDP

PUBLIC FPURoundToInt
FPURoundToInt PROC
    movsd QWORD PTR [rsp+8], xmm0
    fld QWORD PTR [rsp+8]
    fistp DWORD PTR [rsp+8]
    mov eax, DWORD PTR [rsp+8]
    ret
FPURoundToInt ENDP

PUBLIC FPUPrecisionDifference
FPUPrecisionDifference PROC
    movsd QWORD PTR [rsp+8], xmm0
    fld QWORD PTR [rsp+8]
    fld1
    faddp st(1), st(0)
    fsub QWORD PTR [rsp+8]
    fistp DWORD PTR [rsp+8]
    mov eax, DWORD PTR [rsp+8]
    ret
FPUPrecisionDifference ENDP
END
