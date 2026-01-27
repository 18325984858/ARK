EXTERN g_MyHalpHvCounterQueryCounterAddr:QWORD
EXTERN g_OldHalpPerformanceCounter:QWORD



.CODE
;
;ETW HOOK 跳板函数
;
checkLogger PROC

correctLogger:
	push rcx
	mov rcx,rsp								;参数一:当前栈顶
	call g_MyHalpHvCounterQueryCounterAddr	;调用自己的HOOK函数
	pop rcx
exit:
	mov rax, g_OldHalpPerformanceCounter	;调用老的函数
	jmp rax

checkLogger ENDP

END