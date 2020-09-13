@echo off
rem
rem build Release version
rem
cl.exe /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "STRICT" /D "UNICODE" /D "_UNICODE" /Fp"Release/websocket.pch" /YX /Fo"Release/" /Fd"Release/" /FD /c  base64.c iocpserver.c sha1.c wsserver.c main.c
lib /nologo  /OUT:Release/websocket-mt.lib Release/base64.obj Release/iocpserver.obj Release/sha1.obj Release/wsserver.obj
link /nologo /Out:Release/websocket.exe Release/main.obj Release/websocket-mt.lib ws2_32.lib ../libs/jansson/src/jansson-mt.lib

rem
rem build Debug version
rem
cl.exe /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "STRICT" /D "UNICODE" /D "_UNICODE" /FR"Debug/" /Fp"Debug/websocket.pch" /YX /Fo"Debug/" /Fd"Debug/" /FD /GZ /c base64.c iocpserver.c sha1.c wsserver.c main.c
lib /nologo  /OUT:Debug/websocket-mtd.lib Debug/base64.obj Debug/iocpserver.obj Debug/sha1.obj Debug/wsserver.obj
link /nologo /Out:Debug/websocket.exe Debug/main.obj Debug/websocket-mtd.lib ws2_32.lib ../libs/jansson/src/jansson-mtd.lib
