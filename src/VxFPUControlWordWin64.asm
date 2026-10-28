option casemap:none
.code

PUBLIC ?VxGetFPUControlWord@@YAGXZ
?VxGetFPUControlWord@@YAGXZ PROC
    fstcw WORD PTR [rsp+8]
    movzx eax, WORD PTR [rsp+8]
    ret
?VxGetFPUControlWord@@YAGXZ ENDP

PUBLIC ?VxSetFPUControlWord@@YAXG@Z
?VxSetFPUControlWord@@YAXG@Z PROC
    mov WORD PTR [rsp+8], cx
    fldcw WORD PTR [rsp+8]
    ret
?VxSetFPUControlWord@@YAXG@Z ENDP

PUBLIC ?VxSetBaseFPUControlWord@@YAXXZ
?VxSetBaseFPUControlWord@@YAXXZ PROC
    fstcw WORD PTR [rsp+8]
    mov ax, WORD PTR [rsp+8]
    and ax, 0F0FFh
    or ax, 003Fh
    cmp ax, WORD PTR [rsp+8]
    je baseReady
    mov WORD PTR [rsp+8], ax
    fldcw WORD PTR [rsp+8]
baseReady:
    ret
?VxSetBaseFPUControlWord@@YAXXZ ENDP
END
